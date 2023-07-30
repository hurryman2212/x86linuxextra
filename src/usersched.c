#if defined(_FORCE_UMWAIT) && defined(_NO_UMWAIT)
#error Both _FORCE_UMWAIT and _NO_UMWAIT are defined!
#endif

#include "x86linux/helper.h"

#include <syscall.h>

#include <sys/mman.h>

#include <linux/perf_event.h>

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

uint32_t _user_reschedule(unsigned long long abs_timeout_tsc,
                          const volatile uint32_t *restrict uaddr32,
                          uint32_t old_val32) {
  /* Do not evaluate (*uaddr32 != old_val32 ) first! */

  /* Check immediate timeout. */
  if (!abs_timeout_tsc)
    return 0;

  do {
    unsigned long long tsc;
    int tsc_overflow;
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
    }
  } while (uaddr32);

  return 1;
}

uint32_t usersched_tsc_1us = 0;
int usersched_support_umwait = -1;
/* Setup global variables for 1us TSC value and UMWAIT support. */
static __attribute__((constructor)) void _usersched_constructor(void) {
  struct perf_event_attr pe = {.type = PERF_TYPE_HARDWARE,
                               .size = sizeof(struct perf_event_attr),
                               .config = PERF_COUNT_HW_INSTRUCTIONS,
                               .disabled = 1,
                               .exclude_kernel = 1,
                               .exclude_hv = 1};
  uint32_t eax, ebx, ecx, edx;

#ifdef _NO_UMWAIT
  log_warn("This build does NOT have UMWAIT support!");
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
  const char *env_no_umwait = getenv("USERSCHED_NO_UMWAIT");
  if (env_no_umwait) {
    log_warn("USERSCHED_NO_UMWAIT=%s -> UMWAIT will not be used!",
             env_no_umwait);
    usersched_support_umwait = 0;
  } else {
#endif

    /* Check UMWAIT support. */
    eax = 7;
    ecx = 0;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(eax), "c"(ecx));
    usersched_support_umwait = !!(ecx & (1 << 5));

    if (usersched_support_umwait)
      log_info("This system DOES support UMONITOR/UMWAIT instruction.");
    else
      log_info("This system does NOT support UMONITOR/UMWAIT instruction.");

#ifdef _FORCE_UMWAIT
    if (!usersched_support_umwait)
      log_abort(
          "No UMWAIT support present with UMWAIT-dependent build! Aborting...");
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
  }
#endif

  const char *env_override_tsc_1us = getenv("USERSCHED_OVERRIDE_TSC_1US");
  if (env_override_tsc_1us) {
    usersched_tsc_1us = atoi(env_override_tsc_1us);
    log_warn("USERSCHED_OVERRIDE_TSC_1US=%s -> Overriding TSC value "
             "representing 1us on this system to %u ticks...",
             env_override_tsc_1us, usersched_tsc_1us);
  }

  else {
    /**
     * Check Invariant TSC.
     *
     * If not available, give up.
     */
    eax = 0x80000007;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(eax));
    if (!(edx & (1 << 8))) {
      log("No invariant TSC support -> Giving up...");
      exit(EXIT_FAILURE);
    }

    /**
     * Check `perf_event_open` system call support.
     *
     * If not available, give up.
     */
    int fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd < 0)
      log_perror_abort("perf_event_open");

    ssize_t page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
      log_perror_abort("sysconf");

    struct perf_event_mmap_page *restrict pc =
        mmap(NULL, page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (pc == MAP_FAILED)
      log_perror_abort("mmap");

    /**
     * Check user time support.
     *
     * If not available, give up.
     */
    if (!pc->cap_user_time)
      log_abort("The system doesn't support user time!");

    unsigned int time_mult = pc->time_mult, time_shift = pc->time_shift;

    if (munmap(pc, page_size))
      log_perror_abort("munmap");
    if (close(fd))
      log_perror_abort("close");

    /* Dumb method */
    unsigned long long tsc_start, tsc_end;
    unsigned int tsc_aux_start, tsc_aux_end;
    do {
      tsc_start = __rdtscp(&tsc_aux_start);
      usleep(USERSCHED_TSC_1US_SLEEP_US);
      tsc_end = __rdtscp(&tsc_aux_end);
      /* Check core migration. */
    } while ((tsc_aux_end != tsc_aux_start));
    __uint128_t tmp_tsc_ns;
    if (unlikely(tsc_end <= tsc_start))
      /* TSC overflow */
      tmp_tsc_ns = tsc_end + (UINT64_MAX - tsc_start);
    else
      tmp_tsc_ns = tsc_end - tsc_start;
    tmp_tsc_ns *= time_mult;
    tmp_tsc_ns >>= time_shift;

    usersched_tsc_1us = (tsc_end - tsc_start) / (tmp_tsc_ns / 1000);
  }

  log_info("usersched_tsc_1us = %u", usersched_tsc_1us);
}
