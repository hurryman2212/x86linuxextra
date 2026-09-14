#ifndef __KERNEL__

#include <string.h>

#include <dlfcn.h>

#include <sys/mman.h>

#include <backtrace.h>

#include "sicode_np.h"

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

static char *_progname_copy;
static const char *_ident;
static uint64_t *_log_futexp64;
struct _log_shared {
  uint64_t lock;
  int lvl;
};
static __attribute((constructor(101))) void _log_pre_init() noexcept {
  const int _errno_save = errno;
  const size_t _size = strlen(program_invocation_short_name) + 1;
  _progname_copy = malloc(_size);
  if (unlikely(!_progname_copy)) {
    perror("libx86linuxextra: malloc");
    abort();
  }
  strcpy(_progname_copy, program_invocation_short_name);
  _ident = _progname_copy;

  struct _log_shared *_shared =
      mmap(NULL, sizeof(*_shared), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (unlikely(_shared == MAP_FAILED)) {
    perror("libx86linuxextra: mmap");
    abort();
  }
  _shared->lock = 0;
  _shared->lvl = log_lvl();
  _log_futexp64 = &_shared->lock;
  _log_lvlp = &_shared->lvl;

  const char *_env = getenv("X86LINUX_LOG_LVL");
  if (_env) {
    LOG_INIT();
    log_enable(atoi(_env));
  }

  log(LOG_DEBUG,
      "libx86linuxextra (version %s) by Jihong Min "
      "(hurryman2212@gmail.com)",
      version);

  errno = _errno_save;
}

static void _log_lock() noexcept {
  if (likely(_log_futexp64) &&
      unlikely(usersched_plock(_log_futexp64,
                               USERSCHED_RESTART | USERSCHED_NOEAGAIN,
                               100 * usersched_tsc_1us, NULL, NULL, NULL)))
    _log_assert_perror_fail(FILE_NAME, __LINE__, __func__, "usersched_plock",
                            errno, 1);
}
static void _log_unlock() noexcept {
  if (likely(_log_futexp64) &&
      unlikely(usersched_punlock(_log_futexp64, 0, NULL) == -1))
    _log_assert_perror_fail(FILE_NAME, __LINE__, __func__, "usersched_punlock",
                            errno, 1);
}

void _log_abort(const char *filename, int line, const char *func,
                int skip_lock) noexcept {
  if (!skip_lock)
    _log_lock();

  _log(LOG_EMERG, filename, line, func, _LOG_ABORT_MSG);
  _log_backtrace(LOG_EMERG, filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();

  abort(); // This generates no message on `stdout`/`stderr`.
}
void _log_assert_fail(const char *filename, int line, const char *func,
                      const char *expression, int skip_lock) noexcept {
  if (!skip_lock)
    _log_lock();

  _log(LOG_EMERG, filename, line, func, _LOG_ASSERT_MSG, expression);
  _log_abort(filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();
}
void _log_assert_perror_fail(const char *filename, int line, const char *func,
                             const char *expression, int errnum,
                             int skip_lock) noexcept {
  if (!skip_lock)
    _log_lock();

  _log_perror(LOG_EMERG, filename, line, func, expression, errnum);
  _log_abort(filename, line, func, 1);

  if (!skip_lock)
    _log_unlock();
}

static int _use_syslog;

struct _backtrace_data {
  int lvl;
  const char *filename;
  int line;
  const char *func;
};

static __attribute((nonnull(1))) void
_backtrace_err_cb(void *data, const char *msg, int errnum) noexcept {
  const struct _backtrace_data *_data = data;
  _log(_data->lvl, FILE_NAME, __LINE__, __func__,
       "Backtrace unavailable: %s (%d)", msg ? msg : "unknown error", errnum);
}

static struct backtrace_state *_backtrace_state;
static pthread_once_t _backtrace_once = PTHREAD_ONCE_INIT;
static void _backtrace_init() noexcept {
  static struct _backtrace_data _data = {.lvl = LOG_ERR};
  _backtrace_state = backtrace_create_state(NULL, 1, _backtrace_err_cb, &_data);
}

static __attribute((nonnull(1))) int _backtrace_cb(void *data, uintptr_t pc,
                                                   const char *filepath,
                                                   int line,
                                                   const char *func) noexcept {
  Dl_info _info = {0};
  const int _has_info = dladdr(addr_cast(pc), &_info);
  const void *_saddr = _has_info ? _info.dli_saddr : NULL;
  const uintptr_t _off = _saddr ? pc - val_cast(_saddr) : 0;
  if (!func)
    func = _has_info && _info.dli_sname ? _info.dli_sname : "?";
  if (!filepath)
    filepath = _has_info && _info.dli_fname ? _info.dli_fname : "?";

  const struct _backtrace_data *_data = data;
  const int _lvl = _data->lvl;
  if (_data->filename) {
    if (_off)
      _log(_lvl, _data->filename, _data->line, _data->func,
           "%s+0x%lx(%s:%d) [0x%lx]", func, _off, filename(filepath), line, pc);
    else
      _log(_lvl, _data->filename, _data->line, _data->func,
           _saddr ? "%s(%s:%d) [0x%lx]" : "%s+?(%s:%d) [0x%lx]", func,
           filename(filepath), line, pc);
  } else if (_use_syslog) {
    if (_off)
      syslog(_lvl, _LOG_BACKTRACE_SYSLOG_FMT_OFF, _LOG_LVL_TO_COLOR[_lvl], func,
             _off, filename(filepath), line, pc);
    else
      syslog(_lvl,
             _saddr ? _LOG_BACKTRACE_SYSLOG_FMT
                    : _LOG_BACKTRACE_SYSLOG_FMT_UNKNOWN,
             _LOG_LVL_TO_COLOR[_lvl], func, filename(filepath), line, pc);
  } else {
    if (_off)
      dprintf(STDERR_FILENO, _LOG_BACKTRACE_STDERR_FMT_OFF,
              _LOG_LVL_TO_COLOR[_lvl], func, _off, filename(filepath), line,
              pc);
    else
      dprintf(STDERR_FILENO,
              _saddr ? _LOG_BACKTRACE_STDERR_FMT
                     : _LOG_BACKTRACE_STDERR_FMT_UNKNOWN,
              _LOG_LVL_TO_COLOR[_lvl], func, filename(filepath), line, pc);
  }

  return 0;
}
void _log_backtrace(int lvl, const char *filename, int line, const char *func,
                    int skip_lock) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG))
    return;
  if (!skip_lock)
    _log_lock();

  _log(lvl, filename, line, func, "%s", _LOG_BACKTRACE_MSG);

  struct _backtrace_data _data = {.lvl = lvl};
  pthread_once(&_backtrace_once, _backtrace_init);
  if (_backtrace_state)
    backtrace_full(_backtrace_state, 0, _backtrace_cb, _backtrace_err_cb,
                   &_data);

  if (!skip_lock)
    _log_unlock();
  errno = _errno_save;
}

void _log_backtrace_sig(int lvl, const char *filename, int line,
                        const char *func, int sig, const siginfo_t *info,
                        const void *ctx) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG || log_lvl() < lvl || !info ||
               !ctx))
    return;

  const ucontext_t *_u = ctx;
  const uintptr_t _pc = _u->uc_mcontext.gregs[REG_RIP];

  _log_lock();

  Dl_info _info = {0};
  if (!dladdr(addr_cast(_pc), &_info))
    _info = (Dl_info){0};
  const char *_abbrev = sigabbrev_np(sig);
  const char *_desc = sigdescr_np(sig);
  const char *_code = _sicode_np(sig, info->si_code);
  _log(lvl, filename, line, func,
       "--- SIG%s (%s) {si_code=%d (%s), si_errno=%d, dli_fname=%s, "
       "dli_fbase=%p} ---",
       _abbrev ? _abbrev : "?", _desc ? _desc : "unknown signal", info->si_code,
       _code ? _code : "unknown", info->si_errno,
       _info.dli_fname ? _info.dli_fname : "<unknown>", _info.dli_fbase);

  struct _backtrace_data _data = {.lvl = lvl,
                                  .filename = filename ? filename : "?",
                                  .line = line,
                                  .func = func};
  pthread_once(&_backtrace_once, _backtrace_init);
  if (_backtrace_state)
    backtrace_pcinfo(_backtrace_state, _pc, _backtrace_cb, _backtrace_err_cb,
                     &_data);

  _log_backtrace(lvl, filename, line, func, 1);
  _log_unlock();
  errno = _errno_save;
}

void _log_perror(int lvl, const char *filename, int line, const char *func,
                 const char *s, int errnum) noexcept {
  const int _errno_save = errno;
  const char *_errnodesc = strerrordesc_np(errnum);
  const char *_errnoname = strerrorname_np(errnum);
  if (!_errnodesc)
    _errnodesc = "Unknown error";
  if (!_errnoname)
    _errnoname = "?";

  if (!s || !*s)
    _log(lvl, filename, line, func, _LOG_PERROR_ARG, _errnodesc, _errnoname);
  else
    _log(lvl, filename, line, func, _LOG_PERROR_S_ARG, s, _errnodesc,
         _errnoname);
  errno = _errno_save;
}

__attribute((format(printf, 5, 6))) void _log(int lvl, const char *filename,
                                              int line, const char *func,
                                              const char *fmt, ...) noexcept {
  va_list _ap;
  va_start(_ap, fmt);
  _vlog(lvl, filename, line, func, fmt, _ap);
  va_end(_ap);
}

enum { _LOG_LINE_MAX = LOG_LINE_MAX * 2 };
__attribute((format(printf, 5, 0))) void _vlog(int lvl, const char *filename,
                                               int line, const char *func,
                                               const char *fmt,
                                               va_list ap) noexcept {
  const int _errno_save = errno;
  if (unlikely(lvl < LOG_EMERG || lvl > LOG_DEBUG))
    return;
  char _buf[_LOG_LINE_MAX];
  if (vsnprintf(_buf, sizeof(_buf), fmt, ap) < 0) {
    errno = _errno_save;
    return;
  }

  if (_use_syslog) {
    syslog(lvl, _LOG_SYSLOG_FMT, _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl],
           filename ? filename : "?", line, func ? func : "?", _buf);
  } else {
    dprintf(STDERR_FILENO, _LOG_STDERR_FMT, _ident, gettid(),
            _LOG_LVL_TO_COLOR[lvl], _LOG_LVL_TO_STR[lvl],
            filename ? filename : "?", line, func ? func : "?", _buf);
  }
  errno = _errno_save;
}

void log_init(const char *ident, int option, int facility,
              int use_syslog) noexcept {
  const int _errno_save = errno;
  log_deinit();

  const char *_new_ident = ident ? ident : _progname_copy;

  if (use_syslog) {
    if (option == -1)
      option = LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID;
    if (facility == -1)
      facility = LOG_USER;

    openlog(_new_ident, option, facility);

    _use_syslog = use_syslog;
  }

  _ident = _new_ident;
  errno = _errno_save;
}
void log_deinit() noexcept {
  const int _errno_save = errno;
  /* Disable logging first. */
  log_disable();

  if (_use_syslog) {
    closelog();

    _use_syslog = 0;
  }
  _ident = _progname_copy;
  errno = _errno_save;
}

#endif
