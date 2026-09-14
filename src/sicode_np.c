#include "sicode_np.h"

#define _SICODE_CASE(code)                                                     \
  case code:                                                                   \
    return #code

__attribute((const)) const char *_sicode_np(int sig, int si_code) noexcept {
  switch (sig) {
  case SIGILL:
    switch (si_code) {
      _SICODE_CASE(ILL_ILLOPC);
      _SICODE_CASE(ILL_ILLOPN);
      _SICODE_CASE(ILL_ILLADR);
      _SICODE_CASE(ILL_ILLTRP);
      _SICODE_CASE(ILL_PRVOPC);
      _SICODE_CASE(ILL_PRVREG);
      _SICODE_CASE(ILL_COPROC);
      _SICODE_CASE(ILL_BADSTK);
      _SICODE_CASE(ILL_BADIADDR);
    }
    break;

  case SIGFPE:
    switch (si_code) {
      _SICODE_CASE(FPE_INTDIV);
      _SICODE_CASE(FPE_INTOVF);
      _SICODE_CASE(FPE_FLTDIV);
      _SICODE_CASE(FPE_FLTOVF);
      _SICODE_CASE(FPE_FLTUND);
      _SICODE_CASE(FPE_FLTRES);
      _SICODE_CASE(FPE_FLTINV);
      _SICODE_CASE(FPE_FLTSUB);
      _SICODE_CASE(FPE_FLTUNK);
      _SICODE_CASE(FPE_CONDTRAP);
    }
    break;

  case SIGSEGV:
    switch (si_code) {
      _SICODE_CASE(SEGV_MAPERR);
      _SICODE_CASE(SEGV_ACCERR);
      _SICODE_CASE(SEGV_BNDERR);
      _SICODE_CASE(SEGV_PKUERR);
      _SICODE_CASE(SEGV_ACCADI);
      _SICODE_CASE(SEGV_ADIDERR);
      _SICODE_CASE(SEGV_ADIPERR);
      _SICODE_CASE(SEGV_MTEAERR);
      _SICODE_CASE(SEGV_MTESERR);
      _SICODE_CASE(SEGV_CPERR);
    }
    break;

  case SIGBUS:
    switch (si_code) {
      _SICODE_CASE(BUS_ADRALN);
      _SICODE_CASE(BUS_ADRERR);
      _SICODE_CASE(BUS_OBJERR);
      _SICODE_CASE(BUS_MCEERR_AR);
      _SICODE_CASE(BUS_MCEERR_AO);
    }
    break;

  case SIGTRAP:
    switch (si_code) {
      _SICODE_CASE(TRAP_BRKPT);
      _SICODE_CASE(TRAP_TRACE);
      _SICODE_CASE(TRAP_BRANCH);
      _SICODE_CASE(TRAP_HWBKPT);
      _SICODE_CASE(TRAP_UNK);
    }
    break;

  case SIGCHLD:
    switch (si_code) {
      _SICODE_CASE(CLD_EXITED);
      _SICODE_CASE(CLD_KILLED);
      _SICODE_CASE(CLD_DUMPED);
      _SICODE_CASE(CLD_TRAPPED);
      _SICODE_CASE(CLD_STOPPED);
      _SICODE_CASE(CLD_CONTINUED);
    }
    break;

  case SIGPOLL:
    switch (si_code) {
      _SICODE_CASE(POLL_IN);
      _SICODE_CASE(POLL_OUT);
      _SICODE_CASE(POLL_MSG);
      _SICODE_CASE(POLL_ERR);
      _SICODE_CASE(POLL_PRI);
      _SICODE_CASE(POLL_HUP);
    }
    break;
  }

  switch (si_code) {
    _SICODE_CASE(SI_ASYNCNL);
    _SICODE_CASE(SI_DETHREAD);
    _SICODE_CASE(SI_TKILL);
    _SICODE_CASE(SI_SIGIO);
    _SICODE_CASE(SI_ASYNCIO);
    _SICODE_CASE(SI_MESGQ);
    _SICODE_CASE(SI_TIMER);
    _SICODE_CASE(SI_QUEUE);
    _SICODE_CASE(SI_USER);
    _SICODE_CASE(SI_KERNEL);
  }

  return NULL;
}

#undef _SICODE_CASE
