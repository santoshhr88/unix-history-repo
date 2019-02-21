# $FreeBSD$

GTESTS_CXXFLAGS+= -DGTESTS_HAS_RTTI=1
GTESTS_CXXFLAGS+= -DGTESTS_HAS_POSIX_RE=1
GTESTS_CXXFLAGS+= -DGTESTS_HAS_PTHREAD=1
GTESTS_CXXFLAGS+= -DGTESTS_HAS_STREAM_REDIRECTION=1
GTESTS_CXXFLAGS+= -DGTESTS_LANG_CXX11=1
GTESTS_CXXFLAGS+= -frtti
GTESTS_CXXFLAGS+= -std=c++11

# XXX: src.libnames.mk should handle adding this directory.
GTESTS_CXXFLAGS+= -I${DESTDIR}${INCLUDEDIR}/private

NO_WTHREAD_SAFETY=
