#pragma once

#ifndef __KERNEL__

/* [Userspace] BEGIN */

#ifdef __cplusplus
#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#else
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>
#include <threads.h>
#endif

#include <pthread.h>

#include <sys/syslog.h>
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
#define _CATCH_COMPILER_GCC
#endif

#ifdef _CATCH_COMPILER_GCC
#define _Nonnull
#define _Nullable
#define _Null_unspecified
#endif

#ifdef __cplusplus
/* See https://en.wikipedia.org/wiki/Restrict#Support_by_C++_compilers. */
#define restrict __restrict
#define noexcept noexcept
#else
#define noexcept
#endif

/* Match glibc's nonnull annotation in kernel builds as well. */
#ifndef __nonnull
#define __nonnull(params) __attribute((nonnull params))
#endif

/* Static branch prediction hints */

#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

/* Stringification macros */

/*
 * Expand macros in s, then stringify the result.
 * Example:
 *   #define VALUE 123
 *   rstr(VALUE) -> "123"
 */
#define rstr(s) _str(s)

/*
 * Stringify s without macro expansion.
 * Example:
 *   #define VALUE 123
 *   _str(VALUE) -> "VALUE"
 */
#define _str(s) #s

/* Alignment */

#define align_val(val, target)                                                 \
  ((((val) / (target)) + !!((val) % (target))) * (target))
#define align_val_pow2(val, target_pow2)                                       \
  (((val) + ((target_pow2) - 1)) &                                             \
   ~((typeof((val) + (target_pow2)))((target_pow2) - 1)))
#if !defined(PAGE_SIZE)
#error !defined(PAGE_SIZE)
#endif
/* It uses compile-time definition of `PAGE_SIZE`. */
#define align_val_page(val) align_val_pow2(val, PAGE_SIZE)

/* Casting */

#define addr_cast(val) ((void *)(uintptr_t)(val))
#define val_cast(addr, ...) _val_cast(addr, ##__VA_ARGS__, uintptr_t)
#define _val_cast(addr, type, ...) ((type)(uintptr_t)(addr))

#define elemof(type, var, elem, ...) _elemof(type, var, elem, ##__VA_ARGS__, .)
#define _elemof(type, var, elem, op, ...) (((type)var)op elem)

#define sizeof_elem(type_or_var, elem, ...)                                    \
  sizeof(__VA_ARGS__(elemof(typeof(type_or_var) *, 0, elem, ->)))
#define typeof_elem(type_or_var, elem, ...)                                    \
  typeof(__VA_ARGS__(elemof(typeof(type_or_var) *, 0, elem, ->)))

/* File name */

static __always_inline __attribute((pure)) const char *
filename(const char *path) noexcept {
  if (path) {
    const char *_basename = __builtin_strrchr(path, '/');
    return likely(_basename) ? _basename + 1 : path;
  }
  return path;
}
static __always_inline __attribute((pure)) const char *
filenameat(const char *dirpath, const char *path) noexcept {
  if (likely(dirpath && path)) {
    const char *_relpath = __builtin_strstr(path, dirpath);
    return likely(_relpath) ? _relpath + __builtin_strlen(dirpath) : path;
  }
  return path;
}
#ifdef X86LINUX_DIR
#define FILE_NAME filenameat(X86LINUX_DIR, __FILE__)
#else
#define FILE_NAME filename(__FILE__)
#endif

/* get_pc() */

#define get_pc()                                                               \
  ({                                                                           \
    volatile uintptr_t _pc;                                                    \
    asm volatile("lea (%%rip), %0" : "=r"(_pc));                               \
    _pc;                                                                       \
  })

/* has_single_bit() */

#ifdef __cplusplus
static inline constexpr bool has_single_bit(uint64_t x) noexcept {
  return x && !(x & (x - 1));
}
#else
#define has_single_bit(x) ((x) && !((x) & ((x) - 1)))
#endif

/* memvcmp() */

static __always_inline __attribute((pure)) int
memvcmp(const void *s, unsigned char c, size_t n) noexcept {
  if (likely(n)) {
    const unsigned char *_s = (typeof(_s))s;
    const int _r = *_s - c;
    return _r ? _r : __builtin_memcmp(_s + 1, _s, n - !!n);
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

__nonnull((1)) unsigned char bitset_test(const bitset_t *bitset32,
                                         uint32_t idx) noexcept;

__nonnull((1)) unsigned char bitset_set(bitset_t *bitset32,
                                        uint32_t idx) noexcept;
__nonnull((1)) static __always_inline
    unsigned char bitset_set_atomic(volatile bitset_t *bitset32,
                                    uint32_t idx) noexcept {
  register unsigned char _cf asm("al");
  asm volatile("lock btsl %2, %1\n\t"
               "setc %0"
               : "=a"(_cf), "+m"(bitset32[idx >> 6])
               : "Ir"(idx & 63)
               : "cc", "memory"); // This includes full memory barrier.
  return _cf;
}
__nonnull((1)) unsigned char bitset_unset(bitset_t *bitset32,
                                          uint32_t idx) noexcept;
__nonnull((1)) static __always_inline
    unsigned char bitset_unset_atomic(volatile bitset_t *bitset32,
                                      uint32_t idx) noexcept {
  register unsigned char _cf asm("al");
  asm volatile("lock btrl %2, %1\n\t"
               "setc %0"
               : "=a"(_cf), "+m"(bitset32[idx >> 6])
               : "Ir"(idx & 63)
               : "cc", "memory"); // This includes full memory barrier.
  return _cf;
}

/* Return the matching uint32_t index, or -1 if the range has no match. */
int64_t bitset_search_lowest(const bitset_t *bitset, uint32_t start_idx,
                             uint32_t last_idx) noexcept;
int64_t bitset_search_lowest_common(const bitset_t *bitset,
                                    const bitset_t *bitset2, uint32_t start_idx,
                                    uint32_t last_idx) noexcept;

/* SPSC (free size) */

/*
 * One reader and one writer own their respective positions. Load the peer's
 * position with acquire ordering before peek/rewind; pos_end is exclusive.
 * read/write sizes must not exceed the preceding peek result.
 */

uint32_t spsc_read_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                        uint32_t size) noexcept;
uint32_t spsc_write_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                         uint32_t size) noexcept;

/* Currently, it includes full memory barrier. */
__nonnull((1, 2, 3)) uint32_t
    spsc_read(const void *restrict buf, void *restrict dest,
              uint32_t *restrict pos_r, uint32_t size) noexcept;
/* Currently, it includes full memory barrier. */
__nonnull((1, 2, 3)) uint32_t
    spsc_write(void *restrict buf, const void *restrict src,
               uint32_t *restrict pos_w, uint32_t size) noexcept;

/* If rewinded, return 1. Otherwise, return 0. */
__nonnull((2)) int spsc_rewind_read(uint32_t pos_start, uint32_t *pos_r,
                                    uint32_t pos_w, uint32_t pos_end) noexcept;
/* If rewinded, return 1. Otherwise, return 0. */
__nonnull((3)) int spsc_rewind_write(uint32_t pos_start, uint32_t pos_r,
                                     uint32_t *pos_w,
                                     uint32_t pos_end) noexcept;

/* Intrusive circular doubly linked list */

/*
 * Circular intrusive list. The head is a sentinel, not an element. A node may
 * belong to only one list, and its address must remain stable while linked.
 * Callers own storage, synchronization, and the lifetime of every reader.
 */
typedef struct icdlist {
  struct icdlist *prev;
  struct icdlist *next;
} icdlist_t;

#define ICDLIST_INIT(name) {&(name), &(name)}

/* Initialize an empty head or a detached node. */
static inline void icdlist_init(icdlist_t *node) {
  node->prev = node;
  node->next = node;
}

static inline int icdlist_empty(const icdlist_t *head) {
  return head->next == head;
}

/* Insert a detached node. Inserting before the head appends an element. */
static inline void icdlist_insert_before(icdlist_t *pos, icdlist_t *node) {
  node->prev = pos->prev;
  node->next = pos;
  pos->prev->next = node;
  pos->prev = node;
}

/* Inserting after the head prepends an element. */
static inline void icdlist_insert_after(icdlist_t *pos, icdlist_t *node) {
  node->prev = pos;
  node->next = pos->next;
  pos->next->prev = node;
  pos->next = node;
}

/* Detach an element and reset its links. Never remove the head. */
static inline void icdlist_remove(icdlist_t *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  icdlist_init(node);
}

/* Use a standard-layout type in C++; include const in type when needed. */
#define icdlist_entry(node, type, member)                                      \
  ((type *)((char *)(node) - __builtin_offsetof(type, member)))

/* node and saved are caller-declared icdlist_t pointers. */
#define icdlist_for_each(node, head)                                           \
  for ((node) = (head)->next; (node) != (head); (node) = (node)->next)

/* Allows removing the current node, not concurrent mutation or saved removal.
 */
#define icdlist_for_each_safe(node, saved, head)                               \
  for ((node) = (head)->next, (saved) = (node)->next; (node) != (head);        \
       (node) = (saved), (saved) = (node)->next)

/* Logger */

/* Default: LOG_DISABLED. Userspace storage is shared with fork descendants. */
#ifdef __KERNEL__
extern int _log_lvl;
#else
extern int *_log_lvlp;
#define _log_lvl (*_log_lvlp)
#endif
#define LOG_DISABLED (-1)
#define log_lvl() (__atomic_load_n(&_log_lvl, __ATOMIC_RELAXED))
#define log_enable(lvl) _log_enable(lvl)
static __always_inline int _log_enable(int lvl) noexcept {
  __atomic_store_n(&_log_lvl, lvl, __ATOMIC_RELAXED);
  return lvl;
}
#define log_disable() log_enable(LOG_DISABLED)

/* x86 CPUID */

/* Non-NULL register outputs must designate distinct objects. */
__nonnull((1)) static __always_inline
    void x86_cpuid(uint32_t *restrict eax, uint32_t *restrict ebx,
                   uint32_t *restrict ecx, uint32_t *restrict edx) noexcept {
  uint32_t _ebx, _ecx, _edx;
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(_ebx), "=c"(_ecx), "=d"(_edx)
               : "a"(*eax));
  if (ebx)
    *ebx = _ebx;
  if (ecx)
    *ecx = _ecx;
  if (edx)
    *edx = _edx;
}
__nonnull((1, 3)) static __always_inline
    void x86_cpuidex(uint32_t *restrict eax, uint32_t *restrict ebx,
                     uint32_t *restrict ecx, uint32_t *restrict edx) noexcept {
  uint32_t _ebx, _edx;
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(_ebx), "=c"(*ecx), "=d"(_edx)
               : "a"(*eax), "c"(*ecx));
  if (ebx)
    *ebx = _ebx;
  if (edx)
    *edx = _edx;
}

/* [Common] END */

#ifndef __KERNEL__

/* [Userspace] BEGIN */

/* Compatibility */

#ifndef __cplusplus
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

/* Compiler memory barrier (does not issue a CPU fence). */

#define barrier() asm volatile("" : : : "memory")

/* Logger */

/*
 * Prepare the logging system; You still need to call log_enable() after.
 *
 * Currently, it is not MT/AS-safe.
 */
void log_init(const char *ident, int option, int facility,
              int use_syslog) noexcept;
#define LOG_INIT() log_init(NULL, -1, -1, 0)
/*
 * Disable the logging system and and destruct it.
 *
 * Currently, it is not MT/AS-safe.
 */
void log_deinit() noexcept;

#if !defined(LINE_MAX)
#error !defined(LINE_MAX)
#endif
/*
 * Half the message buffer size used by log*()/vlog*(), including '\0'.
 * Logging prefixes are additional to this limit.
 */
#define LOG_LINE_MAX LINE_MAX

__nonnull((5))
    __attribute((format(printf, 5, 6))) void _log(int lvl, const char *filename,
                                                  int line, const char *func,
                                                  const char *fmt,
                                                  ...) noexcept;
__nonnull((5)) __attribute((format(printf, 5, 0))) void _vlog(
    int lvl, const char *filename, int line, const char *func, const char *fmt,
    va_list ap) noexcept;
/*
 * log_message() is the logging entry point without a standard-name macro.
 * The compatibility log() macro preserves one-argument math calls, except
 * unparenthesized commas in braces or template arguments. Use (std::log)(...)
 * for those calls, or define X86LINUX_NO_LOG_MACRO before including this
 * header.
 */
#ifndef X86LINUX_NO_LOG_MACRO
#define log(val, ...)                                                          \
  _log_select(__VA_OPT__(log_message, ) _log_math)(val, ##__VA_ARGS__)
#define _log_select(func, ...) func
#define _log_math(val) log(val)
#endif
#define log_message(lvl, fmt, ...)                                             \
  ({                                                                           \
    const int _log_level = (lvl);                                              \
    if (unlikely(log_lvl() >= _log_level))                                     \
      _log(_log_level, FILE_NAME, __LINE__, __func__, fmt, ##__VA_ARGS__);     \
  })
#define vlog(lvl, fmt, ap)                                                     \
  ({                                                                           \
    const int _log_level = (lvl);                                              \
    if (unlikely(log_lvl() >= _log_level))                                     \
      _vlog(_log_level, FILE_NAME, __LINE__, __func__, fmt, ap);               \
  })
#ifndef NDEBUG
#define trace(lvl, fmt, ...) log_message(lvl, fmt, ##__VA_ARGS__)
#define vtrace(lvl, fmt, ap) vlog(lvl, fmt, ap)
#else
#define trace(lvl, fmt, ...)
#define vtrace(lvl, fmt, ap)
#endif

void _log_backtrace(int lvl, const char *filename, int line, const char *func,
                    int skip_lock) noexcept;
#define log_backtrace(lvl)                                                     \
  ({                                                                           \
    const int _log_level = (lvl);                                              \
    if (unlikely(log_lvl() >= _log_level))                                     \
      _log_backtrace(_log_level, FILE_NAME, __LINE__, __func__, 0);            \
  })
#ifndef NDEBUG
#define trace_backtrace(lvl) log_backtrace(lvl)
#else
#define trace_backtrace(lvl)
#endif

void _log_backtrace_sig(int lvl, const char *filename, int line,
                        const char *func, int sig, const siginfo_t *info,
                        const void *ctx) noexcept;
/* The one-argument form uses the current handler's sig/info/ctx tuple. */
#define log_backtrace_sig(lvl, ...)                                            \
  _log_backtrace_sig_args(lvl, ##__VA_ARGS__, sig, info, ctx)
#define _log_backtrace_sig_args(lvl, sig, info, ctx, ...)                      \
  ({                                                                           \
    const int _log_level = (lvl);                                              \
    if (unlikely(log_lvl() >= _log_level))                                     \
      _log_backtrace_sig(_log_level, FILE_NAME, __LINE__, __func__, sig, info, \
                         ctx);                                                 \
  })

/*
 * Evaluate the function and its arguments once; log argument expressions.
 * param_fmt is retained for source compatibility and is no longer used.
 */
#define log_call(lvl, ret_fmt, func, param_fmt, ...)                           \
  ({                                                                           \
    const typeof(func(__VA_ARGS__)) _log_ret = func(__VA_ARGS__);              \
    log_message(lvl, #func "(%s) = " ret_fmt, #__VA_ARGS__, _log_ret);         \
    typeof(func(__VA_ARGS__)) _log_ret2;                                       \
    _log_ret2 = _log_ret;                                                      \
  })
#ifndef NDEBUG
#define trace_call(lvl, ret_fmt, func, param_fmt, ...)                         \
  log_call(lvl, ret_fmt, func, param_fmt, ##__VA_ARGS__)
#else
#define trace_call(lvl, ret_fmt, func, param_fmt, ...) func(__VA_ARGS__)
#endif

#if !defined(errno)
#error !defined(errno)
#endif
void _log_perror(int lvl, const char *filename, int line, const char *func,
                 const char *s, int errnum) noexcept;
#define log_perror(lvl, s)                                                     \
  ({                                                                           \
    const int _log_level = (lvl);                                              \
    if (unlikely(log_lvl() >= _log_level))                                     \
      _log_perror(_log_level, FILE_NAME, __LINE__, __func__, s, errno);        \
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
/* perror() when `expr` returns a nonzero error number. */
#define log_perror_errno(lvl, expr)                                            \
  ({                                                                           \
    const int _log_e = val_cast(expr, int);                                    \
    if (unlikely(_log_e)) {                                                    \
      const int _log_level = (lvl);                                            \
      if (log_lvl() >= _log_level)                                             \
        _log_perror(_log_level, FILE_NAME, __LINE__, __func__, #expr, _log_e); \
    }                                                                          \
  })
/*
 * perror() when `errno` is set.
 * (this clears `errno` value before evaluating `expr`)
 */
#define log_perror_err(lvl, expr)                                              \
  ({                                                                           \
    errno = 0;                                                                 \
    expr;                                                                      \
    if (unlikely(errno))                                                       \
      log_perror(lvl, #expr);                                                  \
  })
#ifndef NDEBUG
#define trace_perror_false(lvl, expr) log_perror_false(lvl, expr)
#define trace_perror_errno(lvl, expr) log_perror_errno(lvl, expr)
#define trace_perror_err(lvl, expr) log_perror_err(lvl, expr)
#else
#define trace_perror_false(lvl, expr)
#define trace_perror_errno(lvl, expr)
#define trace_perror_err(lvl, expr)
#endif

void _log_abort(const char *filename, int line, const char *func,
                int skip_lock) noexcept;
#define log_abort() _log_abort(FILE_NAME, __LINE__, __func__, 0)

void _log_assert_fail(const char *filename, int line, const char *func,
                      const char *expression, int skip_lock) noexcept;
void _log_assert_perror_fail(const char *filename, int line, const char *func,
                             const char *expression, int errnum,
                             int skip_lock) noexcept;
/* abort() when `expr` is evaluated as 0. */
#define log_verify(expr)                                                       \
  ({                                                                           \
    if (unlikely(!(expr)))                                                     \
      _log_assert_fail(FILE_NAME, __LINE__, __func__, #expr, 0);               \
  })
/* abort() when `expr` is evaluated >0 and <256. */
#define log_verify_errno(expr)                                                 \
  ({                                                                           \
    const int _log_e = val_cast(expr, int);                                    \
    if (unlikely(_log_e))                                                      \
      _log_assert_perror_fail(FILE_NAME, __LINE__, __func__, #expr, _log_e,    \
                              0);                                              \
  })
/*
 * abort() when `errno` is set.
 * (this clears `errno` value before evaluating `expr`)
 */
#define log_verify_err(expr)                                                   \
  ({                                                                           \
    errno = 0;                                                                 \
    expr;                                                                      \
    if (unlikely(errno))                                                       \
      _log_assert_perror_fail(FILE_NAME, __LINE__, __func__, #expr, errno, 0); \
  })
#ifndef NDEBUG
#define trace_assert(expr) log_verify(expr)
#define trace_assert_errno(expr) log_verify_errno(expr)
#define trace_assert_err(expr) log_verify_err(expr)
#else
#define trace_assert(expr)
#define trace_assert_errno(expr)
#define trace_assert_err(expr)
#endif

/* x86 UMWAIT/TPAUSE */

/* Raw instruction helper; user_wait() performs runtime feature checks. */
__nonnull((1)) static inline
    __attribute((target("waitpkg"))) unsigned char _X86_UMWAIT(
        const volatile uint32_t *uaddr32_wb, uint32_t oldval32, uint32_t ctl,
        uint64_t tsc) noexcept {
  if (__atomic_load_n(uaddr32_wb, __ATOMIC_ACQUIRE) != oldval32)
    return 0;
  _umonitor((void *)uaddr32_wb);
  return __atomic_load_n(uaddr32_wb, __ATOMIC_ACQUIRE) == oldval32
             ? _umwait(ctl, tsc)
             : 0;
}

/* Userspace scheduler */

/*
 * Setup global variables for 1us TSC value and UMWAIT support.
 *
 * It always returns 0, but it may return -1 if the operation is in progress.
 */
int usersched_init(int inhibit_umwait) noexcept;

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
unsigned long long _user_schedule_start(uint32_t timeout_tsc) noexcept;

/*
 * Return updated 32-bit timeout TSC value
 *
 * If abs_timeout_tsc == UINT64_MAX, return UINT32_MAX.
 */
uint32_t _user_update_timeout_tsc(unsigned long long abs_timeout_tsc) noexcept;

uint32_t _user_reschedule(unsigned long long abs_timeout_tsc, uint32_t oldval32,
                          const volatile uint32_t *uaddr32) noexcept;

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
    uint32_t _usersched_cond = init_cond;                                      \
    const typeof(_user_schedule_start(                                         \
        timeout_tsc)) _usersched_abs_timeout_tsc =                             \
        _user_schedule_start(timeout_tsc);                                     \
    do

#define user_timeout_tsc() ({ _usersched_abs_timeout_tsc; })

#define user_cond_set(cond) (_usersched_cond = cond)

#if defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#error defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#endif

static inline __attribute((target("waitpkg"))) unsigned char
user_pause(uint32_t ctl, uint64_t tsc) noexcept {
#ifndef _NO_UMWAIT
  if (likely(tsc)
#ifndef _FORCE_UMWAIT
      && usersched_support_umwait
#endif
  )
    return _tpause(ctl, tsc);
#endif
  _mm_pause();
  return 0;
}
static __always_inline unsigned char user_wait(const volatile uint32_t *uaddr32,
                                               uint32_t oldval32, uint32_t ctl,
                                               uint64_t tsc) noexcept {
#ifndef _NO_UMWAIT
  if (likely(uaddr32 && tsc)
#ifndef _FORCE_UMWAIT
      && usersched_support_umwait
#endif
  )
    return _X86_UMWAIT(uaddr32, oldval32, ctl, tsc);
#endif
  _mm_pause();
  return 0;
}

/*
 * Userspace rescheduler
 *
 * Do NOT use `break` or `continue` above this macro in same scope!
 *
 * @param timeout_tscp (uint32_t *) Pointer to store deducted
 * relative timeout TSC (UINT32_MAX will be stored if indefinite)
 * @param uaddr32 (const volatile uint32_t *) Address to snoop
 * (if UMWAIT support is present) and compare (will NOT be compared if NULL)
 * @param oldval32 (uint32_t) Original value stored at non-NULL uaddr32
 * (timeout will NOT happen if *uaddr32 != oldval32)
 */
#define user_reschedule(timeout_tscp, uaddr32, oldval32)                       \
  while (_usersched_cond == 1                                                  \
             ? (_usersched_cond = ({                                           \
                  const volatile uint32_t *_usersched_uaddr32 = (uaddr32);     \
                  (_usersched_uaddr32 != NULL) &&                              \
                          __atomic_load_n(_usersched_uaddr32,                  \
                                          __ATOMIC_ACQUIRE) !=                 \
                              (uint32_t)(oldval32)                             \
                      ? 1                                                      \
                      : _user_reschedule(_usersched_abs_timeout_tsc, oldval32, \
                                         _usersched_uaddr32);                  \
                }))                                                            \
             : (_usersched_cond ? --_usersched_cond : 0))                      \
    ;                                                                          \
  if ((uintptr_t)(timeout_tscp))                                               \
    *(uint32_t *)(timeout_tscp) =                                              \
        _user_update_timeout_tsc(_usersched_abs_timeout_tsc);                  \
  }                                                                            \
  (void)0

#define USERSCHED_RESTART 0x1
#define USERSCHED_NOEAGAIN 0x2

static_assert(has_single_bit(FUTEX_CLOCK_REALTIME));
static_assert(has_single_bit(FUTEX_PRIVATE_FLAG));
static_assert(has_single_bit(SA_RESTART));
static_assert(has_single_bit(USERSCHED_NOEAGAIN));
static_assert(has_single_bit(USERSCHED_RESTART));

static_assert(FUTEX_CLOCK_REALTIME <= UINT32_MAX);
static_assert(FUTEX_PRIVATE_FLAG <= UINT32_MAX);
static_assert(SA_RESTART <= UINT32_MAX);
static_assert(USERSCHED_NOEAGAIN <= UINT32_MAX);
static_assert(USERSCHED_RESTART <= UINT32_MAX);

static_assert(FUTEX_CLOCK_REALTIME != FUTEX_PRIVATE_FLAG);
static_assert(FUTEX_CLOCK_REALTIME != SA_RESTART);
static_assert(FUTEX_CLOCK_REALTIME != USERSCHED_NOEAGAIN);
static_assert(FUTEX_CLOCK_REALTIME != USERSCHED_RESTART);
static_assert(FUTEX_PRIVATE_FLAG != SA_RESTART);
static_assert(FUTEX_PRIVATE_FLAG != USERSCHED_NOEAGAIN);
static_assert(FUTEX_PRIVATE_FLAG != USERSCHED_RESTART);
static_assert(SA_RESTART != USERSCHED_NOEAGAIN);
static_assert(SA_RESTART != USERSCHED_RESTART);
static_assert(USERSCHED_NOEAGAIN != USERSCHED_RESTART);

static_assert(sizeof(pid_t) == sizeof(uint32_t));

/*
 * Initialize lock64 to zero before use. The low word stores the mutex state;
 * the high word is reserved. Acquisition order is not guaranteed.
 * kernel_timeout is absolute CLOCK_MONOTONIC, or CLOCK_REALTIME when flagged.
 */
__nonnull((1)) int usersched_lock(
    volatile uint64_t *lock64, int flags, uint32_t user_timeout_tsc,
    const struct timespec *kernel_timeout) noexcept;
__nonnull((1)) int usersched_unlock(volatile uint64_t *lock64,
                                    int flags) noexcept;
/* tid must be the calling thread's Linux TID (gettid()), not pthread_t. */
__nonnull((1)) int usersched_lock_pi2(
    volatile uint32_t *lock, pid_t tid, int flags, uint32_t user_timeout_tsc,
    const struct timespec *kernel_timeout) noexcept;
__nonnull((1)) int usersched_unlock_pi(volatile uint32_t *lock, pid_t tid,
                                       int flags) noexcept;

extern const sigset_t _fset;
extern thread_local __attribute((tls_model("initial-exec"))) sigset_t _oset;
/* Block signals; the implicit saved mask is shared and must not be nested. */
#define as_enter()                                                             \
  log_verify_errno(pthread_sigmask(SIG_SETMASK, &_fset, &_oset))
/* Exit AS-safe critical section. */
#define as_exit() log_verify_errno(pthread_sigmask(SIG_SETMASK, &_oset, NULL))
/*
 * If `set` or `oldset` is NULL, internal variable will be used.
 * Pass explicit oldset/set storage for nested critical sections.
 * A failed lock acquisition restores the original signal mask.
 *
 * When `set` is NULL, all signal will be blocked for the thread.
 * (this calls pthread_sigmask(SIG_SETMASK, ...) internally)
 */
__nonnull((1)) int usersched_plock(volatile uint64_t *lock64, int flags,
                                   uint32_t user_timeout_tsc,
                                   const struct timespec *kernel_timeout,
                                   const sigset_t *set,
                                   sigset_t *oldset) noexcept;
/* If `oldset` is NULL, internal variable will be used. */
__nonnull((1)) int usersched_punlock(volatile uint64_t *lock64, int flags,
                                     const sigset_t *set) noexcept;
/*
 * If `set` or `oldset` is NULL, internal variable will be used.
 * Pass explicit oldset/set storage for nested critical sections.
 * A failed lock acquisition restores the original signal mask.
 *
 * When `set` is NULL, all signal will be blocked for the thread.
 * (this calls pthread_sigmask(SIG_SETMASK, ...) internally)
 */
__nonnull((1)) int usersched_plock_pi2(volatile uint32_t *lock, pid_t tid,
                                       int flags, uint32_t user_timeout_tsc,
                                       const struct timespec *kernel_timeout,
                                       const sigset_t *set,
                                       sigset_t *oldset) noexcept;
/* If `oldset` is NULL, internal variable will be used. */
__nonnull((1)) int usersched_punlock_pi(volatile uint32_t *lock, pid_t tid,
                                        int flags,
                                        const sigset_t *set) noexcept;

/* DO NOT USE THIS AS IT IS NOT OPTIMIZED! */
__nonnull((2, 6)) uint32_t
    _usersched_spsc_prepare_read(uint32_t *restrict pos_r,
                                 const volatile uint32_t *restrict pos_w,
                                 uint32_t pos_end, uint32_t size,
                                 uint32_t *restrict usersched_tsc,
                                 uint32_t *restrict pos_w_save) noexcept;
/* DO NOT USE THIS AS IT IS NOT OPTIMIZED! */
__nonnull((1, 6)) uint32_t
    _usersched_spsc_prepare_write(const volatile uint32_t *restrict pos_r,
                                  uint32_t *restrict pos_w, uint32_t pos_end,
                                  uint32_t size,
                                  uint32_t *restrict usersched_tsc,
                                  uint32_t *restrict pos_r_save) noexcept;

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
      printk(KERN_SOH rstr(lvl) "%s[%d]: %s:%d: %s: " fmt "\n",                \
             THIS_MODULE->name, task_pid_vnr(current), FILE_NAME, __LINE__,    \
             __func__, ##__VA_ARGS__);                                         \
  })
#else
#define log(lvl, fmt, ...)                                                     \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH rstr(lvl) "kernel[%d]: %s:%d: %s: " fmt "\n",            \
             task_pid_vnr(current), FILE_NAME, __LINE__, __func__,             \
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
      printk(KERN_SOH rstr(lvl) "%s[%d] %04x:%02hhx:%02x.%x: %s:%d: %s: " fmt  \
                                "\n",                                          \
             THIS_MODULE->name, task_pid_vnr(current),                         \
             pci_domain_nr(pdev->bus), pdev->bus->number,                      \
             PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), FILE_NAME,          \
             __LINE__, __func__, ##__VA_ARGS__);                               \
  })
#else
#define log_pci(pdev, lvl, fmt, ...)                                           \
  ({                                                                           \
    if (unlikely(log_lvl() >= lvl))                                            \
      printk(KERN_SOH rstr(                                                    \
                 lvl) "kernel[%d] %04x:%02hhx:%02x.%x: %s:%d: %s: " fmt "\n",  \
             task_pid_vnr(current), pci_domain_nr(pdev->bus),                  \
             pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),  \
             FILE_NAME, __LINE__, __func__, ##__VA_ARGS__);                    \
  })
#endif
#ifndef NDEBUG
#define trace_pci(pdev, lvl, fmt, ...) log_pci(pdev, lvl, fmt, ##__VA_ARGS__)
#else
#define trace_pci(pdev, lvl, fmt, ...)
#endif

#endif

/* [Kernel] END */

#ifdef __cplusplus
}
#endif
