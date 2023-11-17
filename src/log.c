#include "x86linux/helper.h"

#include <execinfo.h>

int log_enabled = 0;

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
#define LOG_SYSLOG_FMT "%s%s: %s:%d: %s: %s\e[0m"
#define LOG_BACKTRACE_MSG "Call Trace:"

void _log_perror(const char *filename, int line, const char *func,
                 const char *s) {
  if (!s || !*(char *)s)
    return _log(LOG_ERR, filename, line, func, "%s", strerror(errno));
  return _log(LOG_ERR, filename, line, func, "%s: %s", s, strerror(errno));
}
void _log(int level, const char *filename, int line, const char *func,
          const char *fmt, ...) {
  if (unlikely(log_enabled)) {
    va_list ap;
    va_start(ap, fmt);
    _vlog(level, filename, line, func, fmt, ap);
    va_end(ap);
  }
}
static _Thread_local char log_buf[LOG_BUFSIZ];
void _vlog(int level, const char *filename, int line, const char *func,
           const char *fmt, va_list ap) {
  if (unlikely(log_enabled)) {
    if (log_syslog_enabled) {
      snprintf(log_buf, sizeof(log_buf), LOG_SYSLOG_FMT, LEVEL_TO_COLOR[level],
               LEVEL_TO_STR[level], filename, line, func, fmt);

      vsyslog(level, log_buf, ap);
    } else {
      snprintf(log_buf, sizeof(log_buf), LOG_STDERR_FMT,
               (self_ident ? self_ident : program_invocation_short_name),
               usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
               filename, line, func, fmt);

      vdprintf(STDERR_FILENO, log_buf, ap);
    }
  }
}

static _Thread_local void *backtrace_buf[LOG_BACKTRACE_MAX];
void _log_backtrace(int level, const char *filename, int line,
                    const char *func) {
  if (unlikely(log_enabled)) {
    int size = backtrace(backtrace_buf, LOG_BACKTRACE_MAX);

    char **strings;
    log_perror_assert(strings = backtrace_symbols(backtrace_buf, size));

    if (log_syslog_enabled) {
      syslog(level, LOG_SYSLOG_FMT, LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
             filename, line, func, LOG_BACKTRACE_MSG);
      for (int i = 0; i < size; i++)
        syslog(level, " %s", strings[i]);
    } else {
      dprintf(STDERR_FILENO, LOG_STDERR_FMT,
              (self_ident ? self_ident : program_invocation_short_name),
              usersched_gettid(), LEVEL_TO_COLOR[level], LEVEL_TO_STR[level],
              filename, line, func, LOG_BACKTRACE_MSG);
      for (int i = 0; i < size; i++)
        dprintf(STDERR_FILENO, " %s", strings[i]);
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
