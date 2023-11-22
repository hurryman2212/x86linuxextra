#include "x86linux/helper.h"

#include <time.h>

#include <syscall.h>
#include <unistd.h>

#include <sys/mman.h>

#include <linux/perf_event.h>

int usersched_support_invariant_tsc = 0;
int usersched_support_umwait = 0;

uint64_t usersched_tsc_freq = 1000 * 1000 * 1000;
uint32_t usersched_tsc_1us = 1000;

unsigned long long _user_schedule_start(uint32_t timeout_tsc) {
  /* Do not return UINT32_MAX and 0 for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!timeout_tsc || (timeout_tsc == UINT32_MAX))
    return ((unsigned long long)timeout_tsc << 32) | timeout_tsc;

  unsigned long long tsc = _rdtsc();

  if (unlikely((tsc + timeout_tsc) <= tsc))
    /* TSC overflow */
    return (timeout_tsc - (UINT64_MAX - tsc))
               ? (timeout_tsc - (UINT64_MAX - tsc))
               : 1;
  return unlikely((tsc + timeout_tsc) == UINT64_MAX) ? (UINT64_MAX - 1)
                                                     : (tsc + timeout_tsc);
}
uint32_t _user_reschedule(unsigned long long abs_timeout_tsc,
                          const volatile uint32_t *__restrict uaddr32,
                          uint32_t old_val32) {
  /* Do not evaluate (*uaddr32 != old_val32 ) first! */

  /* Check immediate timeout. */
  if (!abs_timeout_tsc)
    return 0;

  do {
    unsigned long long tsc;
    int tsc_overflow = 0;
    int is_indefinite = abs_timeout_tsc == UINT64_MAX;

    /* Check non-indefinite timeout */
    if (!is_indefinite) {
      tsc = _rdtsc();
      tsc_overflow = !!((abs_timeout_tsc ^ tsc) & INT64_MIN);

      if (unlikely(tsc_overflow)) {
        if (abs_timeout_tsc & INT64_MIN)
          /* Timeout expired; Return 0. */
          return 0;
      } else if (abs_timeout_tsc <= tsc)
        /* Timeout expired; Return 0. */
        return 0;
    }

    /* Start snooping if uaddr32 is non-NULL. */
    if (uaddr32) {
      /* Busy loop optimization: PAUSE or UMWAIT */

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
      if (usersched_support_umwait) {
#endif

#ifndef _NO_UMWAIT
        /* Check TSC overflow first. */
        if (is_indefinite || unlikely(tsc_overflow))
          do
            if (*uaddr32 != old_val32)
              return 1;
          while (UMWAIT(uaddr32, 0, UINT64_MAX, uaddr32, old_val32) ||
                 is_indefinite);

        do
          if (*uaddr32 != old_val32)
            return 1;
        while (UMWAIT(uaddr32, 0, abs_timeout_tsc, uaddr32, old_val32));

        if (*uaddr32 == old_val32)
          /* Timeout; Return 0. */
          return 0;
        else
          /* Store event has been detected. */
          return 1;
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
      } else {
#endif

#ifndef _FORCE_UMWAIT
        do
          if (*uaddr32 == old_val32)
            _mm_pause();
          else
            return 1;
        while (is_indefinite);
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
      }
#endif
    } else if (is_indefinite)
      return 1;
  } while (uaddr32);

  return 1;
}
uint32_t _user_update_timeout_tsc(unsigned long long abs_timeout_tsc) {
  /* Do not return UINT64_MAX for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!abs_timeout_tsc || (abs_timeout_tsc == UINT64_MAX))
    return (uint32_t)abs_timeout_tsc;

  unsigned long long tsc = _rdtsc();

  if (unlikely((abs_timeout_tsc ^ tsc) & INT64_MIN))
    /* TSC overflow */
    return (abs_timeout_tsc & INT64_MIN)
               ? 0
               : (abs_timeout_tsc + (UINT64_MAX - tsc));
  return (abs_timeout_tsc <= tsc) ? 0 : (abs_timeout_tsc - tsc);
}

/* Setup global variables for 1us TSC value and UMWAIT support. */
void usersched_init(int prohibit_umwait) {
  struct perf_event_attr pe = {
      .type = PERF_TYPE_HARDWARE,
      .size = sizeof(struct perf_event_attr),
      .config = PERF_COUNT_HW_INSTRUCTIONS,
      .disabled = 1,

      .exclude_user = 1,
      .exclude_kernel = 1,
      .exclude_hv = 1,
      .exclude_idle = 1,
      .exclude_host = 1,
      .exclude_guest = 1,
      .exclude_callchain_kernel = 1,
      .exclude_callchain_user = 1,
  };
  uint32_t eax, ebx, ecx, edx;

  /* Get TSC frequency. */

  int fast_path = 1, fd;
  fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
  /* Check if perf_event_open() system call has succeeded. */
  if (fd == -1)
    fast_path = 0;
  else {
    /* Faster path */

    ssize_t page_size = sysconf(_SC_PAGESIZE);
    log_abort_on_error(page_size = sysconf(_SC_PAGESIZE));
    struct perf_event_mmap_page *pc;
    log_abort_on_error(pc =
                           mmap(NULL, page_size, PROT_READ, MAP_SHARED, fd, 0));

    /* Check user time support. */
    if (!pc->cap_user_time)
      fast_path = 0; // Use the alternative slow path.
    else {
      usersched_tsc_freq =
          ((__uint128_t)(1000 * 1000 * 1000) << pc->time_shift) /
          pc->time_mult; // ?
      /* (TSC per us) = (TSC per sec.) / 10^6 */
      usersched_tsc_1us = usersched_tsc_freq / (1000 * 1000);
    }

    /* Clean up. */
    log_abort_on_error(munmap(pc, page_size));
    log_abort_on_error(close(fd));
  }

  if (!fast_path) {
    /* Slower path */

    unsigned int tsc_aux_start, tsc_aux_end;
    unsigned long long tsc_begin, tsc_end, ns_begin, ns_end;
    do {
      struct timespec ts;

      log_abort_on_error(clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
      ns_begin = (unsigned long long)ts.tv_sec * 1000000000ull + ts.tv_nsec;
      tsc_begin = __rdtscp(&tsc_aux_start);

      usleep(USERSCHED_TSC_SETUP_TIMEOUT_US);

      log_abort_on_error(clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
      ns_end = (unsigned long long)ts.tv_sec * 1000000000ull + ts.tv_nsec;
      tsc_end = __rdtscp(&tsc_aux_end);

    } while (tsc_aux_end != tsc_aux_start); // Check core migration.

    /* (TSC freq.) = (TSC per sec.) = (elapsed TSC) * 10^9 / (elapsed ns) */
    usersched_tsc_freq = ((__uint128_t)tsc_end - tsc_begin) // elapsed tsc
                         * (1000 * 1000 * 1000)             // 10^9
                         / (ns_end - ns_begin);             // elapsed ns
    /* (TSC per us) = (TSC per sec.) / 10^6 */
    usersched_tsc_1us = usersched_tsc_freq / (1000 * 1000);
  }

  /* Check Invariant TSC support. */
  eax = 0x80000007;
  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
  usersched_support_invariant_tsc = edx & (1 << 8);

#if defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#error Both _FORCE_UMWAIT and _NO_UMWAIT are defined!
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
  if (!prohibit_umwait) {
#endif

#ifndef _NO_UMWAIT
    /* Check UMWAIT support. */
    eax = 7;
    ecx = 0;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(eax), "c"(ecx));
    usersched_support_umwait = !!(ecx & (1 << 5));
#else
  usersched_support_umwait = 0;
#endif

#ifdef _FORCE_UMWAIT
    log_abort_if_false(usersched_support_umwait);
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
  }
#endif
}

static _Thread_local pid_t _usersched_tid = 0;
pid_t usersched_gettid(void) {
  return unlikely(!_usersched_tid) ? (_usersched_tid = gettid())
                                   : _usersched_tid;
}
