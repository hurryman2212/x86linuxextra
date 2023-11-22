#pragma once

/* [Common] BEGIN */

/* [Common] END */

#ifndef __KERNEL__

/* [Userspace] BEGIN */

#ifdef __cplusplus
#include <cstdarg>
#include <cstdint>
#include <cstring>
#else
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#endif

#include <syslog.h>

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

/* Casting */

#define address_cast(value) ((void *)(uintptr_t)(value))
#define value_cast(address, type) ((type)(uintptr_t)(address))

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

int spsc_rewind_read(size_t pos_start, size_t *__restrict pos_r, size_t pos_w,
                     size_t pos_end);
int spsc_rewind_write(size_t pos_start, size_t pos_r, size_t *__restrict pos_w,
                      size_t pos_end);

/* x86 BMI */

typedef uint64_t bitset64_t;
#define BITSET64_LEN(nr_bits)                                                  \
  ((nr_bits) / (8 * sizeof(bitset64_t)) +                                      \
   !!((nr_bits) % (8 + sizeof(bitset64_t))))

int x86_test_bit(const bitset64_t *__restrict bitset, uint32_t idx);

int x86_set_bit_nonatomic(bitset64_t *__restrict bitset, uint32_t idx);
int x86_unset_bit_nonatomic(bitset64_t *__restrict bitset, uint32_t idx);
int x86_set_bit_atomic(bitset64_t *__restrict bitset, uint32_t idx);
int x86_unset_bit_atomic(bitset64_t *__restrict bitset, uint32_t idx);

int64_t x86_search_lowest_bit(const bitset64_t *__restrict bitset,
                              uint32_t start_idx, uint32_t last_idx);
int64_t x86_consume_lowest_bit_nonatomic(bitset64_t *__restrict bitset,
                                         uint32_t start_idx, uint32_t last_idx);
int64_t x86_search_lowest_common_bit(const bitset64_t *__restrict bitset,
                                     const bitset64_t *__restrict bitset2,
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

pid_t usersched_gettid(void);

void usersched_init(int prohibit_umwait);

/**
 * Boolean of Invariant TSC support on this system set by usersched_init() call
 *
 * (default value is 0 (unavailable))
 */
extern int usersched_support_invariant_tsc;
/**
 * Boolean of UMWAIT support on this system set by usersched_init() call
 *
 * (default value is 0 (unavailable))
 */
extern int usersched_support_umwait;

#define USERSCHED_TSC_SETUP_TIMEOUT_US 10000
/**
 * TSC frequency (TSC per second) on this system set by usersched_init() call
 *
 * (default value is 1000 * 1000 * 1000, which means 1GHz)
 */
extern uint64_t usersched_tsc_freq;
/**
 * TSC per 1us on this set by usersched_init() call
 *
 * ((TSC per us) = (TSC freq. = TSC per sec.) / 10^6)
 * */
extern uint32_t usersched_tsc_1us;

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
                          const volatile uint32_t *__restrict uaddr32,
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
 * @param abs_timeout_tsc_ptr (unsigned long long *__restrict) Pointer to store
 * absolute timeout TSC (UINT64_MAX will be stored if indefinite or 0 if
 * immediate)
 * @param cond_ptr (uint32_t *__restrict) Pointer to store initial conditional
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
 * @param rel_timeout_tsc_ptr (uint32_t *__restrict) Pointer to store deducted
 * relative timeout TSC (UINT32_MAX will be stored if indefinite)
 * @param abs_timeout_tsc (unsigned long long) Absolute timeout TSC (UINT64_MAX
 * if indefinite or 0 if immediate)
 * @param snoop_uaddr32 (const volatile uint32_t *__restrict) Address to snoop
 * (if UMWAIT support is present) and compare (will NOT be compared if NULL)
 * @param old_val32 (uint32_t) Original value stored at non-NULL snoop_addr32
 * (timeout will NOT happen if *snoop_uaddr32 == old_val32)
 * @param cond_ptr (uint32_t *__restrict) Pointer to conditional value for
 * scheduling (0 will be saved if timeout and if the value > 1, it will be
 * decremented with timeout checking (+ snooping) bypassed)
 */
#define USER_RESCHEDULE(rel_timeout_tsc_ptr, abs_timeout_tsc, snoop_uaddr32,   \
                        old_val32, cond_ptr)                                   \
  {}                                                                           \
  }                                                                            \
  while ((*cond_ptr == 1)                                                      \
             ? (*cond_ptr = (!(uintptr_t)snoop_uaddr32 ||                      \
                             (*(const uint32_t *__restrict)snoop_uaddr32 ==    \
                              old_val32))                                      \
                                ? _user_reschedule(abs_timeout_tsc,            \
                                                   snoop_uaddr32, old_val32)   \
                                : 1)                                           \
             : (*cond_ptr ? --*cond_ptr : 0))                                  \
    ;                                                                          \
  *rel_timeout_tsc_ptr = _user_update_timeout_tsc(abs_timeout_tsc);            \
  (void)0

/* Log */

/* Setup logging system and enable it. */
void log_init(const char *ident, int option, int facility, int broadcast_stderr,
              int broadcast_syslog);
/* Disable logging system and and destruct it. */
void log_deinit(void);

extern int log_enabled; // Default value is -1 (disabled).
#define log_enable(level) ((void)(log_enabled = level))
#define log_disable() ((void)(log_enabled = -1))

#define LOG_LINE_MAX 4096
void _log(int level, const char *filename, int line, const char *func,
          const char *fmt, ...);
void _vlog(int level, const char *filename, int line, const char *func,
           const char *fmt, va_list ap);
#define log(level, fmt, ...)                                                   \
  _log(level, __filename__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define vlog(level, fmt, ap)                                                   \
  _vlog(level, __filename__, __LINE__, __func__, fmt, ap)

#define log_emerg(fmt, ...) log(LOG_EMERG, fmt, ##__VA_ARGS__)
#define log_alert(fmt, ...) log(LOG_ALERT, fmt, ##__VA_ARGS__)
#define log_crit(fmt, ...) log(LOG_CRIT, fmt, ##__VA_ARGS__)
#define log_err(fmt, ...) log(LOG_ERR, fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) log(LOG_WARNING, fmt, ##__VA_ARGS__)
#define log_notice(fmt, ...) log(LOG_NOTICE, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) log(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) log(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define vlog_emerg(fmt, ap) vlog(LOG_EMERG, fmt, ap)
#define vlog_alert(fmt, ap) vlog(LOG_ALERT, fmt, ap)
#define vlog_crit(fmt, ap) vlog(LOG_CRIT, fmt, ap)
#define vlog_err(fmt, ap) vlog(LOG_ERR, fmt, ap)
#define vlog_warning(fmt, ap) vlog(LOG_WARNING, fmt, ap)
#define vlog_notice(fmt, ap) vlog(LOG_NOTICE, fmt, ap)
#define vlog_info(fmt, ap) vlog(LOG_INFO, fmt, ap)
#define vlog_debug(fmt, ap) vlog(LOG_DEBUG, fmt, ap)

void _log_perror(int level, const char *filename, int line, const char *func,
                 const char *s, int errnum);
#define log_perror(level, s)                                                   \
  _log_perror(level, __filename__, __LINE__, __func__, s, -1)
#define log_perror_emerg(s) log_perror(LOG_EMERG, s)
#define log_perror_alert(s) log_perror(LOG_ALERT, s)
#define log_perror_crit(s) log_perror(LOG_CRIT, s)
#define log_perror_err(s) log_perror(LOG_ERR, s)
#define log_perror_warning(s) log_perror(LOG_WARNING, s)
#define log_perror_notice(s) log_perror(LOG_NOTICE, s)
#define log_perror_info(s) log_perror(LOG_INFO, s)
#define log_perror_debug(s) log_perror(LOG_DEBUG, s)

#define log_perror_on_error(level, expression)                                 \
  ({                                                                           \
    if (unlikely(value_cast(expression, int) == -1))                           \
      log_perror(level, #expression);                                          \
  })
#define log_perror_if_errno(level, expression)                                 \
  ({                                                                           \
    if (unlikely(expression))                                                  \
      _log_perror(level, __filename__, __LINE__, __func__, #expression,        \
                  value_cast(expression, int));                                \
  })

#define log_perror_on_error_emerg(expression)                                  \
  log_perror_on_error(LOG_EMERG, expression)
#define log_perror_on_error_alert(expression)                                  \
  log_perror_on_error(LOG_ALERT, expression)
#define log_perror_on_error_crit(expression)                                   \
  log_perror_on_error(LOG_CRIT, expression)
#define log_perror_on_error_err(expression)                                    \
  log_perror_on_error(LOG_ERR, expression)
#define log_perror_on_error_warning(expression)                                \
  log_perror_on_error(LOG_WARNING, expression)
#define log_perror_on_error_notice(expression)                                 \
  log_perror_on_error(LOG_NOTICE, expression)
#define log_perror_on_error_info(expression)                                   \
  log_perror_on_error(LOG_INFO, expression)
#define log_perror_on_error_debug(expression)                                  \
  log_perror_on_error(LOG_DEBUG, expression)
#define log_perror_if_errno_emerg(expression)                                  \
  log_perror_if_errno(LOG_EMERG, expression)
#define log_perror_if_errno_alert(expression)                                  \
  log_perror_if_errno(LOG_ALERT, expression)
#define log_perror_if_errno_crit(expression)                                   \
  log_perror_if_errno(LOG_CRIT, expression)
#define log_perror_if_errno_err(expression)                                    \
  log_perror_if_errno(LOG_ERR, expression)
#define log_perror_if_errno_warning(expression)                                \
  log_perror_if_errno(LOG_WARNING, expression)
#define log_perror_if_errno_notice(expression)                                 \
  log_perror_if_errno(LOG_NOTICE, expression)
#define log_perror_if_errno_info(expression)                                   \
  log_perror_if_errno(LOG_INFO, expression)
#define log_perror_if_errno_debug(expression)                                  \
  log_perror_if_errno(LOG_DEBUG, expression)

#define LOG_BACKTRACE_MAX 1024
void _log_backtrace(int level, const char *filename, int line,
                    const char *func);
#define log_backtrace(level)                                                   \
  _log_backtrace(level, __filename__, __LINE__, __func__)

#define log_backtrace_emerg() log_backtrace(LOG_EMERG)
#define log_backtrace_alert() log_backtrace(LOG_ALERT)
#define log_backtrace_crit() log_backtrace(LOG_CRIT)
#define log_backtrace_err() log_backtrace(LOG_ERR)
#define log_backtrace_warning() log_backtrace(LOG_WARNING)
#define log_backtrace_notice() log_backtrace(LOG_NOTICE)
#define log_backtrace_info() log_backtrace(LOG_INFO)
#define log_backtrace_debug() log_backtrace(LOG_DEBUG)

void _log_abort(const char *filename, int line, const char *func);
#define log_abort() _log_abort(__filename__, __LINE__, __func__)

void _log_assert_fail(const char *filename, int line, const char *func,
                      const char *expression);
void _log_assert_perror_fail(const char *filename, int line, const char *func,
                             const char *expression, int errnum);
#define log_abort_if_false(expression)                                         \
  ({                                                                           \
    if (unlikely(!(expression)))                                               \
      _log_assert_fail(__filename__, __LINE__, __func__, #expression);         \
  })
#define log_abort_on_error(expression)                                         \
  ({                                                                           \
    if (unlikely(value_cast(expression, int) == -1))                           \
      _log_assert_perror_fail(__filename__, __LINE__, __func__, #expression,   \
                              -1);                                             \
  })
#define log_abort_if_errno(expression)                                         \
  ({                                                                           \
    if (unlikely(expression))                                                  \
      _log_assert_perror_fail(__filename__, __LINE__, __func__, #expression,   \
                              value_cast(expression, int));                    \
  })
#ifndef NDEBUG
#define log_assert(expression) log_abort_if_false(expression)
#define log_assert_perror(expression) log_abort_on_error(expression)
#define log_assert_errno(expression) log_abort_if_errno(expression)
#else
#define log_assert(expression)
#define log_assert_perror(expression)
#define log_assert_errno(expression)
#endif

/* [Userspace] END */

#else

/* [Kernel] BEGIN */

/* Base name of file */

#define __filename__                                                           \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1     \
                                    : __FILE__)

/* Compatibility */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(name)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_index)
#else
#define msi_desc_to_index(msi_desc) ((msi_desc)->msi_attrib.entry_nr)
#endif

/* Error handling */

#define ERRNO(ret)                                                             \
  value_cast(({                                                                \
               typeof(ret) _ret = (ret);                                       \
               IS_ERR(address_cast(_ret)) ? _ret : 0;                          \
             }),                                                               \
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
