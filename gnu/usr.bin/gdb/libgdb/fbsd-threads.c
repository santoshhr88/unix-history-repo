/* $FreeBSD$ */
/* FreeBSD libthread_db assisted debugging support.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#include "proc_service.h"
#include "thread_db.h"

#include "defs.h"
#include "bfd.h"
#include "elf-bfd.h"
#include "gdb_assert.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "inferior.h"
#include "objfiles.h"
#include "regcache.h"
#include "symfile.h"
#include "symtab.h"
#include "target.h"
#include "gdbcmd.h"
#include "solib-svr4.h"


#define LIBTHREAD_DB_SO "libthread_db.so"

struct ps_prochandle
{
  pid_t pid;
};

extern int child_suppress_run;

extern struct target_ops child_ops;

/* This module's target vectors.  */
static struct target_ops fbsd_thread_ops;
static struct target_ops fbsd_core_ops;

/* Saved copy of orignal core_ops. */
static struct target_ops orig_core_ops;
extern struct target_ops core_ops;

/* Pointer to the next function on the objfile event chain.  */
static void (*target_new_objfile_chain) (struct objfile *objfile);

/* Non-zero if there is a thread module */
static int fbsd_thread_present;

/* Non-zero if we're using this module's target vector.  */
static int fbsd_thread_active;

/* Non-zero if core_open is called */
static int fbsd_thread_core = 0;

/* Non-zero if we have to keep this module's target vector active
   across re-runs.  */
static int keep_thread_db;

/* Structure that identifies the child process for the
   <proc_service.h> interface.  */
static struct ps_prochandle proc_handle;

/* Connection to the libthread_db library.  */
static td_thragent_t *thread_agent;

/* The last thread we are single stepping */
static ptid_t last_single_step_thread;

/* Pointers to the libthread_db functions.  */

static td_err_e (*td_init_p) (void);

static td_err_e (*td_ta_new_p) (struct ps_prochandle *ps, td_thragent_t **ta);
static td_err_e (*td_ta_delete_p) (td_thragent_t *);
static td_err_e (*td_ta_map_id2thr_p) (const td_thragent_t *ta, thread_t pt,
				       td_thrhandle_t *__th);
static td_err_e (*td_ta_map_lwp2thr_p) (const td_thragent_t *ta, lwpid_t lwpid,
					td_thrhandle_t *th);
static td_err_e (*td_ta_thr_iter_p) (const td_thragent_t *ta,
				     td_thr_iter_f *callback,
				     void *cbdata_p, td_thr_state_e state,
				     int ti_pri, sigset_t *ti_sigmask_p,
				     unsigned int ti_user_flags);
static td_err_e (*td_ta_event_addr_p) (const td_thragent_t *ta,
				       td_event_e event, td_notify_t *ptr);
static td_err_e (*td_ta_set_event_p) (const td_thragent_t *ta,
				      td_thr_events_t *event);
static td_err_e (*td_ta_event_getmsg_p) (const td_thragent_t *ta,
					 td_event_msg_t *msg);
static td_err_e (*td_thr_get_info_p) (const td_thrhandle_t *th,
				      td_thrinfo_t *infop);
static td_err_e (*td_thr_getfpregs_p) (const td_thrhandle_t *th,
				       prfpregset_t *regset);
static td_err_e (*td_thr_getgregs_p) (const td_thrhandle_t *th,
				      prgregset_t gregs);
static td_err_e (*td_thr_setfpregs_p) (const td_thrhandle_t *th,
				       const prfpregset_t *fpregs);
static td_err_e (*td_thr_setgregs_p) (const td_thrhandle_t *th,
				      prgregset_t gregs);
static td_err_e (*td_thr_event_enable_p) (const td_thrhandle_t *th, int event);

static td_err_e (*td_thr_sstep_p) (td_thrhandle_t *th, int step);

static td_err_e (*td_ta_tsd_iter_p) (const td_thragent_t *ta,
				 td_key_iter_f *func, void *data);
static td_err_e (*td_thr_tls_get_addr_p) (const td_thrhandle_t *th,
                                          void *map_address,
                                          size_t offset, void **address);
static td_err_e (*td_thr_dbsuspend_p) (const td_thrhandle_t *);
static td_err_e (*td_thr_dbresume_p) (const td_thrhandle_t *);

/* Prototypes for local functions.  */
static void fbsd_thread_find_new_threads (void);
static int fbsd_thread_alive (ptid_t ptid);

/* Building process ids.  */

#define GET_PID(ptid)		ptid_get_pid (ptid)
#define GET_LWP(ptid)		ptid_get_lwp (ptid)
#define GET_THREAD(ptid)	ptid_get_tid (ptid)

#define IS_LWP(ptid)		(GET_LWP (ptid) != 0)
#define IS_THREAD(ptid)		(GET_THREAD (ptid) != 0)

#define BUILD_LWP(lwp, pid)	ptid_build (pid, lwp, 0)
#define BUILD_THREAD(tid, pid)	ptid_build (pid, 0, tid)

static char *
thread_db_err_str (td_err_e err)
{
  static char buf[64];

  switch (err)
    {
    case TD_OK:
      return "generic 'call succeeded'";
    case TD_ERR:
      return "generic error";
    case TD_NOTHR:
      return "no thread to satisfy query";
    case TD_NOSV:
      return "no sync handle to satisfy query";
    case TD_NOLWP:
      return "no LWP to satisfy query";
    case TD_BADPH:
      return "invalid process handle";
    case TD_BADTH:
      return "invalid thread handle";
    case TD_BADSH:
      return "invalid synchronization handle";
    case TD_BADTA:
      return "invalid thread agent";
    case TD_BADKEY:
      return "invalid key";
    case TD_NOMSG:
      return "no event message for getmsg";
    case TD_NOFPREGS:
      return "FPU register set not available";
    case TD_NOLIBTHREAD:
      return "application not linked with libthread";
    case TD_NOEVENT:
      return "requested event is not supported";
    case TD_NOCAPAB:
      return "capability not available";
    case TD_DBERR:
      return "debugger service failed";
    case TD_NOAPLIC:
      return "operation not applicable to";
    case TD_NOTSD:
      return "no thread-specific data for this thread";
    case TD_MALLOC:
      return "malloc failed";
    case TD_PARTIALREG:
      return "only part of register set was written/read";
    case TD_NOXREGS:
      return "X register set not available for this thread";
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db error '%d'", err);
      return buf;
    }
}

static char *
thread_db_state_str (td_thr_state_e state)
{
  static char buf[64];

  switch (state)
    {
    case TD_THR_STOPPED:
      return "stopped by debugger";
    case TD_THR_RUN:
      return "runnable";
    case TD_THR_ACTIVE:
      return "active";
    case TD_THR_ZOMBIE:
      return "zombie";
    case TD_THR_SLEEP:
      return "sleeping";
    case TD_THR_STOPPED_ASLEEP:
      return "stopped by debugger AND blocked";
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db state %d", state);
      return buf;
    }
}

/* Convert LWP to user-level thread id. */
static ptid_t
thread_from_lwp (ptid_t ptid)
{
  td_thrinfo_t ti;
  td_thrhandle_t th;
  td_err_e err;
 
  gdb_assert (IS_LWP (ptid));

  if (fbsd_thread_active)
    {
      err = td_ta_map_lwp2thr_p (thread_agent, GET_LWP (ptid), &th);
      if (err == TD_OK)
        {
          err = td_thr_get_info_p (&th, &ti);
          if (err != TD_OK)
            error ("Cannot get thread info: %s", thread_db_err_str (err));
          return BUILD_THREAD (ti.ti_tid, GET_PID (ptid));
        }
    }

  /* the LWP is not mapped to user thread */  
  return BUILD_LWP (GET_LWP (ptid), GET_PID (ptid));
}

static void
fbsd_core_get_first_lwp (bfd *abfd, asection *asect, void *obj)
{
  if (strncmp (bfd_section_name (abfd, asect), ".reg/", 5) != 0)
    return;

  if (*(lwpid_t *)obj != 0)
    return;

  *(lwpid_t *)obj = atoi (bfd_section_name (abfd, asect) + 5);
}

static long
get_current_lwp (int pid)
{
  struct ptrace_lwpinfo pl;
  lwpid_t lwpid;

  if (!target_has_execution)
    {
      lwpid = 0;
      bfd_map_over_sections (core_bfd, fbsd_core_get_first_lwp, &lwpid);
      return lwpid;
    }
  if (ptrace (PT_LWPINFO, pid, (caddr_t)&pl, sizeof(pl)))
    perror_with_name("PT_LWPINFO");

  return (long)pl.pl_lwpid;
}

static void
get_current_thread ()
{
  long lwp;
  ptid_t tmp, ptid;

  lwp = get_current_lwp (proc_handle.pid);
  tmp = BUILD_LWP (lwp, proc_handle.pid);
  ptid = thread_from_lwp (tmp);
  if (!in_thread_list (ptid))
    {
      add_thread (ptid);
      inferior_ptid = ptid;
    }
}

static void
fbsd_thread_activate (void)
{
  fbsd_thread_active = 1;
  init_thread_list();
  fbsd_thread_find_new_threads ();
  get_current_thread ();
}

static void
fbsd_thread_deactivate (void)
{
  td_ta_delete_p (thread_agent);

  inferior_ptid = pid_to_ptid (proc_handle.pid);
  proc_handle.pid = 0;
  fbsd_thread_active = 0;
  fbsd_thread_present = 0;
  init_thread_list ();
}

static void
fbsd_thread_new_objfile (struct objfile *objfile)
{
  td_err_e err;

  if (objfile == NULL)
    {
      /* All symbols have been discarded.  If the thread_db target is
         active, deactivate it now.  */
      if (fbsd_thread_active)
        {
          gdb_assert (proc_handle.pid == 0);
          fbsd_thread_active = 0;
        }

      goto quit;
    }

  if (!child_suppress_run)
    goto quit;

  /* Nothing to do.  The thread library was already detected and the
     target vector was already activated.  */
  if (fbsd_thread_active)
    goto quit;

  /* Initialize the structure that identifies the child process.  Note
     that at this point there is no guarantee that we actually have a
     child process.  */
  proc_handle.pid = GET_PID (inferior_ptid);
  
  /* Now attempt to open a connection to the thread library.  */
  err = td_ta_new_p (&proc_handle, &thread_agent);
  switch (err)
    {
    case TD_NOLIBTHREAD:
      /* No thread library was detected.  */
      break;

    case TD_OK:
      /* The thread library was detected.  Activate the thread_db target.  */
      fbsd_thread_present = 1;

      /* We can only poke around if there actually is a child process.
         If there is no child process alive, postpone the steps below
         until one has been created.  */
      if (fbsd_thread_core == 0 && proc_handle.pid != 0)
        {
          push_target(&fbsd_thread_ops);
          fbsd_thread_activate();
        }
      else
        {
          td_ta_delete_p(thread_agent);
          thread_agent = NULL;
        }
      break;

    default:
      warning ("Cannot initialize thread debugging library: %s",
               thread_db_err_str (err));
      break;
    }

 quit:
  if (target_new_objfile_chain)
    target_new_objfile_chain (objfile);
}

static void
fbsd_thread_attach (char *args, int from_tty)
{
  fbsd_thread_core = 0;

  child_ops.to_attach (args, from_tty);

  /* Must get symbols from solibs before libthread_db can run! */
  SOLIB_ADD ((char *) 0, from_tty, (struct target_ops *) 0, auto_solib_add);

  if (fbsd_thread_present && !fbsd_thread_active)
    push_target(&fbsd_thread_ops);
}

static void
fbsd_thread_post_attach (int pid)
{
  child_ops.to_post_attach (pid);

  if (fbsd_thread_present && !fbsd_thread_active)
    {
      proc_handle.pid = GET_PID (inferior_ptid);
      fbsd_thread_activate ();
    }
}

static void
fbsd_thread_detach (char *args, int from_tty)
{
  fbsd_thread_deactivate ();
  unpush_target (&fbsd_thread_ops);

  /* Clear gdb solib information and symbol file
     cache, so that after detach and re-attach, new_objfile
     hook will be called */

  clear_solib();
  symbol_file_clear(0);
  proc_handle.pid = 0;
  child_ops.to_detach (args, from_tty);
}

static int
suspend_thread_callback (const td_thrhandle_t *th_p, void *data)
{
  int err = td_thr_dbsuspend_p (th_p);
  if (err != 0)
	fprintf_filtered(gdb_stderr, "%s %s\n", __func__, thread_db_err_str (err));
  return (err);
}

static int
resume_thread_callback (const td_thrhandle_t *th_p, void *data)
{
  int err = td_thr_dbresume_p (th_p);
  if (err != 0)
	fprintf_filtered(gdb_stderr, "%s %s\n", __func__, thread_db_err_str (err));
  return (err);
}

static void
fbsd_thread_resume (ptid_t ptid, int step, enum target_signal signo)
{
  td_thrhandle_t th;
  td_thrinfo_t ti;
  ptid_t work_ptid;
  int resume_all, ret;
  long lwp, thvalid = 0;

  if (!fbsd_thread_active)
    {
      child_ops.to_resume (ptid, step, signo);
      return;
    }

  if (GET_PID(ptid) != -1 && step != 0)
    {
      resume_all = 0;
      work_ptid = ptid;
    }
  else
    {
      resume_all = 1;
      work_ptid = inferior_ptid;
    }

  lwp = GET_LWP (work_ptid);
  if (lwp == 0)
    {
      /* check user thread */
      ret = td_ta_map_id2thr_p (thread_agent, GET_THREAD(work_ptid), &th);
      if (ret)
        error (thread_db_err_str (ret));

      /* For M:N thread, we need to tell UTS to set/unset single step
         flag at context switch time, the flag will be written into
         thread mailbox. This becauses some architecture may not have
         machine single step flag in ucontext, so we put the flag in mailbox,
         when the thread switches back, kse_switchin restores the single step
         state.  */
      ret = td_thr_sstep_p (&th, step);
      if (ret)
        error (thread_db_err_str (ret));
      ret = td_thr_get_info_p (&th, &ti);
      if (ret)
        error (thread_db_err_str (ret));
      thvalid = 1;
      lwp = ti.ti_lid;
    }

  if (lwp)
    {
      int req = step ? PT_SETSTEP : PT_CLEARSTEP;
      if (ptrace (req, (pid_t) lwp, (caddr_t) 1, target_signal_to_host(signo)))
        perror_with_name ("PT_SETSTEP/PT_CLEARSTEP");
    }

  if (!ptid_equal (last_single_step_thread, null_ptid))
    {
       ret = td_ta_thr_iter_p (thread_agent, resume_thread_callback, NULL,
          TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
          TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
      if (ret != TD_OK)
        error ("resume error: %s", thread_db_err_str (ret));
    }

  if (!resume_all)
    {
      ret = td_ta_thr_iter_p (thread_agent, suspend_thread_callback, NULL,
          TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
          TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
      if (ret != TD_OK)
        error ("suspend error: %s", thread_db_err_str (ret));
      last_single_step_thread = work_ptid;
    }
  else
    last_single_step_thread = null_ptid;

  if (thvalid)
    {
      ret = td_thr_dbresume_p (&th);
      if (ret != TD_OK)
        error ("resume error: %s", thread_db_err_str (ret));
    }
  else
    {
      /* it is not necessary, put it here for completness */
      ret = ptrace(PT_RESUME, lwp, 0, 0);
    }

  /* now continue the process, suspended thread wont run */
  if (ptrace (PT_CONTINUE, proc_handle.pid , (caddr_t)1,
	      target_signal_to_host(signo)))
    perror_with_name ("PT_CONTINUE");
}

static ptid_t
fbsd_thread_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  ptid_t ret;
  long lwp;
  CORE_ADDR stop_pc;

  ret = child_ops.to_wait (ptid, ourstatus);
  if (GET_PID(ret) >= 0 && ourstatus->kind == TARGET_WAITKIND_STOPPED)
    {
      lwp = get_current_lwp (GET_PID(ret));
      ret = thread_from_lwp (BUILD_LWP (lwp, GET_PID (ret)));
      if (!in_thread_list (ret))
        add_thread (ret);
      /* this is a hack, if an event won't cause gdb to stop, for example,
         SIGARLM, gdb resumes the process immediatly without setting
         inferior_ptid to the new thread returned here, this is a bug
         because inferior_ptid may already not exist there, and passing
         a none existing thread to fbsd_thread_resume causes error. */
      if (!fbsd_thread_alive (inferior_ptid))
        {
          delete_thread (inferior_ptid);
          inferior_ptid = ret;
       }
    }

  return (ret);
}

static int
fbsd_thread_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
                        struct mem_attrib *attrib, struct target_ops *target)
{
  int err;

  if (target_has_execution)
    err = child_ops.to_xfer_memory (memaddr, myaddr, len, write, attrib,
	target);
  else
    err = orig_core_ops.to_xfer_memory (memaddr, myaddr, len, write, attrib,
	target);

  return (err);
}

static void
fbsd_lwp_fetch_registers (int regno)
{
  gregset_t gregs;
  fpregset_t fpregs;
  lwpid_t lwp;

  if (!target_has_execution)
    {
      orig_core_ops.to_fetch_registers (-1);
      return;
    }

  lwp = GET_LWP (inferior_ptid);

  if (ptrace (PT_GETREGS, lwp, (caddr_t) &gregs, 0) == -1)
    error ("Cannot get lwp %d registers: %s\n", lwp, safe_strerror (errno));
  supply_gregset (&gregs);
  
  if (ptrace (PT_GETFPREGS, lwp, (caddr_t) &fpregs, 0) == -1)
    error ("Cannot get lwp %d registers: %s\n ", lwp, safe_strerror (errno));

  supply_fpregset (&fpregs);
}

static void
fbsd_thread_fetch_registers (int regno)
{
  prgregset_t gregset;
  prfpregset_t fpregset;
  td_thrhandle_t th;
  td_err_e err;

  if (!IS_THREAD (inferior_ptid))
    {
      fbsd_lwp_fetch_registers (regno);
      return;
    }

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (inferior_ptid), &th);
  if (err != TD_OK)
    error ("Cannot find thread %d: Thread ID=%ld, %s",
           pid_to_thread_id (inferior_ptid),           
           GET_THREAD (inferior_ptid), thread_db_err_str (err));

  err = td_thr_getgregs_p (&th, gregset);
  if (err != TD_OK)
    error ("Cannot fetch general-purpose registers for thread %d: Thread ID=%ld, %s",
           pid_to_thread_id (inferior_ptid),
           GET_THREAD (inferior_ptid), thread_db_err_str (err));

  err = td_thr_getfpregs_p (&th, &fpregset);
  if (err != TD_OK)
    error ("Cannot get floating-point registers for thread %d: Thread ID=%ld, %s",
           pid_to_thread_id (inferior_ptid),
           GET_THREAD (inferior_ptid), thread_db_err_str (err));

  supply_gregset (gregset);
  supply_fpregset (&fpregset);
}

static void
fbsd_lwp_store_registers (int regno)
{
  gregset_t gregs;
  fpregset_t fpregs;
  lwpid_t lwp;

  /* FIXME, is it possible ? */
  if (!IS_LWP (inferior_ptid))
    {
      child_ops.to_store_registers (regno);
      return ;
    }

  lwp = GET_LWP (inferior_ptid);
  if (regno != -1)
    if (ptrace (PT_GETREGS, lwp, (caddr_t) &gregs, 0) == -1)
      error ("Cannot get lwp %d registers: %s\n", lwp, safe_strerror (errno));

  fill_gregset (&gregs, regno);
  if (ptrace (PT_SETREGS, lwp, (caddr_t) &gregs, 0) == -1)
      error ("Cannot set lwp %d registers: %s\n", lwp, safe_strerror (errno));

  if (regno != -1)
    if (ptrace (PT_GETFPREGS, lwp, (caddr_t) &fpregs, 0) == -1)
      error ("Cannot get lwp %d float registers: %s\n", lwp,
             safe_strerror (errno));

  fill_fpregset (&fpregs, regno);
  if (ptrace (PT_SETFPREGS, lwp, (caddr_t) &fpregs, 0) == -1)
     error ("Cannot set lwp %d float registers: %s\n", lwp,
            safe_strerror (errno));
}

static void
fbsd_thread_store_registers (int regno)
{
  prgregset_t gregset;
  prfpregset_t fpregset;
  td_thrhandle_t th;
  td_err_e err;

  if (!IS_THREAD (inferior_ptid))
    {
      fbsd_lwp_store_registers (regno);
      return;
    }

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (inferior_ptid), &th);
  if (err != TD_OK)
    error ("Cannot find thread %d: Thread ID=%ld, %s",
           pid_to_thread_id (inferior_ptid),
           GET_THREAD (inferior_ptid),
           thread_db_err_str (err));

  if (regno != -1)
    {
      char old_value[MAX_REGISTER_SIZE];

      regcache_collect (regno, old_value);
      err = td_thr_getgregs_p (&th, gregset);
      if (err != TD_OK)
        error ("%s: td_thr_getgregs %s", __func__, thread_db_err_str (err));
      err = td_thr_getfpregs_p (&th, &fpregset);
      if (err != TD_OK)
        error ("%s: td_thr_getfpgregs %s", __func__, thread_db_err_str (err));
      supply_register (regno, old_value);
    }

  fill_gregset (gregset, regno);
  fill_fpregset (&fpregset, regno);

  err = td_thr_setgregs_p (&th, gregset);
  if (err != TD_OK)
    error ("Cannot store general-purpose registers for thread %d: Thread ID=%d, %s",
           pid_to_thread_id (inferior_ptid), GET_THREAD (inferior_ptid),
           thread_db_err_str (err));
  err = td_thr_setfpregs_p (&th, &fpregset);
  if (err != TD_OK)
    error ("Cannot store floating-point registers for thread %d: Thread ID=%d, %s",
           pid_to_thread_id (inferior_ptid), GET_THREAD (inferior_ptid),
           thread_db_err_str (err));
}

static void
fbsd_thread_kill (void)
{
  child_ops.to_kill();
}

static int
fbsd_thread_can_run (void)
{
  return child_suppress_run;
}

static void
fbsd_thread_create_inferior (char *exec_file, char *allargs, char **env)
{
  if (fbsd_thread_present && !fbsd_thread_active)
    push_target(&fbsd_thread_ops);

  child_ops.to_create_inferior (exec_file, allargs, env);
}

static void
fbsd_thread_post_startup_inferior (ptid_t ptid)
{
  if (fbsd_thread_present && !fbsd_thread_active)
    {
      /* The child process is now the actual multi-threaded
         program.  Snatch its process ID... */
      proc_handle.pid = GET_PID (ptid);
      td_ta_new_p (&proc_handle, &thread_agent);
      fbsd_thread_activate();
    }
}

static void
fbsd_thread_mourn_inferior (void)
{
  if (fbsd_thread_active)
    fbsd_thread_deactivate ();

  unpush_target (&fbsd_thread_ops);

  child_ops.to_mourn_inferior ();
}

static void
fbsd_core_check_lwp (bfd *abfd, asection *asect, void *obj)
{
  lwpid_t lwp;

  if (strncmp (bfd_section_name (abfd, asect), ".reg/", 5) != 0)
    return;

  /* already found */
  if (*(lwpid_t *)obj == 0)
    return;

  lwp = atoi (bfd_section_name (abfd, asect) + 5);
  if (*(lwpid_t *)obj == lwp)
    *(lwpid_t *)obj = 0;
}

static int
fbsd_thread_alive (ptid_t ptid)
{
  td_thrhandle_t th;
  td_thrinfo_t ti;
  td_err_e err;
  gregset_t gregs;
  lwpid_t lwp;

  if (IS_THREAD (ptid))
    {
      err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (ptid), &th);
      if (err != TD_OK)
        return 0;

      err = td_thr_get_info_p (&th, &ti);
      if (err != TD_OK)
        return 0;

      /* A zombie thread. */
      if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
        return 0;

      return 1;
    }
  else if (GET_LWP (ptid) == 0)
    {
      /* we sometimes are called with lwp == 0 */
      return 1;
    }

  if (fbsd_thread_active)
    {
      err = td_ta_map_lwp2thr_p (thread_agent, GET_LWP (ptid), &th);

      /*
       * if the lwp was already mapped to user thread, don't use it
       * directly, please use user thread id instead.
       */
      if (err == TD_OK)
        return 0;
    }

  if (!target_has_execution)
    {
      lwp = GET_LWP (ptid);
      bfd_map_over_sections (core_bfd, fbsd_core_check_lwp, &lwp);
      return (lwp == 0); 
    }

  /* check lwp in kernel */
  return ptrace (PT_GETREGS, GET_LWP (ptid), (caddr_t)&gregs, 0) == 0;
}

static void
fbsd_thread_files_info (struct target_ops *ignore)
{
  child_ops.to_files_info (ignore);
}

static int
find_new_threads_callback (const td_thrhandle_t *th_p, void *data)
{
  td_thrinfo_t ti;
  td_err_e err;
  ptid_t ptid;

  err = td_thr_get_info_p (th_p, &ti);
  if (err != TD_OK)
    error ("Cannot get thread info: %s", thread_db_err_str (err));

  /* Ignore zombie */
  if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
    return 0;

  ptid = BUILD_THREAD (ti.ti_tid, proc_handle.pid);

  if (!in_thread_list (ptid))
      add_thread (ptid);
  return 0;
}

static void
fbsd_thread_find_new_threads (void)
{
  td_err_e err;

  if (!fbsd_thread_active)
    return;

  /* Iterate over all user-space threads to discover new threads. */
  err = td_ta_thr_iter_p (thread_agent, find_new_threads_callback, NULL,
          TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
          TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
  if (err != TD_OK)
    error ("Cannot find new threads: %s", thread_db_err_str (err));
}

static char *
fbsd_thread_pid_to_str (ptid_t ptid)
{
  static char buf[64];

  if (IS_THREAD (ptid))
    {
      td_thrhandle_t th;
      td_thrinfo_t ti;
      td_err_e err;

      err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (ptid), &th);
      if (err != TD_OK)
        error ("Cannot find thread, Thread ID=%ld, %s",
                GET_THREAD (ptid), thread_db_err_str (err));

      err = td_thr_get_info_p (&th, &ti);
      if (err != TD_OK)
        error ("Cannot get thread info, Thread ID=%ld, %s",
               GET_THREAD (ptid), thread_db_err_str (err));

      if (ti.ti_lid != 0)
        {
          snprintf (buf, sizeof (buf), "Thread %ld (LWP %d)",
                    GET_THREAD (ptid), ti.ti_lid);
        }
      else
        {
          snprintf (buf, sizeof (buf), "Thread %ld (%s)",
                    GET_THREAD (ptid), thread_db_state_str (ti.ti_state));
        }

      return buf;
    }
  else if (IS_LWP (ptid))
    {
      snprintf (buf, sizeof (buf), "LWP %d", (int) GET_LWP (ptid));
      return buf;
    }
  return normal_pid_to_str (ptid);
}

CORE_ADDR
fbsd_thread_get_local_address(ptid_t ptid, struct objfile *objfile,
                              CORE_ADDR offset)
{
  td_thrhandle_t th;
  void *address;
  CORE_ADDR lm;
  int ret, is_library = (objfile->flags & OBJF_SHARED);

  if (IS_THREAD (ptid))
    {
      if (!td_thr_tls_get_addr_p)
        error ("Cannot find thread-local interface in thread_db library.");

      /* Get the address of the link map for this objfile. */
      lm = svr4_fetch_objfile_link_map (objfile);

      /* Couldn't find link map. Bail out. */
      if (!lm)
        {
          if (is_library)
            error ("Cannot find shared library `%s' link_map in dynamic"
                   " linker's module list", objfile->name);
          else
            error ("Cannot find executable file `%s' link_map in dynamic"
                   " linker's module list", objfile->name);
        }

      ret = td_ta_map_id2thr_p (thread_agent, GET_THREAD(ptid), &th);

      /* get the address of the variable. */
      ret = td_thr_tls_get_addr_p (&th, (void *) lm, offset, &address);

      if (ret != TD_OK)
        {
          if (is_library)
            error ("Cannot find thread-local storage for thread %ld, "
                   "shared library %s:\n%s",
                   (long) GET_THREAD (ptid),
                   objfile->name, thread_db_err_str (ret));
          else
            error ("Cannot find thread-local storage for thread %ld, "
                   "executable file %s:\n%s",
                   (long) GET_THREAD (ptid),
                   objfile->name, thread_db_err_str (ret));
        }

      /* Cast assuming host == target. */
      return (CORE_ADDR) address;
    }
  return (0);
}

static int
tsd_cb (thread_key_t key, void (*destructor)(void *), void *ignore)
{
  struct minimal_symbol *ms;
  char *name;

  ms = lookup_minimal_symbol_by_pc ((CORE_ADDR)destructor);
  if (!ms)
    name = "???";
  else
    name = DEPRECATED_SYMBOL_NAME (ms);

  printf_filtered ("Destructor %p <%s>\n", destructor, name);
  return 0;
}

static void
fbsd_thread_tsd_cmd (char *exp, int from_tty)
{
  if (fbsd_thread_active)
    td_ta_tsd_iter_p (thread_agent, tsd_cb, NULL);
}

static int
ignore (CORE_ADDR addr, char *contents)
{
  return 0;
}

static void
fbsd_core_open (char *filename, int from_tty)
{
  int err;

  fbsd_thread_core = 1;

  orig_core_ops.to_open (filename, from_tty);

  if (fbsd_thread_present)
    {
      err = td_ta_new_p (&proc_handle, &thread_agent);
      if (err == TD_OK)
        {
          proc_handle.pid = elf_tdata (core_bfd)->core_pid;
          fbsd_thread_activate ();
        }
      else
        error ("fbsd_core_open: td_ta_new: %s", thread_db_err_str (err));
    }
}

static void
fbsd_core_close (int quitting)
{
  orig_core_ops.to_close (quitting);
}

static void
fbsd_core_detach (char *args, int from_tty)
{
  if (fbsd_thread_active)
    fbsd_thread_deactivate ();
  unpush_target (&fbsd_thread_ops);
  orig_core_ops.to_detach (args, from_tty);
 
  /* Clear gdb solib information and symbol file
     cache, so that after detach and re-attach, new_objfile
     hook will be called */
  clear_solib();
  symbol_file_clear(0);
}

static void
fbsd_core_files_info (struct target_ops *ignore)
{
  orig_core_ops.to_files_info (ignore);
}

static void
init_fbsd_core_ops (void)
{
  fbsd_core_ops.to_shortname = "FreeBSD-core";
  fbsd_core_ops.to_longname = "FreeBSD core thread.";
  fbsd_core_ops.to_doc = "FreeBSD threads support for core files.";
  fbsd_core_ops.to_open = fbsd_core_open;
  fbsd_core_ops.to_close = fbsd_core_close;
  fbsd_core_ops.to_attach = 0;
  fbsd_core_ops.to_post_attach = 0;
  fbsd_core_ops.to_detach = fbsd_core_detach;
  /* fbsd_core_ops.to_resume  = 0; */
  /* fbsd_core_ops.to_wait  = 0;  */
  fbsd_core_ops.to_fetch_registers = fbsd_thread_fetch_registers;
  /* fbsd_core_ops.to_store_registers  = 0; */
  /* fbsd_core_ops.to_prepare_to_store  = 0; */
  fbsd_core_ops.to_xfer_memory = fbsd_thread_xfer_memory;
  fbsd_core_ops.to_files_info = fbsd_core_files_info;
  fbsd_core_ops.to_insert_breakpoint = ignore;
  fbsd_core_ops.to_remove_breakpoint = ignore;
  /* fbsd_core_ops.to_lookup_symbol  = 0; */
  fbsd_core_ops.to_create_inferior = fbsd_thread_create_inferior;
  fbsd_core_ops.to_stratum = core_stratum;
  fbsd_core_ops.to_has_all_memory = 0;
  fbsd_core_ops.to_has_memory = 1;
  fbsd_core_ops.to_has_stack = 1;
  fbsd_core_ops.to_has_registers = 1;
  fbsd_core_ops.to_has_execution = 0;
  fbsd_core_ops.to_has_thread_control = tc_none;
  fbsd_core_ops.to_thread_alive = fbsd_thread_alive;
  fbsd_core_ops.to_pid_to_str = fbsd_thread_pid_to_str;
  fbsd_core_ops.to_find_new_threads = fbsd_thread_find_new_threads;
  fbsd_core_ops.to_sections = 0;
  fbsd_core_ops.to_sections_end = 0;
  fbsd_core_ops.to_magic = OPS_MAGIC;
}

static void
init_fbsd_thread_ops (void)
{
  fbsd_thread_ops.to_shortname = "freebsd-threads";
  fbsd_thread_ops.to_longname = "FreeBSD multithreaded child process.";
  fbsd_thread_ops.to_doc = "FreeBSD threads support.";
  fbsd_thread_ops.to_attach = fbsd_thread_attach;
  fbsd_thread_ops.to_detach = fbsd_thread_detach;
  fbsd_thread_ops.to_post_attach = fbsd_thread_post_attach;
  fbsd_thread_ops.to_resume = fbsd_thread_resume;
  fbsd_thread_ops.to_wait = fbsd_thread_wait;
  fbsd_thread_ops.to_fetch_registers = fbsd_thread_fetch_registers;
  fbsd_thread_ops.to_store_registers = fbsd_thread_store_registers;
  fbsd_thread_ops.to_xfer_memory = fbsd_thread_xfer_memory;
  fbsd_thread_ops.to_files_info = fbsd_thread_files_info;
  fbsd_thread_ops.to_kill = fbsd_thread_kill;
  fbsd_thread_ops.to_create_inferior = fbsd_thread_create_inferior;
  fbsd_thread_ops.to_post_startup_inferior = fbsd_thread_post_startup_inferior;
  fbsd_thread_ops.to_mourn_inferior = fbsd_thread_mourn_inferior;
  fbsd_thread_ops.to_can_run = fbsd_thread_can_run;
  fbsd_thread_ops.to_thread_alive = fbsd_thread_alive;
  fbsd_thread_ops.to_find_new_threads = fbsd_thread_find_new_threads;
  fbsd_thread_ops.to_pid_to_str = fbsd_thread_pid_to_str;
  fbsd_thread_ops.to_stratum = thread_stratum;
  fbsd_thread_ops.to_has_thread_control = tc_none;
  fbsd_thread_ops.to_has_all_memory = 1;
  fbsd_thread_ops.to_has_memory = 1;
  fbsd_thread_ops.to_has_stack = 1;
  fbsd_thread_ops.to_has_registers = 1;
  fbsd_thread_ops.to_has_execution = 1;
  fbsd_thread_ops.to_insert_breakpoint = memory_insert_breakpoint;
  fbsd_thread_ops.to_remove_breakpoint = memory_remove_breakpoint;
  fbsd_thread_ops.to_get_thread_local_address = fbsd_thread_get_local_address;
  fbsd_thread_ops.to_magic = OPS_MAGIC;
}

static int
thread_db_load (void)
{
  void *handle;
  td_err_e err;

  handle = dlopen (LIBTHREAD_DB_SO, RTLD_NOW);
  if (handle == NULL)
      return 0;

#define resolve(X)			\
 if (!(X##_p = dlsym (handle, #X)))	\
   return 0;

  resolve(td_init);
  resolve(td_ta_new);
  resolve(td_ta_delete);
  resolve(td_ta_map_id2thr);
  resolve(td_ta_map_lwp2thr);
  resolve(td_ta_thr_iter);
  resolve(td_thr_get_info);
  resolve(td_thr_getfpregs);
  resolve(td_thr_getgregs);
  resolve(td_thr_setfpregs);
  resolve(td_thr_setgregs);
  resolve(td_thr_sstep);
  resolve(td_ta_tsd_iter);
  resolve(td_thr_dbsuspend);
  resolve(td_thr_dbresume);
  resolve(td_thr_tls_get_addr);

  /* Initialize the library.  */
  err = td_init_p ();
  if (err != TD_OK)
    {
      warning ("Cannot initialize libthread_db: %s", thread_db_err_str (err));
      return 0;
    }

  return 1;
}

/* we suppress the call to add_target of core_ops in corelow because
   if there are two targets in the stratum core_stratum, find_core_target
   won't know which one to return.  see corelow.c for an additonal
   comment on coreops_suppress_target. */

int coreops_suppress_target = 1;

void
_initialize_thread_db (void)
{
  init_fbsd_thread_ops ();
  init_fbsd_core_ops ();

  if (thread_db_load ())
    {
      add_target (&fbsd_thread_ops);

      /* "thread tsd" command */
      add_cmd ("tsd", class_run, fbsd_thread_tsd_cmd,
            "Show the thread-specific data keys and destructors "
            "for the process.\n",
           &thread_cmd_list);

      memcpy (&orig_core_ops, &core_ops, sizeof (struct target_ops));
      memcpy (&core_ops, &fbsd_core_ops, sizeof (struct target_ops));
      add_target (&core_ops);

      /* Add ourselves to objfile event chain. */
      target_new_objfile_chain = target_new_objfile_hook;
      target_new_objfile_hook = fbsd_thread_new_objfile;

      child_suppress_run = 1;
    }
  else
    {
      fprintf_unfiltered (gdb_stderr,
        "[GDB will not be able to debug user-mode threads: %s]\n", dlerror());
     
      /* allow the user to debug non-threaded core files */
      add_target (&core_ops);
    }
}

/* proc service functions */
void
ps_plog (const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vfprintf_filtered (gdb_stderr, fmt, args);
  va_end (args);
}

ps_err_e
ps_pglobal_lookup (struct ps_prochandle *ph, const char *obj,
   const char *name, psaddr_t *sym_addr)
{
  struct minimal_symbol *ms;

  ms = lookup_minimal_symbol (name, NULL, NULL);
  if (ms == NULL)
    return PS_NOSYM;

  *sym_addr = (psaddr_t) SYMBOL_VALUE_ADDRESS (ms);
  return PS_OK;
}

ps_err_e
ps_pread (struct ps_prochandle *ph, psaddr_t addr, void *buf, size_t len)
{
  int err = target_read_memory ((CORE_ADDR) addr, buf, len);
  return (err == 0 ? PS_OK : PS_ERR);
}

ps_err_e
ps_pwrite (struct ps_prochandle *ph, psaddr_t addr, const void *buf,
            size_t len)
{
  int err = target_write_memory ((CORE_ADDR) addr, (void *)buf, len);
  return (err == 0 ? PS_OK : PS_ERR);
}

ps_err_e
ps_lgetregs (struct ps_prochandle *ph, lwpid_t lwpid, prgregset_t gregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();
  inferior_ptid = BUILD_LWP (lwpid, PIDGET (inferior_ptid));
  target_fetch_registers (-1);
  fill_gregset (gregset, -1);
  do_cleanups (old_chain);
  return PS_OK;
}

ps_err_e
ps_lsetregs (struct ps_prochandle *ph, lwpid_t lwpid, const prgregset_t gregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();
  inferior_ptid = BUILD_LWP (lwpid, PIDGET (inferior_ptid));
  supply_gregset (gregset);
  target_store_registers (-1);
  do_cleanups (old_chain);
  return PS_OK;
}

ps_err_e
ps_lgetfpregs (struct ps_prochandle *ph, lwpid_t lwpid, prfpregset_t *fpregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();
  inferior_ptid = BUILD_LWP (lwpid, PIDGET (inferior_ptid));
  target_fetch_registers (-1);
  fill_fpregset (fpregset, -1);
  do_cleanups (old_chain);
  return PS_OK;
}

ps_err_e
ps_lsetfpregs (struct ps_prochandle *ph, lwpid_t lwpid,
               const prfpregset_t *fpregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();
  inferior_ptid = BUILD_LWP (lwpid, PIDGET (inferior_ptid));
  supply_fpregset (fpregset);
  target_store_registers (-1);
  do_cleanups (old_chain);
  return PS_OK;
}

ps_err_e
ps_lstop(struct ps_prochandle *ph, lwpid_t lwpid)
{
  if (ptrace (PT_SUSPEND, lwpid, 0, 0) == -1)
    return PS_ERR;
  return PS_OK;  
}

ps_err_e
ps_lcontinue(struct ps_prochandle *ph, lwpid_t lwpid)
{
  if (ptrace (PT_RESUME, lwpid, 0, 0) == -1)
    return PS_ERR;
  return PS_OK;   
}
