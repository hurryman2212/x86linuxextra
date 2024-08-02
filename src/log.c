#include "x86linux/helper.h"

#ifndef __KERNEL__

#include <string.h>

#include <sys/mman.h>

#include <backtrace.h>

#endif

int _log_lvl = LOG_DISABLED;

#ifndef __KERNEL__

static const char _LOG_ABORT_MSG[] = "Aborted.";
static const char _LOG_ASSERT_MSG[] = "Assertion `%s' failed.";

static const char *const restrict _LOG_LVL_TO_COLOR[] = {
    "\e[1;41;37m", "\e[1;101m", "\e[101m", "\e[1;31m",
    "\e[1;33m",    "\e[1;34m",  "\e[34m",  "\e[2m",
};
static const char *const restrict _LOG_LVL_TO_STR[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG",
};

static const char _LOG_BACKTRACE_MSG[] = "Call Trace:";
static const char _LOG_BACKTRACE_STDERR_FMT[] = " %s%s(%s:%d) [0x%lx]\e[0m\n";
static const char _LOG_BACKTRACE_SYSLOG_FMT[] = " %s%s(%s:%d) [0x%lx]\e[0m";

static const char _LOG_PERROR_ARG[] = "%s (%s)";
static const char _LOG_PERROR_S_ARG[] = "%s: %s (%s)";

static const char _LOG_STDERR_FMT[] = "%s[%d]: %s%s: %s:%d: %s: %s\e[0m\n";
static const char _LOG_SYSLOG_FMT[] = "%s%s: %s:%d: %s: %s\e[0m";

static uint32_t *restrict _log_futexp;
static __attribute((constructor(101))) void _log_pre_init() {
  const char *const env = getenv("X86LINUX_LOG_LVL");
  if (env) {
    LOG_INIT();
    log_enable(atoi(env));
  }

  log_verify_error(_log_futexp =
                       mmap(NULL, sizeof(*_log_futexp), PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_SHARED, -1, 0));
  *_log_futexp = 0;
  barrier();
}
static void _log_lock() {
  if (likely(_log_futexp))
    /* Should we use `*_plock_pi` and `USERSCHED_RESTART` here? */
    usersched_plock_pi(_log_futexp, USERSCHED_RESTART, 100 * usersched_tsc_1us,
                       NULL, NULL, NULL);
}
static void _log_unlock() {
  if (likely(_log_futexp))
    /* Should we use `*_punlock_pi` here? */
    usersched_punlock_pi(_log_futexp, 0, NULL);
}

void _log_abort(const char *restrict filename, int line,
                const char *restrict func, int skip_lock) {
  if (!skip_lock)
    _log_lock();

  _log(LOG_EMERG, filename, line, func, _LOG_ABORT_MSG);
  _log_backtrace(LOG_EMERG, filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();

  abort();
}
void _log_assert_fail(const char *restrict filename, int line,
                      const char *restrict func,
                      const char *restrict expression, int skip_lock) {
  if (!skip_lock)
    _log_lock();

  _log(LOG_EMERG, filename, line, func, _LOG_ASSERT_MSG, expression);
  _log_abort(filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();
}
void _log_assert_perror_fail(const char *restrict filename, int line,
                             const char *restrict func,
                             const char *restrict expression, int errnum,
                             int skip_lock) {
  if (!skip_lock)
    _log_lock();

  _log_perror(LOG_EMERG, filename, line, func, expression, errnum);
  _log_abort(filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();
}

void _log_perror(int lvl, const char *restrict filename, int line,
                 const char *restrict func, const char *restrict s,
                 int errnum) {
  const char *const restrict __errnodesc = strerrordesc_np(errnum);
  const char *const restrict __errnoname = strerrorname_np(errnum);

  if (!s || !*(char *)s)
    return _log(lvl, filename, line, func, _LOG_PERROR_ARG, __errnodesc,
                __errnoname);
  return _log(lvl, filename, line, func, _LOG_PERROR_S_ARG, s, __errnodesc,
              __errnoname);
}
__attribute((format(printf, 5, 6))) void
_log(int lvl, const char *restrict filename, int line,
     const char *restrict func, const char *fmt, ...) {
  va_list __ap;
  va_start(__ap, fmt);
  _vlog(lvl, filename, line, func, fmt, __ap);
  va_end(__ap);
}

enum { _LOG_LINE_MAX = LOG_LINE_MAX * 2 };
static int _use_syslog;
static const char *restrict _ident;
void _vlog(int lvl, const char *restrict filename, int line,
           const char *restrict func, const char *restrict fmt, va_list ap) {
  static thread_local
      __attribute((tls_model("initial-exec"))) char __buf[_LOG_LINE_MAX];

  if (_use_syslog) {
    snprintf(__buf, _LOG_LINE_MAX, _LOG_SYSLOG_FMT, _LOG_LVL_TO_COLOR[lvl],
             _LOG_LVL_TO_STR[lvl], filename, line, func, fmt);

    vsyslog(lvl, __buf, ap);
  } else {
    snprintf(__buf, _LOG_LINE_MAX, _LOG_STDERR_FMT, _ident, gettid(),
             _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl], filename, line, func,
             fmt);

    vdprintf(STDERR_FILENO, __buf, ap);
  }
}
static int _backtrace_callback(void *data, uintptr_t pc, const char *filepath,
                               int line, const char *func) {
  const int __lvl = value_cast(data, int);
  if (_use_syslog)
    syslog(__lvl, _LOG_BACKTRACE_SYSLOG_FMT, _LOG_LVL_TO_COLOR[__lvl], func,
           filename(filepath), line, pc);
  else
    dprintf(STDERR_FILENO, _LOG_BACKTRACE_STDERR_FMT, _LOG_LVL_TO_COLOR[__lvl],
            func, filename(filepath), line, pc);
  return 0;
}
void _log_backtrace(int lvl, const char *restrict filename, int line,
                    const char *restrict func, int skip_lock) {
  if (!skip_lock)
    _log_lock();

  if (_use_syslog)
    syslog(lvl, _LOG_SYSLOG_FMT, _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl],
           filename, line, func, _LOG_BACKTRACE_MSG);
  else
    dprintf(STDERR_FILENO, _LOG_STDERR_FMT, _ident, gettid(),
            _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl], filename, line, func,
            _LOG_BACKTRACE_MSG);

  struct backtrace_state *const restrict __state =
      backtrace_create_state(NULL, 0, NULL, NULL);
  backtrace_full(__state, 0, _backtrace_callback, NULL, address_cast(lvl));

  if (!skip_lock)
    _log_unlock();
}

void log_init(const char *restrict ident, int option, int facility,
              int use_syslog) {
  log_deinit();

  const char *const restrict __ident =
      ident ? ident : program_invocation_short_name;

  if (use_syslog) {
    if (option == -1)
      option = LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID;
    if (facility == -1)
      facility = LOG_USER;

    openlog(__ident, option, facility);

    _use_syslog = use_syslog;
  }

  _ident = __ident;
}
void log_deinit() {
  /* Disable logging first. */
  log_disable();

  if (_use_syslog) {
    closelog();

    _use_syslog = 0;
  }
}

#endif
