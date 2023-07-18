#pragma once

#ifndef __KERNEL__

/* For userspace only */

#ifdef __cplusplus
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#else
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

#include <unistd.h>

#include <x86intrin.h>

#else

/* For kernel only */

#include <linux/version.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define __filename__                                                           \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1     \
                                    : __FILE__)

#define ADDR_CAST(val) (void *)(uintptr_t)val
#define VAL_CAST(val) (uintptr_t) val

#ifndef __KERNEL__

/* For userspace only */

#define UMWAIT(address, control, counter, uaddr32, old_val32)                  \
  ({                                                                           \
    _umonitor((void *)address);                                                \
    (*uaddr32 == old_val32) ? _umwait(control, counter) : 0;                   \
  })

#define USERSCHED_TSC_1US_SLEEP_US 10000
/**
 * TSC value representing 1us on this system setup by constructor
 *
 * (0 if not yet set)
 */
extern uint32_t usersched_tsc_1us;

/**
 * Boolean representing UMWAIT support on this system setup by constructor
 *
 * (-1 if not yet set)
 */
extern int usersched_support_umwait;

/**
 * Return absolute TSC value referring timeout
 *
 * If timeout_tsc == 0, return 0.
 * If timeout_tsc == UINT32_MAX, return UINT64_MAX.
 */
unsigned long long _user_schedule_start(uint32_t timeout_tsc);

/**
 * Return updated 32-bit timeout TSC value
 *
 * If abs_timeout_tsc == UINT64_MAX, return UINT32_MAX.
 */
uint32_t _user_update_timeout_tsc(unsigned long long abs_timeout_tsc);

uint32_t _user_reschedule(unsigned long long abs_timeout_tsc,
                          const volatile uint32_t *restrict uaddr32,
                          uint32_t old_val32);

enum _USERSCHED_COND {
  USERSCHED_COND_TERM = 0,
  USERSCHED_COND_ONESHOT = 0,
  USERSCHED_COND_RUN = 1
};

/**
 * Userspace scheduler
 *
 * Do NOT use `break` or `continue` below this macro in same scope (use
 * cond_ptr instead).
 *
 * @param rel_timeout_tsc (uint32_t) Relative timeout TSC (UINT32_MAX if
 * indefinite)
 * @param abs_timeout_tsc_ptr (unsigned long long *restrict) Pointer to store
 * absolute timeout TSC (UINT64_MAX will be stored if indefinite or 0 if
 * immediate)
 * @param cond_ptr (uint32_t *restrict) Pointer to store initial conditional
 * value for scheduling
 * @param cond_init (uint32_t) Initial value to be saved to *cond_ptr
 */
#define USER_SCHEDULE(rel_timeout_tsc, abs_timeout_tsc_ptr, cond_ptr,          \
                      cond_init)                                               \
  {} *abs_timeout_tsc_ptr = _user_schedule_start(rel_timeout_tsc);             \
  *cond_ptr = cond_init;                                                       \
  do {                                                                         \
  (void)0

/**
 * Userspace rescheduler
 *
 * Do NOT use `break` or `continue` above this macro in same scope (use
 * cond_ptr instead).
 *
 * @param rel_timeout_tsc_ptr (uint32_t *restrict) Pointer to store deducted
 * relative timeout TSC (UINT32_MAX will be stored if indefinite)
 * @param abs_timeout_tsc (unsigned long long) Absolute timeout TSC (UINT64_MAX
 * if indefinite or 0 if immediate)
 * @param snoop_uaddr32 (const volatile uint32_t *restrict) Address to snoop (if
 * UMWAIT support is present) and compare (will NOT be compared if NULL)
 * @param old_val32 (uint32_t) Original value stored at non-NULL snoop_addr32
 * (timeout will NOT happen if *snoop_uaddr32 == old_val32)
 * @param cond_ptr (uint32_t *restrict) Pointer to conditional value for
 * scheduling (0 will be saved if timeout and if the value > 1, it will be
 * decremented with timeout checking (+ snooping) bypassed)
 */
#define USER_RESCHEDULE(rel_timeout_tsc_ptr, abs_timeout_tsc, snoop_uaddr32,   \
                        old_val32, cond_ptr)                                   \
  {}                                                                           \
  }                                                                            \
  while ((*cond_ptr == 1)                                                      \
             ? (*cond_ptr =                                                    \
                    (!(uintptr_t)snoop_uaddr32 ||                              \
                     (*(const uint32_t *restrict)snoop_uaddr32 == old_val32))  \
                        ? _user_reschedule(abs_timeout_tsc, snoop_uaddr32,     \
                                           old_val32)                          \
                        : 1)                                                   \
             : (*cond_ptr ? --*cond_ptr : 0))                                  \
    ;                                                                          \
  *rel_timeout_tsc_ptr = _user_update_timeout_tsc(abs_timeout_tsc);            \
  (void)0

#define barrier() __asm__ __volatile__("" : : : "memory")

/* unlikely() should be prioritized over likely()! */
#define unlikely(expr) __builtin_expect(!!(expr), 0)
/* Use of likely() is discouraged! Try to use unlikely(). */
#define likely(expr) __builtin_expect(!!(expr), 1)

typedef uint64_t bitset_t;

int test_bit(const bitset_t *restrict bitset, uint32_t idx);

int set_bit_nonatomic(bitset_t *restrict bitset, uint32_t idx);
int unset_bit_nonatomic(bitset_t *restrict bitset, uint32_t idx);
int set_bit_atomic(bitset_t *restrict bitset, uint32_t idx);
int unset_bit_atomic(bitset_t *restrict bitset, uint32_t idx);

int64_t search_lowest_bit(const bitset_t *restrict bitset, uint32_t start_idx,
                          uint32_t last_idx);
int64_t consume_lowest_bit_nonatomic(bitset_t *restrict bitset,
                                     uint32_t start_idx, uint32_t last_idx);
int64_t search_lowest_common_bit(const bitset_t *restrict bitset,
                                 const bitset_t *restrict bitset2,
                                 uint32_t start_idx, uint32_t last_idx);

extern int _suppress_log;
#define ENABLE_LOG() (void)(_suppress_log = 0)
#define DISABLE_LOG() (void)(_suppress_log = 1)

#define _LOG_MSG(filename, line, func, postfix, format, ...)                   \
  "[tid %d] %s: %s:%d: %s: " format postfix, gettid(),                         \
      program_invocation_short_name, filename, line, func, ##__VA_ARGS__
#define _LOG_MSG_TMPL(postfix, format, ...)                                    \
  _LOG_MSG(__filename__, __LINE__, __func__, postfix, format, ##__VA_ARGS__)

#define LOG_MSG(format, ...) _LOG_MSG_TMPL("\n", format, ##__VA_ARGS__)
#define LOG_DEBUG_MSG(format, ...)                                             \
  _LOG_MSG_TMPL("\e[0m\n", "\e[2;0mDEBUG: " format, ##__VA_ARGS__)
#define LOG_INFO_MSG(format, ...)                                              \
  _LOG_MSG_TMPL("\e[0m\n", "\e[34mINFO: " format, ##__VA_ARGS__)
#define LOG_WARN_MSG(format, ...)                                              \
  _LOG_MSG_TMPL("\e[0m\n", "\e[33mWARNING: " format, ##__VA_ARGS__)
#define LOG_ERR_MSG(format, ...)                                               \
  _LOG_MSG_TMPL("\e[0m\n", "\e[31mERROR: " format, ##__VA_ARGS__)

#define _ASSERT_MSG(expression) "Assertion `" #expression "' failed."
#define LOG_FATAL_MSG(format, ...)                                             \
  _LOG_MSG_TMPL("\e[0m\n", "\e[1;31mFATAL: " format, ##__VA_ARGS__)
#define LOG_ASSERT_MSG(postfix, expression)                                    \
  _LOG_MSG_TMPL(postfix, "\e[1;31m" _ASSERT_MSG(expression))

#define _log(message, ...) dprintf(STDERR_FILENO, message)

#define log(format, ...)                                                       \
  (unlikely(_suppress_log) ? 0 : _log(LOG_MSG(format, ##__VA_ARGS__)))
#define log_info(format, ...)                                                  \
  (unlikely(_suppress_log) ? 0 : _log(LOG_INFO_MSG(format, ##__VA_ARGS__)))
#define log_warn(format, ...)                                                  \
  (unlikely(_suppress_log) ? 0 : _log(LOG_WARN_MSG(format, ##__VA_ARGS__)))
#define log_err(format, ...)                                                   \
  (unlikely(_suppress_log) ? 0 : _log(LOG_ERR_MSG(format, ##__VA_ARGS__)))
#define _log_fatal(format, ...)                                                \
  (unlikely(_suppress_log) ? 0 : _log(LOG_FATAL_MSG(format, ##__VA_ARGS__)))

#define log_perror(format, ...)                                                \
  (!strcmp(format, "") ? log("%s", strerror(errno))                            \
                       : log(format ": %s", ##__VA_ARGS__, strerror(errno)))
#define log_info_perror(format, ...)                                           \
  (!strcmp(format, "")                                                         \
       ? log_info("%s", strerror(errno))                                       \
       : log_info(format ": %s", ##__VA_ARGS__, strerror(errno)))
#define log_warn_perror(format, ...)                                           \
  (!strcmp(format, "")                                                         \
       ? log_warn("%s", strerror(errno))                                       \
       : log_warn(format ": %s", ##__VA_ARGS__, strerror(errno)))
#define log_err_perror(format, ...)                                            \
  (!strcmp(format, "")                                                         \
       ? log_err("%s", strerror(errno))                                        \
       : log_err(format ": %s", ##__VA_ARGS__, strerror(errno)))

#define LOG_MAX_BACKTRACE 1024
int _log_backtrace(const char *filename, int line, const char *func);

#define log_backtrace()                                                        \
  (unlikely(_suppress_log) ? 0                                                 \
                           : _log_backtrace(__filename__, __LINE__, __func__))

#define _log_abort()                                                           \
  log_backtrace();                                                             \
  abort()

#define log_abort(format, ...)                                                 \
  {                                                                            \
    !strcmp(format, "") ? _log_fatal("abort")                                  \
                        : _log_fatal(format, ##__VA_ARGS__);                   \
    _log_abort();                                                              \
  }                                                                            \
  (void)0
#define log_perror_abort(format, ...)                                          \
  {                                                                            \
    !strcmp(format, "")                                                        \
        ? _log_fatal("%s", strerror(errno))                                    \
        : _log_fatal(format ": %s", ##__VA_ARGS__, strerror(errno));           \
    _log_abort();                                                              \
  }                                                                            \
  (void)0

#define log_assert(expression)                                                 \
  if (!(expression)) {                                                         \
    _log(LOG_ASSERT_MSG("\e[0m\n", expression));                               \
    _log_abort();                                                              \
  }                                                                            \
  (void)0
#define log_perror_assert(expression)                                          \
  if (!(expression))                                                           \
    log_perror_abort(_ASSERT_MSG(expression));                                 \
  (void)0

#ifndef NDEBUG
#define log_debug(format, ...)                                                 \
  (unlikely(_suppress_log) ? 0 : _log(LOG_DEBUG_MSG(format)))
#define log_debug_perror(format, ...)                                          \
  (!strcmp(format, "")                                                         \
       ? log_debug("%s", strerror(errno))                                      \
       : log_debug(format ": %s", ##__VA_ARGS__, strerror(errno)))
#define log_debug_assert(expression) log_assert(expression)
#define log_debug_perror_assert(expression) log_perror_assert(expression)
#define log_debug_backtrace() log_backtrace()
#else
#define log_debug(format, ...)
#define log_debug_perror(format, ...)
#define log_debug_assert(expression)
#define log_debug_perror_assert(expression)
#define log_debug_backtrace()
#endif

#else

/* For kernel only */

#define ERROR_CODE(ret) (int)(uintptr_t)(IS_ERR(ret) ? ret : 0)

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 17, 0)
#define msi_desc_to_index(msi_desc) (msi_desc->msi_index)
#else
#define msi_desc_to_index(msi_desc) (msi_desc->msi_attrib.entry_nr)
#endif

enum module_destruct_level { MODULE_DATA, MODULE_CLS, MODULE_DRIVER };

enum pci_destruct_level {
  PCI_DATA,
  PCI_PDEV,
  PCI_REGION,
  PCI_CHRDEV,
  PCI_CDEV,
  PCI_DEV,
  PCI_REG,
  PCI_DEVM,
  PCI_IRQ
};

#define _KLOG_MSG(filename, line, func, fmt, ...)                              \
  "%s[%d]: %s:%d: %s: " fmt "\n", module_name(THIS_MODULE),                    \
      task_pid_vnr(current), filename, line, func, ##__VA_ARGS__
#define _KLOG_MSG_TMPL(fmt, ...)                                               \
  _KLOG_MSG(__filename__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define klog(level, fmt, ...) printk(level _KLOG_MSG_TMPL(fmt, ##__VA_ARGS__))

#endif

#ifdef __cplusplus
}
#endif
