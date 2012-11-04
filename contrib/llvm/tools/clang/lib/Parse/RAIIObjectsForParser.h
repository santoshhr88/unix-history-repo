//===--- RAIIObjectsForParser.h - RAII helpers for the parser ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines and implements the some simple RAII objects that are used
// by the parser to manage bits in recursion.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PARSE_RAII_OBJECTS_FOR_PARSER_H
#define LLVM_CLANG_PARSE_RAII_OBJECTS_FOR_PARSER_H

#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Sema/DelayedDiagnostic.h"
#include "clang/Sema/Sema.h"

namespace clang {
  // TODO: move ParsingClassDefinition here.
  // TODO: move TentativeParsingAction here.

  /// \brief A RAII object used to temporarily suppress access-like
  /// checking.  Access-like checks are those associated with
  /// controlling the use of a declaration, like C++ access control
  /// errors and deprecation warnings.  They are contextually
  /// dependent, in that they can only be resolved with full
  /// information about what's being declared.  They are also
  /// suppressed in certain contexts, like the template arguments of
  /// an explicit instantiation.  However, those suppression contexts
  /// cannot necessarily be fully determined in advance;  for
  /// example, something starting like this:
  ///   template <> class std::vector<A::PrivateType>
  /// might be the entirety of an explicit instantiation:
  ///   template <> class std::vector<A::PrivateType>;
  /// or just an elaborated type specifier:
  ///   template <> class std::vector<A::PrivateType> make_vector<>();
  /// Therefore this class collects all the diagnostics and permits
  /// them to be re-delayed in a new context.
  class SuppressAccessChecks {
    Sema &S;
    sema::DelayedDiagnosticPool DiagnosticPool;
    Sema::ParsingDeclState State;
    bool Active;

  public:
    /// Begin suppressing access-like checks 
    SuppressAccessChecks(Parser &P, bool activate = true)
        : S(P.getActions()), DiagnosticPool(NULL) {
      if (activate) {
        State = S.PushParsingDeclaration(DiagnosticPool);
        Active = true;
      } else {
        Active = false;
      }
    }

    void done() {
      assert(Active && "trying to end an inactive suppression");
      S.PopParsingDeclaration(State, NULL);
      Active = false;
    }

    void redelay() {
      assert(!Active && "redelaying without having ended first");
      if (!DiagnosticPool.pool_empty())
        S.redelayDiagnostics(DiagnosticPool);
      assert(DiagnosticPool.pool_empty());
    }

    ~SuppressAccessChecks() {
      if (Active) done();
    }
  };

  /// \brief RAII object used to inform the actions that we're
  /// currently parsing a declaration.  This is active when parsing a
  /// variable's initializer, but not when parsing the body of a
  /// class or function definition.
  class ParsingDeclRAIIObject {
    Sema &Actions;
    sema::DelayedDiagnosticPool DiagnosticPool;
    Sema::ParsingDeclState State;
    bool Popped;

    // Do not implement.
    ParsingDeclRAIIObject(const ParsingDeclRAIIObject &other);
    ParsingDeclRAIIObject &operator=(const ParsingDeclRAIIObject &other);

  public:
    enum NoParent_t { NoParent };
    ParsingDeclRAIIObject(Parser &P, NoParent_t _)
        : Actions(P.getActions()), DiagnosticPool(NULL) {
      push();
    }

    /// Creates a RAII object whose pool is optionally parented by another.
    ParsingDeclRAIIObject(Parser &P,
                          const sema::DelayedDiagnosticPool *parentPool)
        : Actions(P.getActions()), DiagnosticPool(parentPool) {
      push();
    }

    /// Creates a RAII object and, optionally, initialize its
    /// diagnostics pool by stealing the diagnostics from another
    /// RAII object (which is assumed to be the current top pool).
    ParsingDeclRAIIObject(Parser &P, ParsingDeclRAIIObject *other)
        : Actions(P.getActions()),
          DiagnosticPool(other ? other->DiagnosticPool.getParent() : NULL) {
      if (other) {
        DiagnosticPool.steal(other->DiagnosticPool);
        other->abort();
      }
      push();
    }

    ~ParsingDeclRAIIObject() {
      abort();
    }

    sema::DelayedDiagnosticPool &getDelayedDiagnosticPool() {
      return DiagnosticPool;
    }
    const sema::DelayedDiagnosticPool &getDelayedDiagnosticPool() const {
      return DiagnosticPool;
    }

    /// Resets the RAII object for a new declaration.
    void reset() {
      abort();
      push();
    }

    /// Signals that the context was completed without an appropriate
    /// declaration being parsed.
    void abort() {
      pop(0);
    }

    void complete(Decl *D) {
      assert(!Popped && "ParsingDeclaration has already been popped!");
      pop(D);
    }

    /// Unregister this object from Sema, but remember all the
    /// diagnostics that were emitted into it.
    void abortAndRemember() {
      pop(0);
    }

  private:
    void push() {
      State = Actions.PushParsingDeclaration(DiagnosticPool);
      Popped = false;
    }

    void pop(Decl *D) {
      if (!Popped) {
        Actions.PopParsingDeclaration(State, D);
        Popped = true;
      }
    }
  };

  /// A class for parsing a DeclSpec.
  class ParsingDeclSpec : public DeclSpec {
    ParsingDeclRAIIObject ParsingRAII;

  public:
    ParsingDeclSpec(Parser &P)
      : DeclSpec(P.getAttrFactory()),
        ParsingRAII(P, ParsingDeclRAIIObject::NoParent) {}
    ParsingDeclSpec(Parser &P, ParsingDeclRAIIObject *RAII)
      : DeclSpec(P.getAttrFactory()),
        ParsingRAII(P, RAII) {}

    const sema::DelayedDiagnosticPool &getDelayedDiagnosticPool() const {
      return ParsingRAII.getDelayedDiagnosticPool();
    }

    void complete(Decl *D) {
      ParsingRAII.complete(D);
    }

    void abort() {
      ParsingRAII.abort();
    }
  };

  /// A class for parsing a declarator.
  class ParsingDeclarator : public Declarator {
    ParsingDeclRAIIObject ParsingRAII;

  public:
    ParsingDeclarator(Parser &P, const ParsingDeclSpec &DS, TheContext C)
      : Declarator(DS, C), ParsingRAII(P, &DS.getDelayedDiagnosticPool()) {
    }

    const ParsingDeclSpec &getDeclSpec() const {
      return static_cast<const ParsingDeclSpec&>(Declarator::getDeclSpec());
    }

    ParsingDeclSpec &getMutableDeclSpec() const {
      return const_cast<ParsingDeclSpec&>(getDeclSpec());
    }

    void clear() {
      Declarator::clear();
      ParsingRAII.reset();
    }

    void complete(Decl *D) {
      ParsingRAII.complete(D);
    }
  };

  /// A class for parsing a field declarator.
  class ParsingFieldDeclarator : public FieldDeclarator {
    ParsingDeclRAIIObject ParsingRAII;

  public:
    ParsingFieldDeclarator(Parser &P, const ParsingDeclSpec &DS)
      : FieldDeclarator(DS), ParsingRAII(P, &DS.getDelayedDiagnosticPool()) {
    }

    const ParsingDeclSpec &getDeclSpec() const {
      return static_cast<const ParsingDeclSpec&>(D.getDeclSpec());
    }

    ParsingDeclSpec &getMutableDeclSpec() const {
      return const_cast<ParsingDeclSpec&>(getDeclSpec());
    }

    void complete(Decl *D) {
      ParsingRAII.complete(D);
    }
  };

  /// ExtensionRAIIObject - This saves the state of extension warnings when
  /// constructed and disables them.  When destructed, it restores them back to
  /// the way they used to be.  This is used to handle __extension__ in the
  /// parser.
  class ExtensionRAIIObject {
    void operator=(const ExtensionRAIIObject &);     // DO NOT IMPLEMENT
    ExtensionRAIIObject(const ExtensionRAIIObject&); // DO NOT IMPLEMENT
    DiagnosticsEngine &Diags;
  public:
    ExtensionRAIIObject(DiagnosticsEngine &diags) : Diags(diags) {
      Diags.IncrementAllExtensionsSilenced();
    }

    ~ExtensionRAIIObject() {
      Diags.DecrementAllExtensionsSilenced();
    }
  };
  
  /// ColonProtectionRAIIObject - This sets the Parser::ColonIsSacred bool and
  /// restores it when destroyed.  This says that "foo:" should not be
  /// considered a possible typo for "foo::" for error recovery purposes.
  class ColonProtectionRAIIObject {
    Parser &P;
    bool OldVal;
  public:
    ColonProtectionRAIIObject(Parser &p, bool Value = true)
      : P(p), OldVal(P.ColonIsSacred) {
      P.ColonIsSacred = Value;
    }
    
    /// restore - This can be used to restore the state early, before the dtor
    /// is run.
    void restore() {
      P.ColonIsSacred = OldVal;
    }
    
    ~ColonProtectionRAIIObject() {
      restore();
    }
  };
  
  /// \brief RAII object that makes '>' behave either as an operator
  /// or as the closing angle bracket for a template argument list.
  class GreaterThanIsOperatorScope {
    bool &GreaterThanIsOperator;
    bool OldGreaterThanIsOperator;
  public:
    GreaterThanIsOperatorScope(bool &GTIO, bool Val)
    : GreaterThanIsOperator(GTIO), OldGreaterThanIsOperator(GTIO) {
      GreaterThanIsOperator = Val;
    }
    
    ~GreaterThanIsOperatorScope() {
      GreaterThanIsOperator = OldGreaterThanIsOperator;
    }
  };
  
  class InMessageExpressionRAIIObject {
    bool &InMessageExpression;
    bool OldValue;
    
  public:
    InMessageExpressionRAIIObject(Parser &P, bool Value)
      : InMessageExpression(P.InMessageExpression), 
        OldValue(P.InMessageExpression) {
      InMessageExpression = Value;
    }
    
    ~InMessageExpressionRAIIObject() {
      InMessageExpression = OldValue;
    }
  };
  
  /// \brief RAII object that makes sure paren/bracket/brace count is correct
  /// after declaration/statement parsing, even when there's a parsing error.
  class ParenBraceBracketBalancer {
    Parser &P;
    unsigned short ParenCount, BracketCount, BraceCount;
  public:
    ParenBraceBracketBalancer(Parser &p)
      : P(p), ParenCount(p.ParenCount), BracketCount(p.BracketCount),
        BraceCount(p.BraceCount) { }
    
    ~ParenBraceBracketBalancer() {
      P.ParenCount = ParenCount;
      P.BracketCount = BracketCount;
      P.BraceCount = BraceCount;
    }
  };

  class PoisonSEHIdentifiersRAIIObject {
    PoisonIdentifierRAIIObject Ident_AbnormalTermination;
    PoisonIdentifierRAIIObject Ident_GetExceptionCode;
    PoisonIdentifierRAIIObject Ident_GetExceptionInfo;
    PoisonIdentifierRAIIObject Ident__abnormal_termination;
    PoisonIdentifierRAIIObject Ident__exception_code;
    PoisonIdentifierRAIIObject Ident__exception_info;
    PoisonIdentifierRAIIObject Ident___abnormal_termination;
    PoisonIdentifierRAIIObject Ident___exception_code;
    PoisonIdentifierRAIIObject Ident___exception_info;
  public:
    PoisonSEHIdentifiersRAIIObject(Parser &Self, bool NewValue)
      : Ident_AbnormalTermination(Self.Ident_AbnormalTermination, NewValue),
        Ident_GetExceptionCode(Self.Ident_GetExceptionCode, NewValue),
        Ident_GetExceptionInfo(Self.Ident_GetExceptionInfo, NewValue),
        Ident__abnormal_termination(Self.Ident__abnormal_termination, NewValue),
        Ident__exception_code(Self.Ident__exception_code, NewValue),
        Ident__exception_info(Self.Ident__exception_info, NewValue),
        Ident___abnormal_termination(Self.Ident___abnormal_termination, NewValue),
        Ident___exception_code(Self.Ident___exception_code, NewValue),
        Ident___exception_info(Self.Ident___exception_info, NewValue) {
    }
  };

  /// \brief RAII class that helps handle the parsing of an open/close delimiter
  /// pair, such as braces { ... } or parentheses ( ... ).
  class BalancedDelimiterTracker : public GreaterThanIsOperatorScope {
    Parser& P;
    tok::TokenKind Kind, Close;
    SourceLocation (Parser::*Consumer)();
    SourceLocation LOpen, LClose;
    
    unsigned short &getDepth() {
      switch (Kind) {
        case tok::l_brace: return P.BraceCount;
        case tok::l_square: return P.BracketCount;
        case tok::l_paren: return P.ParenCount;
        default: llvm_unreachable("Wrong token kind");
      }
    }
    
    enum { MaxDepth = 256 };
    
    bool diagnoseOverflow();
    bool diagnoseMissingClose();
    
  public:
    BalancedDelimiterTracker(Parser& p, tok::TokenKind k)
      : GreaterThanIsOperatorScope(p.GreaterThanIsOperator, true),
        P(p), Kind(k)
    {
      switch (Kind) {
        default: llvm_unreachable("Unexpected balanced token");
        case tok::l_brace:
          Close = tok::r_brace; 
          Consumer = &Parser::ConsumeBrace;
          break;
        case tok::l_paren:
          Close = tok::r_paren; 
          Consumer = &Parser::ConsumeParen;
          break;
          
        case tok::l_square:
          Close = tok::r_square; 
          Consumer = &Parser::ConsumeBracket;
          break;
      }      
    }
    
    SourceLocation getOpenLocation() const { return LOpen; }
    SourceLocation getCloseLocation() const { return LClose; }
    SourceRange getRange() const { return SourceRange(LOpen, LClose); }
    
    bool consumeOpen() {
      if (!P.Tok.is(Kind))
        return true;
      
      if (getDepth() < MaxDepth) {
        LOpen = (P.*Consumer)();
        return false;
      }
      
      return diagnoseOverflow();
    }
    
    bool expectAndConsume(unsigned DiagID,
                          const char *Msg = "",
                          tok::TokenKind SkipToTok = tok::unknown);
    bool consumeClose() {
      if (P.Tok.is(Close)) {
        LClose = (P.*Consumer)();
        return false;
      } 
      
      return diagnoseMissingClose();
    }
    void skipToEnd();
  };

} // end namespace clang

#endif
