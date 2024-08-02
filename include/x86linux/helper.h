#pragma once

#ifndef __KERNEL__

/* [Userspace] BEGIN */

#ifdef __cplusplus
#include <cassert>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#else
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>

#include <threads.h>
#endif

#include <sys/syslog.h>
#if !defined(_SYS_SYSLOG_H)
#error !defined(_SYS_SYSLOG_H)
#endif
#include <sys/user.h>

#include <linux/futex.h>

#include <x86intrin.h>

/* [Userspace] END */

#else

/* [Kernel] BEGIN */

#include <linux/pci.h>
#include <linux/version.h>

/* [Kernel] END */

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* [Common] BEGIN */

/* Compatibility */

#if defined(__GNUC__) && !defined(__clang__) && !defined(__ICC) &&             \
    !defined(__CUDACC__) && !defined(__LCC__)
#define CATCH_COMPILER_GCC
#endif

#ifdef CATCH_COMPILER_GCC
#define _Nonnull
#define _Nullable
#define _Null_unspecified
#endif

#ifdef __cplusplus
/* See https://en.wikipedia.org/wiki/Restrict#Support_by_C++_compilers. */
#define restrict __restrict
#endif

/* Static branch prediction hints */

#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

/* Stringification macros */

#define xstr(s) str(s)
#define str(s) #s

/* Alignment */

#define align(val, target)                                                     \
  ((((val) / (target)) + !!((val) % (target))) * (target))
#define align_pow2(val, target_pow2)                                           \
  (((val) + ((target_pow2) - 1)) & ~((target_pow2) - 1))
#if !defined(PAGE_SIZE)
#error !defined(PAGE_SIZE)
#endif
/* It uses compile-time definition of `PAGE_SIZE`. */
#define align_page(val) align_pow2(val, PAGE_SIZE)

/* Casting */

#define address_cast(value) ((void *)(uintptr_t)(value))
#define value_cast(address, ...) _value_cast(address, ##__VA_ARGS__, uintptr_t)
#define _value_cast(address, type, ...) ((type)(uintptr_t)(address))

#define elemof(type, var, elem, ...) _elemof(type, var, elem, ##__VA_ARGS__, .)
#define _elemof(type, var, elem, op, ...) (((type)var)op elem)

#define sizeof_elem(type_or_var, elem, ...)                                    \
  sizeof(__VA_ARGS__(elemof(typeof(type_or_var) *, 0, elem, ->)))
#define typeof_elem(type_or_var, elem, ...)                                    \
  typeof(__VA_ARGS__(elemof(typeof(type_or_var) *, 0, elem, ->)))

/* File name */

static __always_inline __attribute((pure)) const char *
filename(const char *restrict path) {
  if (path) {
    const char *const restrict __basename = __builtin_strrchr(path, '/');
    return likely(__basename) ? __basename + 1 : path;
  }
  return path;
}
static __always_inline __attribute((pure)) const char *
filenameat(const char *restrict dirpath, const char *restrict path) {
  if (likely(dirpath && path)) {
    const char *const restrict __relpath = __builtin_strstr(path, dirpath);
    return likely(__relpath) ? __relpath + __builtin_strlen(dirpath) : path;
  }
  return path;
}
#ifdef __DIR__
#define __filename__ filenameat(__DIR__, __FILE__)
#else
#define __filename__ filename(__FILE__)
#endif

/* get_pc() */

#define get_pc()                                                               \
  ({                                                                           \
    volatile uintptr_t __pc;                                                   \
    asm volatile("lea (%%rip), %0" : "=r"(__pc));                              \
    __pc;                                                                      \
  })

/* has_signel_bit() */

#define has_single_bit(x) ((x) && !((x) & ((x) - 1)))

/* memvcmp() */

static __always_inline __attribute((pure)) int
memvcmp(const void *restrict s, unsigned char c, size_t n) {
  if (likely(n)) {
    const unsigned char *const restrict __s = s;
    const unsigned char __r = *__s - c;
    return __r ? __r : __builtin_memcmp(__s + 1, __s, n - 1);
  }
  return 0;
}

/* Bitset operations */

typedef uint64_t bitset_t;
#define BITS_PER_BITSET (8 * sizeof(bitset_t))
/* Convert the number of bits to the length of `bitset_t` type. */
#define BITSET_LEN(nr_bits)                                                    \
  (((nr_bits) / BITS_PER_BITSET) + !!((nr_bits) % BITS_PER_BITSET))
/* Convert the number of bits to the size (in byte(s)) of `bitset_t` type. */
#define BITSET_SIZE(nr_bits) (sizeof(bitset_t) * BITSET_LEN(nr_bits))

unsigned char bitset_test(const bitset_t *restrict bitset32, uint32_t idx);

unsigned char bitset_set(bitset_t *restrict bitset32, uint32_t idx);
static __always_inline unsigned char
bitset_set_atomic(volatile bitset_t *restrict bitset32, uint32_t idx) {
  register unsigned char __cf asm("al");
  asm volatile("lock btsl %2, %1\n\t"
               "setc %0"
               : "=a"(__cf), "+m"(*bitset32)
               : "Ir"(idx)
               : "cc", "memory"); // This includes full memory barrier.
  return __cf;
}
unsigned char bitset_unset(bitset_t *restrict bitset32, uint32_t idx);
static __always_inline unsigned char
bitset_unset_atomic(volatile bitset_t *restrict bitset32, uint32_t idx) {
  register unsigned char __cf asm("al");
  asm volatile("lock btrl %2, %1\n\t"
               "setc %0"
               : "=a"(__cf), "+m"(*bitset32)
               : "Ir"(idx)
               : "cc", "memory"); // This includes full memory barrier.
  return __cf;
}

int32_t bitset_search_lowest(const bitset_t *restrict bitset,
                             uint32_t start_idx, uint32_t last_idx);
int32_t bitset_search_lowest_common(const bitset_t *restrict bitset,
                                    const bitset_t *restrict bitset2,
                                    uint32_t start_idx, uint32_t last_idx);

/* Logger */

/* The default value is LOG_DISABLED. */
extern int _log_lvl;
#define LOG_DISABLED (-1)
#define log_lvl() (_log_lvl)
#define log_enable(lvl) (_log_lvl = lvl)
#define log_disable() (_log_lvl = LOG_DISABLED)

/* SPSC (free size) */

uint32_t spsc_read_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                        uint32_t size);
uint32_t spsc_write_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                         uint32_t size);

/* Currently, it includes full memory barrier. */
uint32_t spsc_read(const void *restrict buf, void *restrict dest,
                   uint32_t *restrict pos_r, uint32_t size);
/* Currently, it includes full memory barrier. */
uint32_t spsc_write(void *restrict buf, const void *restrict src,
                    uint32_t *restrict pos_w, uint32_t size);

/* If rewinded, return 1. Otherwise, return 0. */
int spsc_rewind_read(uint32_t pos_start, uint32_t *restrict pos_r,
                     uint32_t pos_w, uint32_t pos_end);
/* If rewinded, return 1. Otherwise, return 0. */
int spsc_rewind_write(uint32_t pos_start, uint32_t pos_r,
                      uint32_t *restrict pos_w, uint32_t pos_end);

/* x86 CPUID */

static __always_inline void x86_cpuid(uint32_t *restrict eax,
                                      uint32_t *restrict ebx,
                                      uint32_t *restrict ecx,
                                      uint32_t *restrict edx) {
  uint32_t __ebx, __ecx, __edx;
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(__ebx), "=c"(__ecx), "=d"(__edx)
               : "a"(*eax));
  if (ebx)
    *ebx = __ebx;
  if (ecx)
    *ecx = __ecx;
  if (edx)
    *edx = __edx;
}
static __always_inline void x86_cpuidex(uint32_t *restrict eax,
                                        uint32_t *restrict ebx,
                                        uint32_t *restrict ecx,
                                        uint32_t *restrict edx) {
  uint32_t __ebx, __edx;
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(__ebx), "=c"(*ecx), "=d"(__edx)
               : "a"(*eax), "c"(*ecx));
  if (ebx)
    *ebx = __ebx;
  if (edx)
    *edx = __edx;
}

/* [Common] END */

#ifndef __KERNEL__

/* [Userspace] BEGIN */

/* Compatibility */

#ifndef __cplusplus
#if !defined(static_assert)
#error !defined(static_assert)
#endif
#undef static_assert
#define static_assert(expr, ...) _static_assert(expr, ##__VA_ARGS__, #expr)
#define _static_assert(expr, msg, ...) _Static_assert(expr, msg)
#endif

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(fallthrough)
#define fallthrough [[fallthrough]]
#endif
#endif
#ifndef fallthrough
#if __has_attribute(__fallthrough__)
#define fallthrough __attribute((__fallthrough__))
#else
#define fallthrough
#endif
#endif

/* Full memory barrier */

#define barrier() asm volatile("" : : : "memory")

/* Logger */

/*
 * Prepare the logging system; You still need to call log_enable() after.
 *
 * Currently, it is not MT/AS-safe.
 */
void log_init(const char *restrict ident, int option, int facility,
              int broadcast_syslog);
#define LOG_INIT() log_init(NULL, -1, -1, 0)
/*
 * Disable the logging system and and destruct it.
 *
 * Currently, it is not MT/AS-safe.
 */
void log_deinit();

#if !defined(LINE_MAX)
#error !defined(LINE_MAX)
#endif
/*
 * Half of limit that log*()/vlog*() can print at once
 * (this includes '\0' and logging prefixes)
 */
#define LOG_LINE_MAX LINE_MAX

__attribute((format(printf, 5, 6))) void
_log(int lvl, const char *restrict filename, int line,
     const char *restrict func, const char *restrict fmt, ...);
void _vlog(int lvl, const char *restrict filename, int line,
           const char *restrict func, const char *restrict fmt, va_list ap);
#define log(lvl, fmt, ...)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      _log(lvl, __filename__, __LINE__, __func__, fmt, ##__VA_ARGS__);         \
  })
#define vlog(lvl, fmt, ap)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      _vlog(lvl, __filename__, __LINE__, __func__, fmt, ap);                   \
  })
#ifndef NDEBUG
#define trace(lvl, fmt, ...) log(lvl, fmt, ##__VA_ARGS__)
#define vtrace(lvl, fmt, ap) vlog(lvl, fmt, ap)
#else
#define trace(lvl, fmt, ...)
#define vtrace(lvl, fmt, ap)
#endif

void _log_backtrace(int lvl, const char *restrict filename, int line,
                    const char *restrict func, int skip_lock);
#define log_backtrace(lvl)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      _log_backtrace(lvl, __filename__, __LINE__, __func__, 0);                \
  })
#ifndef NDEBUG
#define trace_backtrace(lvl) log_backtrace(lvl)
#else
#define trace_backtrace(lvl)
#endif

#define log_call(lvl, ret_fmt, func, param_fmt, ...)                           \
  ({                                                                           \
    const typeof(func(__VA_ARGS__)) __log_ret = func(__VA_ARGS__);             \
    log(lvl, #func "(" param_fmt ") = " ret_fmt, ##__VA_ARGS__, __log_ret);    \
    typeof(func(__VA_ARGS__)) __log_ret2;                                      \
    __log_ret2 = __log_ret;                                                    \
  })
#ifndef NDEBUG
#define trace_call(lvl, ret_fmt, func, param_fmt, ...)                         \
  log_call(lvl, ret_fmt, func, param_fmt, ##__VA_ARGS__)
#else
#define trace_call(lvl, ret_fmt, func, param_fmt, ...)
#endif

#if !defined(errno)
#error !defined(errno)
#endif
void _log_perror(int lvl, const char *restrict filename, int line,
                 const char *restrict func, const char *restrict s, int errnum);
#define log_perror(lvl, s)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      _log_perror(lvl, __filename__, __LINE__, __func__, s, errno);            \
  })
#ifndef NDEBUG
#define trace_perror(lvl, s) log_perror(lvl, s)
#else
#define trace_perror(lvl, s)
#endif

/* perror() when `expr` is evaluated as 0. */
#define log_perror_false(lvl, expr)                                            \
  ({                                                                           \
    if (unlikely(!(expr)))                                                     \
      log_perror(lvl, #expr);                                                  \
  })
/* perror() when `expr` is evaluated >0 and <256. */
#define log_perror_errno(lvl, expr)                                            \
  ({                                                                           \
    uint8_t __log_e = value_cast(expr, sizeof(__log_e));                       \
    if (unlikely(__log_e))                                                     \
      _log_perror(lvl, __filename__, __LINE__, __func__, #expr, __log_e);      \
  })
/*
 * perror() when `errno` is set.
 * (this clears `errno` value before evaluating `expr`)
 */
#define log_perror_error(lvl, expr)                                            \
  ({                                                                           \
    errno = 0;                                                                 \
    expr;                                                                      \
    if (unlikely(errno))                                                       \
      log_perror(lvl, #expr);                                                  \
  })
#ifndef NDEBUG
#define trace_perror_false(lvl, expr) log_perror_false(lvl, expr)
#define trace_perror_errno(lvl, expr) log_perror_errno(lvl, expr)
#define trace_perror_error(lvl, expr) log_perror_error(lvl, expr)
#else
#define trace_perror_false(lvl, expr)
#define trace_perror_errno(lvl, expr)
#define trace_perror_error(lvl, expr)
#endif

void _log_abort(const char *restrict filename, int line,
                const char *restrict func, int skip_lock);
#define log_abort() _log_abort(__filename__, __LINE__, __func__, 0)

void _log_assert_fail(const char *restrict filename, int line,
                      const char *restrict func, const char *restrict expr,
                      int skip_lock);
void _log_assert_perror_fail(const char *restrict filename, int line,
                             const char *restrict func,
                             const char *restrict expr, int errnum,
                             int skip_lock);
/* abort() when `expr` is evaluated as 0. */
#define log_verify(expr)                                                       \
  ({                                                                           \
    if (unlikely(!(expr)))                                                     \
      _log_assert_fail(__filename__, __LINE__, __func__, #expr, 0);            \
  })
/* abort() when `expr` is evaluated >0 and <256. */
#define log_verify_errno(expr)                                                 \
  ({                                                                           \
    int __log_e = value_cast(expr, int);                                       \
    if (unlikely(__log_e))                                                     \
      _log_assert_perror_fail(__filename__, __LINE__, __func__, #expr,         \
                              __log_e, 0);                                     \
  })
/*
 * abort() when `errno` is set.
 * (this clears `errno` value before evaluating `expr`)
 */
#define log_verify_error(expr)                                                 \
  ({                                                                           \
    errno = 0;                                                                 \
    expr;                                                                      \
    if (unlikely(errno))                                                       \
      _log_assert_perror_fail(__filename__, __LINE__, __func__, #expr, errno,  \
                              0);                                              \
  })
#ifndef NDEBUG
#define trace_assert(expr) log_verify(expr)
#define trace_assert_errno(expr) log_verify_errno(expr)
#define trace_assert_error(expr) log_verify_error(expr)
#else
#define trace_assert(expr)
#define trace_assert_errno(expr)
#define trace_assert_error(expr)
#endif

/* Cached getpid() and gettid() */

pid_t cached_getpid();
#define getpid() cached_getpid()
pid_t cached_gettid();
#define gettid() cached_gettid()

/* x86 UMWAIT/TPAUSE */

static __always_inline unsigned char
X86_UMWAIT(const volatile uint32_t *restrict uaddr32_wb, uint32_t oldval32,
           uint32_t control, uint64_t tsc) {
  if (*uaddr32_wb == oldval32)
    return 0;
  _umonitor((void *)uaddr32_wb);
  return *uaddr32_wb == oldval32 ? _umwait(control, tsc) : 0;
}

/* Userspace scheduler */

/*
 * Setup global variables for 1us TSC value and UMWAIT support.
 *
 * It always returns 0, but it may return -1 if the operation is in progress.
 */
int usersched_init(int inhibit_umwait);

/*
 * Boolean of Invariant TSC support on this system set by usersched_init() call
 *
 * (default value is 0 (unavailable))
 */
extern int usersched_support_invariant_tsc;
/*
 * Boolean of UMWAIT support on this system set by usersched_init() call
 *
 * (default value is 0 (unavailable))
 */
extern int usersched_support_umwait;

/*
 * TSC frequency (TSC per second) on this system set by usersched_init() call
 *
 * (default value is 1000 * 1000 * 1000, which means 1GHz)
 */
extern uint64_t usersched_tsc_freq_hz;
/*
 * TSC per 1us on this set by usersched_init() call
 *
 * ((TSC per us) = (TSC freq. = TSC per sec.) / 10^6)
 * */
extern uint32_t usersched_tsc_1us;

/*
 * Return absolute TSC value referring timeout
 *
 * If timeout_tsc == 0, return 0.
 * If timeout_tsc == UINT32_MAX, return UINT64_MAX.
 */
unsigned long long _user_schedule_start(uint32_t timeout_tsc);

/*
 * Return updated 32-bit timeout TSC value
 *
 * If abs_timeout_tsc == UINT64_MAX, return UINT32_MAX.
 */
uint32_t _user_update_timeout_tsc(unsigned long long abs_timeout_tsc);

uint32_t _user_reschedule(unsigned long long abs_timeout_tsc, uint32_t oldval32,
                          const volatile uint32_t *restrict uaddr32);

enum {
  USERSCHED_COND_BREAK,
  USERSCHED_COND_ONESHOT = 0,
  USERSCHED_COND_SCHEDULE,
  USERSCHED_COND_CONTINUE,
};

/*
 * Userspace scheduler
 *
 * Do NOT use `break` or `continue` below this macro in same scope!
 *
 * @param timeout_tsc (uint32_t) Relative timeout TSC (UINT32_MAX if indefinite)
 * @param init_cond (uint32_t) Initial conditional value
 */
#define user_schedule(timeout_tsc, init_cond)                                  \
  {                                                                            \
    uint32_t __usersched_cond = init_cond;                                     \
    typeof(_user_schedule_start(timeout_tsc)) __usersched_abs_timeout_tsc =    \
        _user_schedule_start(timeout_tsc);                                     \
    do

#define user_cond_set(cond) (__usersched_cond = cond)

#if defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#error defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#endif

static __always_inline unsigned char user_pause(uint32_t control,
                                                uint64_t tsc) {
#ifndef _NO_UMWAIT
  if (likely(tsc)
#ifndef _FORCE_UMWAIT
      && usersched_support_umwait
#endif
  )
    return _tpause(control, tsc);
#endif
  _mm_pause();
  return 0;
}
static __always_inline unsigned char
user_wait(const volatile uint32_t *restrict uaddr32, uint32_t oldval32,
          uint32_t control, uint64_t tsc) {
#ifndef _NO_UMWAIT
  if (likely(uaddr32 && tsc)
#ifndef _FORCE_UMWAIT
      && usersched_support_umwait
#endif
  )
    return X86_UMWAIT(uaddr32, oldval32, control, tsc);
#endif
  _mm_pause();
  return 0;
}

/*
 * Userspace rescheduler
 *
 * Do NOT use `break` or `continue` above this macro in same scope!
 *
 * @param timeout_tscp (uint32_t *restrict) Pointer to store deducted
 * relative timeout TSC (UINT32_MAX will be stored if indefinite)
 * @param uaddr32 (const volatile uint32_t *restrict) Address to snoop
 * (if UMWAIT support is present) and compare (will NOT be compared if NULL)
 * @param oldval32 (uint32_t) Original value stored at non-NULL uaddr32
 * (timeout will NOT happen if *uaddr32 != oldval32)
 */
#define user_reschedule(timeout_tscp, uaddr32, oldval32)                       \
  while (                                                                      \
      __usersched_cond == 1                                                    \
          ? (__usersched_cond =                                                \
                 (uaddr32) && *(const volatile uint32_t *restrict)(uaddr32) != \
                                  (uint32_t)(oldval32)                         \
                     ? 1                                                       \
                     : _user_reschedule(__usersched_abs_timeout_tsc, oldval32, \
                                        uaddr32))                              \
          : (__usersched_cond ? --__usersched_cond : 0))                       \
    ;                                                                          \
  if ((uintptr_t)(timeout_tscp))                                               \
    *(uint32_t *restrict)(timeout_tscp) =                                      \
        _user_update_timeout_tsc(__usersched_abs_timeout_tsc);                 \
  }                                                                            \
  (void)0

#define USERSCHED_RESTART 1
static_assert(FUTEX_PRIVATE_FLAG <= UINT32_MAX);
static_assert(SA_RESTART <= UINT32_MAX);
static_assert(USERSCHED_RESTART <= UINT32_MAX);
static_assert(FUTEX_PRIVATE_FLAG != SA_RESTART);
static_assert(SA_RESTART != USERSCHED_RESTART);
static_assert(USERSCHED_RESTART != FUTEX_PRIVATE_FLAG);
static_assert(has_single_bit(FUTEX_PRIVATE_FLAG));
static_assert(has_single_bit(SA_RESTART));
static_assert(has_single_bit(USERSCHED_RESTART));
int usersched_lock(volatile uint64_t *restrict lock64, int flags,
                   uint32_t user_timeout_tsc,
                   const struct timespec *restrict kernel_timeout);
int usersched_unlock(volatile uint64_t *restrict lock64, int flags);
int usersched_lock_pi(volatile uint32_t *restrict lock, int flags,
                      uint32_t user_timeout_tsc,
                      const struct timespec *restrict kernel_timeout);
int usersched_unlock_pi(volatile uint32_t *restrict lock, int flags);

extern const sigset_t _fset;
extern thread_local __attribute((tls_model("initial-exec"))) sigset_t _oset;
/* Enter AS-safe critical section. */
#define as_enter()                                                             \
  log_verify_errno(pthread_sigmask(SIG_SETMASK, &_fset, &_oset))
/* Exit AS-safe critical section. */
#define as_exit() log_verify_errno(pthread_sigmask(SIG_SETMASK, &_oset, NULL))
/*
 * If `set` or `oldset` is NULL, internal variable will be used.
 *
 * When `set` is NULL, all signal will be blocked for the thread.
 * (this calls pthread_sigmask(SIG_SETMASK, ...) internally)
 */
int usersched_plock(volatile uint64_t *restrict lock64, int flags,
                    uint32_t user_timeout_tsc,
                    const struct timespec *restrict kernel_timeout,
                    const sigset_t *restrict set, sigset_t *restrict oldset);
/* If `oldset` is NULL, internal variable will be used. */
int usersched_punlock(volatile uint64_t *restrict lock64, int flags,
                      const sigset_t *restrict set);
/*
 * If `set` or `oldset` is NULL, internal variable will be used.
 *
 * When `set` is NULL, all signal will be blocked for the thread.
 * (this calls pthread_sigmask(SIG_SETMASK, ...) internally)
 */
int usersched_plock_pi(volatile uint32_t *restrict lock, int flags,
                       uint32_t user_timeout_tsc,
                       const struct timespec *restrict kernel_timeout,
                       const sigset_t *restrict set, sigset_t *restrict oldset);
/* If `oldset` is NULL, internal variable will be used. */
int usersched_punlock_pi(volatile uint32_t *restrict lock, int flags,
                         const sigset_t *restrict set);

/* [Userspace] END */

#else

/* [Kernel] BEGIN */

/* Compatibility */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(name)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_index)
#else
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_attrib.entry_nr)
#endif

/* Types */

typedef s8 int8_t;
#define INT8_MIN S8_MIN
#define INT8_MAX S8_MAX
typedef s16 int16_t;
#define INT16_MIN S16_MIN
#define INT16_MAX S16_MAX
typedef s32 int32_t;
#define INT32_MIN S32_MIN
#define INT32_MAX S32_MAX
typedef s64 int64_t;
#define INT64_MIN S64_MIN
#define INT64_MAX S64_MAX

typedef u8 uint8_t;
#define UINT8_MIN U8_MIN
#define UINT8_MAX U8_MAX
typedef u16 uint16_t;
#define UINT16_MIN U16_MIN
#define UINT16_MAX U16_MAX
typedef u32 uint32_t;
#define UINT32_MIN U32_MIN
#define UINT32_MAX U32_MAX
typedef u64 uint64_t;
#define UINT64_MIN U64_MIN
#define UINT64_MAX U64_MAX

/* Logger */

#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#ifdef MODULE
#define log(lvl, fmt, ...)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH xstr(lvl) "%s[%d]: %s:%d: %s: " fmt "\n",                \
             THIS_MODULE->name, task_pid_vnr(current), __filename__, __LINE__, \
             __func__, ##__VA_ARGS__);                                         \
  })
#else
#define log(lvl, fmt, ...)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH xstr(lvl) "kernel[%d]: %s:%d: %s: " fmt "\n",            \
             task_pid_vnr(current), __filename__, __LINE__, __func__,          \
             ##__VA_ARGS__);                                                   \
  })
#endif
#ifndef NDEBUG
#define trace(lvl, fmt, ...) log(lvl, fmt, ##__VA_ARGS__)
#else
#define trace(lvl, fmt, ...)
#endif

#ifdef MODULE
#define log_pci(pdev, lvl, fmt, ...)                                           \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH xstr(lvl) "%s[%d] %04x:%02hhx:%02x.%x: %s:%d: %s: " fmt  \
                                "\n",                                          \
             THIS_MODULE->name, task_pid_vnr(current),                         \
             pci_domain_nr(pdev->bus), pdev->bus->number,                      \
             PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), __filename__,       \
             __LINE__, __func__, ##__VA_ARGS__);                               \
  })
#else
#define log_pci(pdev, lvl, fmt, ...)                                           \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH xstr(                                                    \
                 lvl) "kernel[%d] %04x:%02hhx:%02x.%x: %s:%d: %s: " fmt "\n",  \
             task_pid_vnr(current), pci_domain_nr(pdev->bus),                  \
             pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),  \
             __filename__, __LINE__, __func__, ##__VA_ARGS__);                 \
  })
#endif
#ifndef NDEBUG
#define trace_pci(pdev, lvl, fmt, ...) trace_pci(pdev, lvl, fmt, ##__VA_ARGS__)
#else
#define trace_pci(pdev, lvl, fmt, ...)
#endif

#endif

/* [Kernel] END */

#ifdef __cplusplus
}
#endif
