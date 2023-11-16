#pragma once

/* [Common] BEGIN */

/* [Common] END */

#ifndef __KERNEL__

/* [Userspace] BEGIN */

#ifdef __cplusplus
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <sys/syscall.h>
#include <sys/unistd.h>

#include <x86intrin.h>

/* [Userspace] END */

#else

/* [Kernel] BEGIN */

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/version.h>

/* [Kernel] END */

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* [Common] BEGIN */

/* Static assertion */

#if defined(__cplusplus) && !defined(_Static_assert)
#define _Static_assert static_assert
#endif
#define STATIC_ASSERT(expression)                                              \
  _Static_assert(expression, "Assertion `" #expression "' failed.")

/* Casting */

#define ADDR_CAST(addr) ((void *)(intptr_t)(addr))
#define VAL_CAST(val, type) ((type)(intptr_t)(val))

#define typeof_elem(struct, elem) typeof(((typeof(struct)){0}).elem)
#define sizeof_elem(struct, elem) sizeof(((typeof(struct)){0}).elem)

/* Alignment */

#define align_size(size, alignment)                                            \
  ((((size) / (alignment)) + !!((size) % (alignment))) * (alignment))
#define align_pow2_size(size, alignment_pow2)                                  \
  (((size) + ((alignment_pow2)-1)) & ~((alignment_pow2)-1))

/* SPSC (free size) */

size_t spsc_read_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                      size_t req_size);
size_t spsc_write_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                       size_t req_size);

/* x86 BMI */

typedef uint64_t bitset64_t;
#define BITSET64_ARR_LEN(nr_bits)                                              \
  ((nr_bits) / (8 * sizeof(bitset64_t)) +                                      \
   !!((nr_bits) % (8 + sizeof(bitset64_t))))

int x86_test_bit(const bitset64_t *restrict bitset, uint32_t idx);

int x86_set_bit_nonatomic(bitset64_t *restrict bitset, uint32_t idx);
int x86_unset_bit_nonatomic(bitset64_t *restrict bitset, uint32_t idx);
int x86_set_bit_atomic(bitset64_t *restrict bitset, uint32_t idx);
int x86_unset_bit_atomic(bitset64_t *restrict bitset, uint32_t idx);

int64_t x86_search_lowest_bit(const bitset64_t *restrict bitset,
                              uint32_t start_idx, uint32_t last_idx);
int64_t x86_consume_lowest_bit_nonatomic(bitset64_t *restrict bitset,
                                         uint32_t start_idx, uint32_t last_idx);
int64_t x86_search_lowest_common_bit(const bitset64_t *restrict bitset,
                                     const bitset64_t *restrict bitset2,
                                     uint32_t start_idx, uint32_t last_idx);

/* [Common] END */

#ifndef __KERNEL__

/* [Userspace] BEGIN */

/* Base name of file */

#define __filename__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* Memory barrier */

#define barrier() __asm__ __volatile__("" : : : "memory")

/* Static branch prediction */

/* unlikely() should be prioritized over likely()! (?) */
#define unlikely(expr) __builtin_expect(!!(expr), 0)
/* Use of likely() is discouraged! Try to use unlikely(). (?) */
#define likely(expr) __builtin_expect(!!(expr), 1)

/* x86 UMWAIT */

#define UMWAIT(address, control, counter, uaddr32, old_val32)                  \
  ({                                                                           \
    _umonitor((void *)(address));                                              \
    (*(uaddr32) == (old_val32)) ? _umwait(control, counter) : 0;               \
  })

/* Userspace scheduler */

void usersched_init(void);

#define USERSCHED_TSC_SETUP_TIMEOUT_US 10000
/**
 * TSC frequency (TSC per second) on this system set by usersched_init()
 *
 * (default value is 1000 * 1000 * 1000, which means 1GHz)
 */
extern int64_t usersched_tsc_freq;

/* Boolean representing UMWAIT support on this system set by usersched_init() */
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
  USERSCHED_COND_BREAK = 0,
  USERSCHED_COND_ONESHOT = 0,
  USERSCHED_COND_SCHEDULE = 1,
  USERSCHED_COND_CONTINUE = 2,
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

/* Log */

extern int log_suppress;
#define ENABLE_LOG() (void)(log_suppress = 0)
#define DISABLE_LOG() (void)(log_suppress = 1)

#define _LOG_MSG(filename, line, func, postfix, format, ...)                   \
  "[tid %d] %s: %s:%d: %s: " format postfix, gettid(),                         \
      program_invocation_short_name, filename, line, func, ##__VA_ARGS__
#define _LOG_MSG_TMPL(postfix, format, ...)                                    \
  _LOG_MSG(__filename__, __LINE__, __func__, postfix, format, ##__VA_ARGS__)

#define LOG_MSG(format, ...) _LOG_MSG_TMPL("\n", format, ##__VA_ARGS__)
#define LOG_DEBUG_MSG(format, ...)                                             \
  _LOG_MSG_TMPL("\e[0m\n", "\e[2;37mDEBUG: " format, ##__VA_ARGS__)
#define LOG_INFO_MSG(format, ...)                                              \
  _LOG_MSG_TMPL("\e[0m\n", "\e[34mINFO: " format, ##__VA_ARGS__)
#define LOG_WARN_MSG(format, ...)                                              \
  _LOG_MSG_TMPL("\e[0m\n", "\e[33mWARNING: " format, ##__VA_ARGS__)
#define LOG_ERR_MSG(format, ...)                                               \
  _LOG_MSG_TMPL("\e[0m\n", "\e[31mERROR: " format, ##__VA_ARGS__)
#define LOG_FATAL_MSG(format, ...)                                             \
  _LOG_MSG_TMPL("\e[0m\n", "\e[1;31mFATAL: " format, ##__VA_ARGS__)

#define _ASSERT_MSG(expression) "Assertion `" #expression "' failed."

#define _log(message, ...) dprintf(STDERR_FILENO, message)

#define log(format, ...)                                                       \
  (unlikely(log_suppress) ? 0 : _log(LOG_MSG(format, ##__VA_ARGS__)))
#define log_info(format, ...)                                                  \
  (unlikely(log_suppress) ? 0 : _log(LOG_INFO_MSG(format, ##__VA_ARGS__)))
#define log_warn(format, ...)                                                  \
  (unlikely(log_suppress) ? 0 : _log(LOG_WARN_MSG(format, ##__VA_ARGS__)))
#define log_err(format, ...)                                                   \
  (unlikely(log_suppress) ? 0 : _log(LOG_ERR_MSG(format, ##__VA_ARGS__)))
#define _log_fatal(format, ...)                                                \
  (unlikely(log_suppress) ? 0 : _log(LOG_FATAL_MSG(format, ##__VA_ARGS__)))

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
  (unlikely(log_suppress) ? 0                                                  \
                          : _log_backtrace(__filename__, __LINE__, __func__))

#define _log_abort()                                                           \
  log_backtrace();                                                             \
  abort()

#define log_abort(format, ...)                                                 \
  ({                                                                           \
    !strcmp(format, "") ? _log_fatal("abort")                                  \
                        : _log_fatal(format, ##__VA_ARGS__);                   \
    _log_abort();                                                              \
  })
#define log_perror_abort(format, ...)                                          \
  ({                                                                           \
    !strcmp(format, "")                                                        \
        ? _log_fatal("%s", strerror(errno))                                    \
        : _log_fatal(format ": %s", ##__VA_ARGS__, strerror(errno));           \
    _log_abort();                                                              \
  })

#define _log_assert(expression)                                                \
  ({                                                                           \
    _log_fatal(_ASSERT_MSG(expression));                                       \
    _log_abort();                                                              \
  })
#define log_assert(expression)                                                 \
  (!(expression) ? _log_assert(expression) : (void)0)
#define log_perror_assert(expression)                                          \
  (!(expression) ? log_perror_abort(_ASSERT_MSG(expression)) : (void)0)

#ifndef NDEBUG
#define log_debug(format, ...)                                                 \
  (unlikely(log_suppress) ? 0 : _log(LOG_DEBUG_MSG(format, ##__VA_ARGS__)))
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
#define log_debug_assert(expression) (void)(expression)
#define log_debug_perror_assert(expression) (void)(expression)
#define log_debug_backtrace()
#endif

/* [Userspace] END */

#else

/* [Kernel] BEGIN */

/* Base name of file */

#define __filename__                                                           \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1     \
                                    : __FILE__)

/* Linux kernel compatibility */

#if LINUX_VERSION_CODE > KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(name)
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 17, 0)
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_index)
#else
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_attrib.entry_nr)
#endif

/* Error handling */

#define ERROR_CODE(ret)                                                        \
  VAL_CAST(                                                                    \
      {                                                                        \
        typeof(ret) _ret = (ret);                                              \
        IS_ERR(_ret) ? _ret : 0;                                               \
      },                                                                       \
      int)

/* Log */

#ifdef MODULE
#define _KLOG_MSG(filename, line, func, fmt, ...)                              \
  "%s[%d]: %s:%d: %s: " fmt "\n", THIS_MODULE->name, task_pid_vnr(current),    \
      filename, line, func, ##__VA_ARGS__
#else
#define _KLOG_MSG(filename, line, func, fmt, ...)                              \
  "%s[%d]: %s:%d: %s: " fmt "\n", module_name(THIS_MODULE),                    \
      task_pid_vnr(current), filename, line, func, ##__VA_ARGS__
#endif
#define _KLOG_MSG_TMPL(fmt, ...)                                               \
  _KLOG_MSG(__filename__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define klog(level, fmt, ...) printk(level _KLOG_MSG_TMPL(fmt, ##__VA_ARGS__))

#define _KLOG_DEV_MSG(pdev, filename, line, func, fmt, ...)                    \
  "%s[%d] %04x:%02hhx:%02x.%x: %s:%d: %s: " fmt "\n", THIS_MODULE->name,       \
      task_pid_vnr(current), pci_domain_nr(pdev->bus), pdev->bus->number,      \
      PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), filename, line, func,      \
      ##__VA_ARGS__
#define _KLOG_DEV_MSG_TMPL(pdev, fmt, ...)                                     \
  _KLOG_DEV_MSG(pdev, __filename__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define klog_dev(pdev, level, fmt, ...)                                        \
  printk(level _KLOG_DEV_MSG_TMPL(pdev, fmt, ##__VA_ARGS__))

#endif

/* [Kernel] END */

#ifdef __cplusplus
}
#endif
