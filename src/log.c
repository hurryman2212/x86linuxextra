#ifndef __KERNEL__

#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <unistd.h>

#include <sys/mman.h>

#include <backtrace.h>

#include "generated/version.h"

#endif

#include "x86linux/helper.h"

#ifdef __KERNEL__
int _log_lvl = LOG_DISABLED;
#else
static int _log_initial_lvl = LOG_DISABLED;
int *_log_lvlp = &_log_initial_lvl;

static const char _LOG_ABORT_MSG[] = "Aborted.";
static const char _LOG_ASSERT_MSG[] = "Assertion `%s' failed.";

static const char *const _LOG_LVL_TO_COLOR[] = {
    "\e[1;41;37m", "\e[1;101m", "\e[101m", "\e[1;31m",
    "\e[1;33m",    "\e[1;34m",  "\e[34m",  "\e[2m",
};
static const char *const _LOG_LVL_TO_STR[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG",
};

static const char _LOG_BACKTRACE_MSG[] = "Call Trace:";
static const char _LOG_BACKTRACE_STDERR_FMT[] = " %s%s(%s:%d) [0x%lx]\e[0m\n";
static const char _LOG_BACKTRACE_STDERR_FMT_OFF[] =
    " %s%s+0x%lx(%s:%d) [0x%lx]\e[0m\n";
static const char _LOG_BACKTRACE_STDERR_FMT_UNKNOWN[] =
    " %s%s+?(%s:%d) [0x%lx]\e[0m\n";
static const char _LOG_BACKTRACE_SYSLOG_FMT[] = " %s%s(%s:%d) [0x%lx]\e[0m";
static const char _LOG_BACKTRACE_SYSLOG_FMT_OFF[] =
    " %s%s+0x%lx(%s:%d) [0x%lx]\e[0m";
static const char _LOG_BACKTRACE_SYSLOG_FMT_UNKNOWN[] =
    " %s%s+?(%s:%d) [0x%lx]\e[0m";

static const char _LOG_PERROR_ARG[] = "%s (%s)";
static const char _LOG_PERROR_S_ARG[] = "%s: %s (%s)";

static const char _LOG_STDERR_FMT[] = "%s[%d]: %s%s: %s:%d: %s: %s\e[0m\n";
static const char _LOG_SYSLOG_FMT[] = "%s%s: %s:%d: %s: %s\e[0m";

static char *_log_progname_copy;
static const char *_log_ident;
static int _log_init_errno;
static volatile uint32_t *_log_futexp32;
struct _log_shared {
  uint32_t lock;
  int lvl;
};

#define _LOG_SICODE_CASE(code)                                                 \
  case code:                                                                   \
    return #code

static __attribute((const))
const char *_log_sicode_np(int sig, int si_code) noexcept {
  switch (sig) {
  case SIGILL:
    switch (si_code) {
      _LOG_SICODE_CASE(ILL_ILLOPC);
      _LOG_SICODE_CASE(ILL_ILLOPN);
      _LOG_SICODE_CASE(ILL_ILLADR);
      _LOG_SICODE_CASE(ILL_ILLTRP);
      _LOG_SICODE_CASE(ILL_PRVOPC);
      _LOG_SICODE_CASE(ILL_PRVREG);
      _LOG_SICODE_CASE(ILL_COPROC);
      _LOG_SICODE_CASE(ILL_BADSTK);
      _LOG_SICODE_CASE(ILL_BADIADDR);
    }
    break;

  case SIGFPE:
    switch (si_code) {
      _LOG_SICODE_CASE(FPE_INTDIV);
      _LOG_SICODE_CASE(FPE_INTOVF);
      _LOG_SICODE_CASE(FPE_FLTDIV);
      _LOG_SICODE_CASE(FPE_FLTOVF);
      _LOG_SICODE_CASE(FPE_FLTUND);
      _LOG_SICODE_CASE(FPE_FLTRES);
      _LOG_SICODE_CASE(FPE_FLTINV);
      _LOG_SICODE_CASE(FPE_FLTSUB);
      _LOG_SICODE_CASE(FPE_FLTUNK);
      _LOG_SICODE_CASE(FPE_CONDTRAP);
    }
    break;

  case SIGSEGV:
    switch (si_code) {
      _LOG_SICODE_CASE(SEGV_MAPERR);
      _LOG_SICODE_CASE(SEGV_ACCERR);
      _LOG_SICODE_CASE(SEGV_BNDERR);
      _LOG_SICODE_CASE(SEGV_PKUERR);
      _LOG_SICODE_CASE(SEGV_ACCADI);
      _LOG_SICODE_CASE(SEGV_ADIDERR);
      _LOG_SICODE_CASE(SEGV_ADIPERR);
      _LOG_SICODE_CASE(SEGV_MTEAERR);
      _LOG_SICODE_CASE(SEGV_MTESERR);
      _LOG_SICODE_CASE(SEGV_CPERR);
    }
    break;

  case SIGBUS:
    switch (si_code) {
      _LOG_SICODE_CASE(BUS_ADRALN);
      _LOG_SICODE_CASE(BUS_ADRERR);
      _LOG_SICODE_CASE(BUS_OBJERR);
      _LOG_SICODE_CASE(BUS_MCEERR_AR);
      _LOG_SICODE_CASE(BUS_MCEERR_AO);
    }
    break;

  case SIGTRAP:
    switch (si_code) {
      _LOG_SICODE_CASE(TRAP_BRKPT);
      _LOG_SICODE_CASE(TRAP_TRACE);
      _LOG_SICODE_CASE(TRAP_BRANCH);
      _LOG_SICODE_CASE(TRAP_HWBKPT);
      _LOG_SICODE_CASE(TRAP_UNK);
    }
    break;

  case SIGCHLD:
    switch (si_code) {
      _LOG_SICODE_CASE(CLD_EXITED);
      _LOG_SICODE_CASE(CLD_KILLED);
      _LOG_SICODE_CASE(CLD_DUMPED);
      _LOG_SICODE_CASE(CLD_TRAPPED);
      _LOG_SICODE_CASE(CLD_STOPPED);
      _LOG_SICODE_CASE(CLD_CONTINUED);
    }
    break;

  case SIGPOLL:
    switch (si_code) {
      _LOG_SICODE_CASE(POLL_IN);
      _LOG_SICODE_CASE(POLL_OUT);
      _LOG_SICODE_CASE(POLL_MSG);
      _LOG_SICODE_CASE(POLL_ERR);
      _LOG_SICODE_CASE(POLL_PRI);
      _LOG_SICODE_CASE(POLL_HUP);
    }
    break;
  }

  switch (si_code) {
    _LOG_SICODE_CASE(SI_ASYNCNL);
    _LOG_SICODE_CASE(SI_DETHREAD);
    _LOG_SICODE_CASE(SI_TKILL);
    _LOG_SICODE_CASE(SI_SIGIO);
    _LOG_SICODE_CASE(SI_ASYNCIO);
    _LOG_SICODE_CASE(SI_MESGQ);
    _LOG_SICODE_CASE(SI_TIMER);
    _LOG_SICODE_CASE(SI_QUEUE);
    _LOG_SICODE_CASE(SI_USER);
    _LOG_SICODE_CASE(SI_KERNEL);
  }

  return NULL;
}

#undef _LOG_SICODE_CASE

static __attribute((constructor(101))) void _log_pre_init(void) noexcept {
  const int _errno_save = errno;
  const char *_program_name = program_invocation_short_name;
  if (!_program_name)
    _program_name = "?";
  const size_t _size = strlen(_program_name) + 1;
  _log_progname_copy = malloc(_size);
  if (unlikely(!_log_progname_copy)) {
    _log_init_errno = errno ? errno : ENOMEM;
    _log_ident = _program_name;
    goto _done;
  }
  strcpy(_log_progname_copy, _program_name);
  _log_ident = _log_progname_copy;

  struct _log_shared *_shared =
      mmap(NULL, sizeof(*_shared), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (unlikely(_shared == MAP_FAILED)) {
    _log_init_errno = errno ? errno : ENOMEM;
    goto _done;
  }
  _shared->lock = 0;
  _shared->lvl = log_lvl();
  _log_futexp32 = &_shared->lock;
  _log_lvlp = &_shared->lvl;

  const char *_env = getenv("X86LINUX_LOG_LVL");
  if (_env) {
    LOG_INIT();
    log_enable(atoi(_env));
  }

  log_msg(LOG_DEBUG,
          "libx86linuxextra (version %s) by Jihong Min "
          "(hurryman2212@gmail.com)",
          version);

_done:
  errno = _errno_save;
}

static int _log_lock_ready(void) {
  if (likely(_log_futexp32))
    return 0;
  errno = _log_init_errno ? _log_init_errno : ENODEV;
  return -1;
}

int log_lock(void) noexcept {
  if (_log_lock_ready() == -1)
    return -1;
  return usersched_lock_pi2(_log_futexp32, gettid(),
                            USERSCHED_LOCK_RESTART | USERSCHED_LOCK_NOEAGAIN,
                            100 * usersched_tsc_1us, NULL);
}
int log_unlock(void) noexcept {
  if (_log_lock_ready() == -1)
    return -1;
  return usersched_unlock_pi(_log_futexp32, gettid(), 0);
}
int log_plock(void) noexcept {
  if (_log_lock_ready() == -1)
    return -1;
  return usersched_plock_pi2(_log_futexp32, gettid(),
                             USERSCHED_LOCK_RESTART | USERSCHED_LOCK_NOEAGAIN,
                             100 * usersched_tsc_1us, NULL, NULL, NULL);
}
int log_punlock(void) noexcept {
  if (_log_lock_ready() == -1)
    return -1;
  return usersched_punlock_pi(_log_futexp32, gettid(), 0, NULL);
}

void _log_abort(const char *restrict filename, int line,
                const char *restrict func, int skip_lock) noexcept {
  if (!skip_lock)
    log_plock();

  _log(LOG_EMERG, filename, line, func, _LOG_ABORT_MSG);
  _log_backtrace(LOG_EMERG, filename, line, func, 1);

  if (!skip_lock)
    log_punlock();

  abort(); // This generates no message on `stdout`/`stderr`.
}
void _log_assert_fail(const char *restrict filename, int line,
                      const char *restrict func,
                      const char *restrict expression, int skip_lock) noexcept {
  if (!skip_lock)
    log_plock();

  _log(LOG_EMERG, filename, line, func, _LOG_ASSERT_MSG, expression);
  _log_abort(filename, line, func, 1);
}
void _log_assert_perror_fail(const char *restrict filename, int line,
                             const char *restrict func,
                             const char *restrict expression, int errnum,
                             int skip_lock) noexcept {
  if (!skip_lock)
    log_plock();

  _log_perror(LOG_EMERG, filename, line, func, expression, errnum);
  _log_abort(filename, line, func, 1);
}

static int _log_use_syslog;

struct _log_backtrace_data {
  int lvl;
  const char *filename;
  int line;
  const char *func;
};

static void _log_backtrace_err_cb(void *restrict data, const char *restrict msg,
                                  int errnum) {
  const struct _log_backtrace_data *_data = data;
  (void)_log(_data->lvl, FILE_NAME, __LINE__, __func__,
             "Backtrace unavailable: %s (%d)", msg ? msg : "unknown error",
             errnum);
}

static struct backtrace_state *_log_backtrace_state;
static pthread_once_t _log_backtrace_once = PTHREAD_ONCE_INIT;
static int _log_backtrace_errno;
static void _log_backtrace_init(void) {
  const int _errno_save = errno;
  static struct _log_backtrace_data _data = {.lvl = LOG_ERR};
  _log_backtrace_state =
      backtrace_create_state(NULL, 1, _log_backtrace_err_cb, &_data);
  if (unlikely(!_log_backtrace_state))
    _log_backtrace_errno = errno ? errno : ENOMEM;
  errno = _errno_save;
}
static int _log_backtrace_ready(void) {
  const int _ret = pthread_once(&_log_backtrace_once, _log_backtrace_init);
  if (unlikely(_ret)) {
    errno = _ret;
    return -1;
  }
  if (unlikely(!_log_backtrace_state)) {
    errno = _log_backtrace_errno ? _log_backtrace_errno : ENOSYS;
    return -1;
  }
  return 0;
}

static int _log_backtrace_cb(void *restrict data, uintptr_t pc,
                             const char *restrict filepath, int line,
                             const char *restrict func) {
  Dl_info _info = {0};
  const int _has_info = dladdr(addr_cast(pc), &_info);
  const void *_saddr = _has_info ? _info.dli_saddr : NULL;
  const uintptr_t _off = _saddr ? pc - val_cast(_saddr) : 0;
  if (!func)
    func = _has_info && _info.dli_sname ? _info.dli_sname : "?";
  if (!filepath)
    filepath = _has_info && _info.dli_fname ? _info.dli_fname : "?";

  const struct _log_backtrace_data *_data = data;
  const int _lvl = _data->lvl;
  if (_data->filename)
    return _off ? _log(_lvl, _data->filename, _data->line, _data->func,
                       "%s+0x%lx(%s:%d) [0x%lx]", func, _off,
                       filename(filepath), line, pc)
                : _log(_lvl, _data->filename, _data->line, _data->func,
                       _saddr ? "%s(%s:%d) [0x%lx]" : "%s+?(%s:%d) [0x%lx]",
                       func, filename(filepath), line, pc);
  if (_log_use_syslog) {
    if (_off)
      syslog(_lvl, _LOG_BACKTRACE_SYSLOG_FMT_OFF, _LOG_LVL_TO_COLOR[_lvl], func,
             _off, filename(filepath), line, pc);
    else
      syslog(_lvl,
             _saddr ? _LOG_BACKTRACE_SYSLOG_FMT
                    : _LOG_BACKTRACE_SYSLOG_FMT_UNKNOWN,
             _LOG_LVL_TO_COLOR[_lvl], func, filename(filepath), line, pc);
  } else {
    const int _ret = _off
                         ? dprintf(STDERR_FILENO, _LOG_BACKTRACE_STDERR_FMT_OFF,
                                   _LOG_LVL_TO_COLOR[_lvl], func, _off,
                                   filename(filepath), line, pc)
                         : dprintf(STDERR_FILENO,
                                   _saddr ? _LOG_BACKTRACE_STDERR_FMT
                                          : _LOG_BACKTRACE_STDERR_FMT_UNKNOWN,
                                   _LOG_LVL_TO_COLOR[_lvl], func,
                                   filename(filepath), line, pc);
    if (unlikely(_ret < 0))
      return -1;
  }

  return 0;
}
int _log_backtrace(int lvl, const char *restrict filename, int line,
                   const char *restrict func, int skip_lock) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG)) {
    errno = EINVAL;
    return -1;
  }
  if (!skip_lock && log_plock() == -1)
    return -1;

  int _error = 0;
  if (_log(lvl, filename, line, func, "%s", _LOG_BACKTRACE_MSG) == -1)
    _error = errno;

  struct _log_backtrace_data _data = {.lvl = lvl};
  if (!_error && _log_backtrace_ready() == -1)
    _error = errno;
  if (!_error && backtrace_full(_log_backtrace_state, 0, _log_backtrace_cb,
                                _log_backtrace_err_cb, &_data))
    _error = errno ? errno : EIO;

  if (!skip_lock && log_punlock() == -1 && !_error)
    _error = errno;
  if (_error) {
    errno = _error;
    return -1;
  }
  errno = _errno_save;
  return 0;
}

int _log_backtrace_sig(int lvl, const char *restrict filename, int line,
                       const char *restrict func, int sig,
                       const siginfo_t *restrict info,
                       const void *restrict ctx) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG)) {
    errno = EINVAL;
    return -1;
  }
  if (likely(log_lvl() < lvl))
    return 0;
  if (unlikely(!info || !ctx)) {
    errno = EINVAL;
    return -1;
  }

  const ucontext_t *_u = ctx;
  const uintptr_t _pc = _u->uc_mcontext.gregs[REG_RIP];

  if (log_plock() == -1)
    return -1;

  int _error = 0;

  Dl_info _info = {0};
  if (!dladdr(addr_cast(_pc), &_info))
    _info = (Dl_info){0};
  const char *_abbrev = sigabbrev_np(sig);
  const char *_desc = sigdescr_np(sig);
  const char *_code = _log_sicode_np(sig, info->si_code);
  if (_log(lvl, filename, line, func,
           "--- SIG%s (%s) {si_code=%d (%s), si_errno=%d, dli_fname=%s, "
           "dli_fbase=%p} ---",
           _abbrev ? _abbrev : "?", _desc ? _desc : "unknown signal",
           info->si_code, _code ? _code : "unknown", info->si_errno,
           _info.dli_fname ? _info.dli_fname : "<unknown>",
           _info.dli_fbase) == -1)
    _error = errno;

  struct _log_backtrace_data _data = {.lvl = lvl,
                                      .filename = filename ? filename : "?",
                                      .line = line,
                                      .func = func};
  if (!_error && _log_backtrace_ready() == -1)
    _error = errno;
  if (!_error && backtrace_pcinfo(_log_backtrace_state, _pc, _log_backtrace_cb,
                                  _log_backtrace_err_cb, &_data))
    _error = errno ? errno : EIO;

  if (!_error && _log_backtrace(lvl, filename, line, func, 1) == -1)
    _error = errno;
  if (log_punlock() == -1 && !_error)
    _error = errno;
  if (_error) {
    errno = _error;
    return -1;
  }
  errno = _errno_save;
  return 0;
}

int _log_perror(int lvl, const char *restrict filename, int line,
                const char *restrict func, const char *restrict s,
                int errnum) noexcept {
  const int _errno_save = errno;
  const char *_errnodesc = strerrordesc_np(errnum);
  const char *_errnoname = strerrorname_np(errnum);
  if (!_errnodesc)
    _errnodesc = "Unknown error";
  if (!_errnoname)
    _errnoname = "?";

  const int _ret = !s || !*s
                       ? _log(lvl, filename, line, func, _LOG_PERROR_ARG,
                              _errnodesc, _errnoname)
                       : _log(lvl, filename, line, func, _LOG_PERROR_S_ARG, s,
                              _errnodesc, _errnoname);
  if (_ret == -1)
    return -1;
  errno = _errno_save;
  return 0;
}

int _log(int lvl, const char *restrict filename, int line,
         const char *restrict func, const char *restrict fmt, ...) noexcept {
  va_list _ap;
  va_start(_ap, fmt);
  const int _ret = _log_va(lvl, filename, line, func, fmt, _ap);
  va_end(_ap);
  return _ret;
}

enum { _LOG_LINE_MAX = LOG_LINE_MAX * 2 };
int _log_va(int lvl, const char *restrict filename, int line,
            const char *restrict func, const char *restrict fmt,
            va_list ap) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG)) {
    errno = EINVAL;
    return -1;
  }
  char _buf[_LOG_LINE_MAX];
  if (vsnprintf(_buf, sizeof(_buf), fmt, ap) < 0) {
    if (!errno)
      errno = EIO;
    return -1;
  }

  if (_log_use_syslog) {
    syslog(lvl, _LOG_SYSLOG_FMT, _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl],
           filename ? filename : "?", line, func ? func : "?", _buf);
  } else {
    const char *_name = _log_ident ? _log_ident
                                   : (program_invocation_short_name
                                          ? program_invocation_short_name
                                          : "?");
    if (dprintf(STDERR_FILENO, _LOG_STDERR_FMT, _name, gettid(),
                _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl],
                filename ? filename : "?", line, func ? func : "?", _buf) < 0)
      return -1;
  }
  errno = _errno_save;
  return 0;
}

int log_init(const char *restrict ident, int option, int facility,
             int use_syslog) noexcept {
  const int _errno_save = errno;
  if (log_deinit() == -1)
    return -1;

  if (!_log_futexp32 && _log_init_errno) {
    errno = _log_init_errno;
    return -1;
  }

  const char *_new_ident = ident ? ident : _log_progname_copy;
  if (!_new_ident)
    _new_ident = program_invocation_short_name;
  if (!_new_ident) {
    errno = EINVAL;
    return -1;
  }

  if (use_syslog) {
    if (option == -1)
      option = LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID;
    if (facility == -1)
      facility = LOG_USER;

    openlog(_new_ident, option, facility);

    _log_use_syslog = use_syslog;
  }

  _log_ident = _new_ident;
  errno = _errno_save;
  return 0;
}
int log_deinit(void) noexcept {
  const int _errno_save = errno;
  /* Disable logging first. */
  log_disable();

  if (_log_use_syslog) {
    closelog();

    _log_use_syslog = 0;
  }
  _log_ident =
      _log_progname_copy ? _log_progname_copy : program_invocation_short_name;
  errno = _errno_save;
  return 0;
}

#endif
