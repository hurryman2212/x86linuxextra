#include "x86linux/helper.h"

#include <errno.h>
#include <stdio.h>

#include <unistd.h>

#include <execinfo.h>

int log_enabled = -1;

static int log_syslog_enabled = 0;
static const char *self_ident = NULL;

static const char *LEVEL_TO_COLOR[] = {
    "\e[1;41;37m", "\e[1;101m", "\e[101m", "\e[1;31m",
    "\e[1;33m",    "\e[1;34m",  "\e[34m",  "\e[2m",
};
static const char *LEVEL_TO_STR[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG",
};
#define LOG_STDERR_FMT "%s[%d]: %s%s: %s:%d: %s: %s\e[0m\n"
#define LOG_STDERR_OBFUS_FMT "%s[%d]: %s%s: %s\e[0m\n"
#define LOG_SYSLOG_FMT "%s%s: %s:%d: %s: %s\e[0m"
#define LOG_SYSLOG_OBFUS_FMT "%s%s: %s\e[0m"
#define LOG_BACKTRACE_MSG "Call Trace:"
#define LOG_ASSERT_MSG "Assertion `%s' failed."
#define LOG_ABORT_MSG "Aborted."

void _log_assert_fail(const char *filename, int line, const char *func,
                      const char *expression) {
  _log(LOG_EMERG, filename, line, func, LOG_ASSERT_MSG, expression);
  _log_abort(filename, line, func);
}
void _log_assert_perror_fail(const char *filename, int line, const char *func,
                             const char *expression, int errnum) {
  _log_perror(LOG_EMERG, filename, line, func, expression, errnum);
  _log_abort(filename, line, func);
}
void _log_abort(const char *filename, int line, const char *func) {
  _log(LOG_EMERG, filename, line, func, LOG_ABORT_MSG);
  _log_backtrace(LOG_EMERG, filename, line, func);
  abort();
}

void _log_perror(int level, const char *filename, int line, const char *func,
                 const char *s, int errnum) {
  if (errnum == -1)
    errnum = errno;
  const char *errnumdesc = strerrordesc_np(errnum);

  if (!s || !*(char *)s)
    return _log(level, filename, line, func, "%s", errnumdesc);
  return _log(level, filename, line, func, "%s: %s", s, errnumdesc);
}
void _log(int level, const char *filename, int line, const char *func,
          const char *fmt, ...) {
  if (unlikely(log_enabled >= level)) {
    va_list ap;
    va_start(ap, fmt);
    _vlog(level, filename, line, func, fmt, ap);
    va_end(ap);
  }
}
static _Thread_local char log_buf[LOG_LINE_MAX];
void _vlog(int level, const char *filename, int line, const char *func,
           const char *fmt, va_list ap) {
  if (unlikely(log_enabled >= level)) {
    if (log_syslog_enabled) {
      if (filename && func)
        snprintf(log_buf, sizeof(log_buf), LOG_SYSLOG_FMT,
                 LEVEL_TO_COLOR[level], LEVEL_TO_STR[level], filename, line,
                 func, fmt);
      else
        snprintf(log_buf, sizeof(log_buf), LOG_SYSLOG_OBFUS_FMT,
                 LEVEL_TO_COLOR[level], LEVEL_TO_STR[level], fmt);

      vsyslog(level, log_buf, ap);
    } else {
      if (filename && func)
        snprintf(log_buf, sizeof(log_buf), LOG_STDERR_FMT,
                 (self_ident ? self_ident : program_invocation_short_name),
                 usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
                 filename, line, func, fmt);
      else
        snprintf(log_buf, sizeof(log_buf), LOG_STDERR_OBFUS_FMT,
                 (self_ident ? self_ident : program_invocation_short_name),
                 usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
                 fmt);

      vdprintf(STDERR_FILENO, log_buf, ap);
    }
  }
}

static _Thread_local void *backtrace_buf[LOG_BACKTRACE_MAX];
void _log_backtrace(int level, const char *filename, int line,
                    const char *func) {
  if (unlikely(log_enabled >= level)) {
    int size = backtrace(backtrace_buf, LOG_BACKTRACE_MAX);

    char **strings;
    strings = backtrace_symbols(backtrace_buf, size);

    if (log_syslog_enabled) {
      if (filename && func)
        syslog(level, LOG_SYSLOG_FMT, LEVEL_TO_COLOR[level],
               LEVEL_TO_STR[level], filename, line, func, LOG_BACKTRACE_MSG);
      else
        syslog(level, LOG_SYSLOG_OBFUS_FMT, LEVEL_TO_COLOR[level],
               LEVEL_TO_STR[level], LOG_BACKTRACE_MSG);

      for (int i = 0; i < size; i++)
        syslog(level, " %s%s\e[0m", LEVEL_TO_COLOR[level], strings[i]);
    } else {
      if (filename && func)
        dprintf(STDERR_FILENO, LOG_STDERR_FMT,
                (self_ident ? self_ident : program_invocation_short_name),
                usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
                filename, line, func, LOG_BACKTRACE_MSG);
      else
        dprintf(STDERR_FILENO, LOG_STDERR_OBFUS_FMT,
                (self_ident ? self_ident : program_invocation_short_name),
                usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
                LOG_BACKTRACE_MSG);

      for (int i = 0; i < size; i++)
        dprintf(STDERR_FILENO, " %s%s\e[0m\n", LEVEL_TO_COLOR[level],
                strings[i]);
    }

    free(strings);
  }
}

void log_init(const char *ident, int option, int facility, int broadcast_stderr,
              int broadcast_syslog) {
  /* Destruct old syslog connection. */
  log_deinit();

  if (broadcast_syslog) {
    if (!ident)
      ident = program_invocation_short_name;
    if (!option)
      option =
          LOG_PID | LOG_CONS | LOG_NDELAY | (broadcast_stderr ? LOG_PERROR : 0);
    if (facility == -1) // Should we allow LOG_KERN (== 0) here?
      facility = LOG_USER;

    openlog(ident, option, facility);
    log_syslog_enabled = broadcast_syslog;
  }
}
void log_deinit(void) {
  /* Disable logging first. */
  log_disable();

  if (log_syslog_enabled) {
    closelog();
    log_syslog_enabled = 0;
  }
}
