#include "x86linux/helper.h"

#include <syscall.h>

#include <sys/mman.h>

#include <linux/perf_event.h>

int usersched_support_invariant_tsc;
int usersched_support_umwait;

uint64_t usersched_tsc_freq_hz = 1000 * 1000 * 1000;
uint32_t usersched_tsc_1us = 1000;

unsigned long long _user_schedule_start(uint32_t timeout_tsc) {
  /* Do not return UINT32_MAX and 0 for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!timeout_tsc || (timeout_tsc == UINT32_MAX))
    return ((unsigned long long)timeout_tsc << 32) | timeout_tsc;

  const unsigned long long __tsc = _rdtsc();
  if (unlikely((__tsc + timeout_tsc) <= __tsc))
    /* TSC has overflowed. */
    return (timeout_tsc - (UINT64_MAX - __tsc))
               ? (timeout_tsc - (UINT64_MAX - __tsc))
               : 1;
  return unlikely((__tsc + timeout_tsc) == UINT64_MAX) ? (UINT64_MAX - 1)
                                                       : (__tsc + timeout_tsc);
}
uint32_t _user_reschedule(unsigned long long abs_timeout_tsc, uint32_t oldval32,
                          const volatile uint32_t *restrict uaddr32) {
  /* Do not evaluate (*uaddr32 != oldval32 ) first! */

  /* Check immediate timeout. */
  if (!abs_timeout_tsc)
    return 0;

  /* Check non-indefinite timeout. */
  const int __indefinite = abs_timeout_tsc == UINT64_MAX;
  int __tsc_overflow = 0;
  if (!__indefinite) {
    const unsigned long long __tsc = _rdtsc();
    __tsc_overflow = !!((abs_timeout_tsc ^ __tsc) & INT64_MIN);
    if (unlikely(__tsc_overflow)) {
      if (abs_timeout_tsc & INT64_MIN)
        /* Timeout expired; Return 0. */
        return 0;
    } else if (abs_timeout_tsc <= __tsc)
      /* Timeout expired; Return 0. */
      return 0;
  }

  /* Start snooping if uaddr32 is non-NULL. */
  if (uaddr32) {
    /* Busy loop optimization: Do PAUSE or UMWAIT. */

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
    if (usersched_support_umwait) {
#endif

#ifndef _NO_UMWAIT
      /* Check TSC overflow first. */
      if (__indefinite || unlikely(__tsc_overflow))
        /* Should we use `control` value as `0` here? */
        while (X86_UMWAIT(uaddr32, oldval32, 0, UINT64_MAX) ||
               (*uaddr32 == oldval32 && __indefinite))
          ;

      /* Should we use `control` value as `0` here? */
      while (X86_UMWAIT(uaddr32, oldval32, 0, abs_timeout_tsc))
        ;

      if (*uaddr32 == oldval32)
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
        if (*uaddr32 == oldval32)
          _mm_pause();
        else
          return 1;
      while (__indefinite);
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
    }
#endif
  }

  return 1;
}
uint32_t _user_update_timeout_tsc(unsigned long long abs_timeout_tsc) {
  /* Do not return UINT64_MAX for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!abs_timeout_tsc || (abs_timeout_tsc == UINT64_MAX))
    return (uint32_t)abs_timeout_tsc;

  const unsigned long long __tsc = _rdtsc();
  if (unlikely((abs_timeout_tsc ^ __tsc) & INT64_MIN))
    /* TSC has overflowed. */
    return (abs_timeout_tsc & INT64_MIN)
               ? 0
               : (abs_timeout_tsc + (UINT64_MAX - __tsc));
  return (abs_timeout_tsc <= __tsc) ? 0 : (abs_timeout_tsc - __tsc);
}

static int _check_lock(volatile uint32_t *restrict enter_nr,
                       uint32_t *restrict enter_nr_save,
                       uint32_t local_wait_nr) {
  return (*enter_nr_save = *enter_nr) == local_wait_nr;
}
int usersched_lock(volatile uint64_t *restrict lock64, int flags,
                   uint32_t user_timeout_tsc,
                   const struct timespec *restrict kernel_timeout) {
  const uint32_t __user_timeout_tsc_save = user_timeout_tsc;

  volatile uint32_t *const restrict __enter_nr = (typeof(__enter_nr))lock64;
  volatile uint32_t *const restrict __wait_nr = __enter_nr + 1;

  /* Save `*__wait_nr`, then increment it. */
  const uint32_t __local_wait_nr = __sync_fetch_and_add(__wait_nr, 1);

  uint32_t __enter_nr_save;
  while (!_check_lock(__enter_nr, &__enter_nr_save, __local_wait_nr)) {
    /* Failed; Use usersched. */
    user_schedule(user_timeout_tsc, USERSCHED_COND_SCHEDULE) {
      if (_check_lock(__enter_nr, &__enter_nr_save, __local_wait_nr))
        return 0;
    }
    user_reschedule(&user_timeout_tsc, __enter_nr, __enter_nr_save);

    if (!_check_lock(__enter_nr, &__enter_nr_save, __local_wait_nr)) {
      if (syscall(SYS_futex, __enter_nr,
                  flags & FUTEX_PRIVATE_FLAG ? FUTEX_WAIT_BITSET_PRIVATE
                                             : FUTEX_WAIT_BITSET,
                  __enter_nr_save, kernel_timeout, NULL,
                  1 << (__local_wait_nr % 32)) == -1 &&
          (errno != EAGAIN && (!(flags & SA_RESTART) || errno != EINTR)))
        return -1;

      errno = 0;
    }

    if (flags & USERSCHED_RESTART)
      user_timeout_tsc = __user_timeout_tsc_save;
  }

  return 0;
}
int usersched_unlock(volatile uint64_t *restrict lock64, int flags) {
  uint32_t *const restrict __enter_nr = (typeof(__enter_nr))lock64;
  volatile uint32_t *const restrict __wait_nr = __enter_nr + 1;

  /* Increment `*__enter_nr`, then save it. */
  const uint32_t __local_enter_nr = __sync_add_and_fetch(__enter_nr, 1);

  if (*__wait_nr != __local_enter_nr)
    return syscall(SYS_futex, __enter_nr,
                   flags & FUTEX_PRIVATE_FLAG ? FUTEX_WAKE_BITSET_PRIVATE
                                              : FUTEX_WAKE_BITSET,
                   INT_MAX, NULL, NULL, 1 << (__local_enter_nr % 32));
  return 0;
}

static int _acquire_lock_pi(volatile uint32_t *restrict lock,
                            uint32_t *restrict lock_save, pid_t tid) {
  return (*lock_save = __sync_val_compare_and_swap(lock, 0, tid)) == 0 ||
         (*lock_save & FUTEX_TID_MASK) == tid;
}
int usersched_lock_pi(volatile uint32_t *restrict lock, int flags,
                      uint32_t user_timeout_tsc,
                      const struct timespec *restrict kernel_timeout) {
  pid_t __tid = cached_gettid();
  const uint32_t __user_timeout_tsc_save = user_timeout_tsc;
  uint32_t __lock_save;
  while (!_acquire_lock_pi(lock, &__lock_save, __tid)) {
    /* Failed; Use usersched. */
    user_schedule(user_timeout_tsc, USERSCHED_COND_SCHEDULE) {
      if (_acquire_lock_pi(lock, &__lock_save, __tid))
        return 0;
    }
    user_reschedule(&user_timeout_tsc, lock, __lock_save);

    if (!_acquire_lock_pi(lock, &__lock_save, __tid)) {
      if (syscall(SYS_futex, lock,
                  flags & FUTEX_PRIVATE_FLAG ? FUTEX_LOCK_PI_PRIVATE
                                             : FUTEX_LOCK_PI,
                  0, kernel_timeout) == -1 &&
          errno != EAGAIN) // No need to check EINTR here.
        return -1;

      errno = 0;

      if ((*lock & FUTEX_TID_MASK) == __tid)
        return 0;

      if (flags & USERSCHED_RESTART)
        user_timeout_tsc = __user_timeout_tsc_save;
    }
  }

  return 0;
}
int usersched_unlock_pi(volatile uint32_t *restrict lock, int flags) {
  pid_t __tid = cached_gettid();
  if (__sync_val_compare_and_swap(lock, __tid, 0) != __tid)
    return syscall(SYS_futex, lock,
                   flags & FUTEX_PRIVATE_FLAG ? FUTEX_UNLOCK_PI_PRIVATE
                                              : FUTEX_UNLOCK_PI);
  return 0;
}

const sigset_t _fset = {
    .__val = {[0 ... sizeof_elem(_fset, __val) / sizeof_elem(_fset, __val, *) -
               1] = (typeof_elem(_fset, __val, *))UINT64_MAX}};
thread_local __attribute((tls_model("initial-exec"))) sigset_t _oset;
int usersched_plock(volatile uint64_t *restrict lock64, int flags,
                    uint32_t user_timeout_tsc,
                    const struct timespec *restrict kernel_timeout,
                    const sigset_t *restrict set, sigset_t *restrict oldset) {
  int __ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset,
                              oldset ? oldset : &_oset);
  if (__ret) {
    errno = __ret;
    return -1;
  }

  return usersched_lock(lock64, flags, user_timeout_tsc, kernel_timeout);
}
int usersched_punlock(volatile uint64_t *restrict lock64, int flags,
                      const sigset_t *restrict set) {
  if (usersched_unlock(lock64, flags) == -1)
    return -1;

  int __ret = pthread_sigmask(SIG_SETMASK, set ? set : &_oset, NULL);
  if (__ret) {
    errno = __ret;
    return -1;
  }
  return 0;
}
int usersched_plock_pi(volatile uint32_t *restrict lock, int flags,
                       uint32_t user_timeout_tsc,
                       const struct timespec *restrict kernel_timeout,
                       const sigset_t *restrict set,
                       sigset_t *restrict oldset) {
  int __ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset,
                              oldset ? oldset : &_oset);
  if (__ret) {
    errno = __ret;
    return -1;
  }

  return usersched_lock_pi(lock, flags, user_timeout_tsc, kernel_timeout);
}
int usersched_punlock_pi(volatile uint32_t *restrict lock, int flags,
                         const sigset_t *restrict set) {
  if (usersched_unlock_pi(lock, flags) == -1)
    return -1;

  int __ret = pthread_sigmask(SIG_SETMASK, set ? set : &_oset, NULL);
  if (__ret) {
    errno = __ret;
    return -1;
  }
  return 0;
}

/* Initialization */

static volatile int _usersched_inited;
int usersched_init(int inhibit_umwait) {
  if (!__sync_val_compare_and_swap(&_usersched_inited, 0, -1)) {
    /* Get the TSC frequency. */
    struct perf_event_attr __attr = {
        .config = PERF_COUNT_HW_INSTRUCTIONS,
        .disabled = 1,
        .exclude_callchain_kernel = 1,
        .exclude_callchain_user = 1,
        .exclude_guest = 1,
        .exclude_host = 1,
        .exclude_hv = 1,
        .exclude_idle = 1,
        .exclude_kernel = 1,
        .exclude_user = 1,
        .size = sizeof(struct perf_event_attr),
        .type = PERF_TYPE_HARDWARE,
    };
    int __use_fast_path = 1,
        __fd = syscall(SYS_perf_event_open, &__attr, 0, -1, -1, 0);
    /* Check if perf_event_open() system call has succeeded. */
    if (__fd == -1)
      __use_fast_path = 0;
    else {
      /* Use the faster path. */
      log(LOG_DEBUG, "Using the faster path with perf_event_open()...");

      struct perf_event_mmap_page *__page;
      const size_t __page_size = sysconf(_SC_PAGESIZE);
      log_verify_error(
          __page = mmap(NULL, __page_size, PROT_READ, MAP_SHARED, __fd, 0));

      /* Check the user time support. */
      if (!__page->cap_user_time) {
        log(LOG_DEBUG, "!cap_user_time: Giving up...");
        __use_fast_path = 0;
      } else {
        usersched_tsc_freq_hz =
            ((__uint128_t)(1000 * 1000 * 1000) << __page->time_shift) /
            __page->time_mult;
        /* (TSC per us) = (TSC per sec.) / 10^6 */
        usersched_tsc_1us = usersched_tsc_freq_hz / (1000 * 1000);
      }

      /* Clean up. */
      log_verify_error(munmap(__page, __page_size));
      log_verify_error(close(__fd));
    }

    if (!__use_fast_path) {
      /* Use the slower (and imprecise) path. */
      log(LOG_DEBUG,
          "Using the slower (and imprecise) path with clock_gettime()...");

      /* Check RDTSCP support which is mandatory in this path. */
      {
        uint32_t __eax = 0x80000001, __edx;
        x86_cpuid(&__eax, NULL, NULL, &__edx);
        log_verify(!!(__edx & 1 << 27));
      }

      unsigned int __tsc_aux_start, __tsc_aux_end;
      unsigned long long __tsc_begin, __tsc_end, __ns_begin, __ns_end;
      do {
        struct timespec ts;

        log_verify_error(clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
        __ns_begin = (unsigned long long)ts.tv_sec * 1000000000ull + ts.tv_nsec;
        __tsc_begin = __rdtscp(&__tsc_aux_start);

        /* Sleep 10ms (probably enough to get accurate TSC value for 1us). */
        usleep(10000);

        log_verify_error(clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
        __ns_end = (unsigned long long)ts.tv_sec * 1000000000ull + ts.tv_nsec;
        __tsc_end = __rdtscp(&__tsc_aux_end);

        /* Check if core migration is happend. */
      } while (__tsc_aux_end != __tsc_aux_start);

      /* (TSC freq.) = (TSC per sec.) = (elapsed TSC) * 10^9 / (elapsed ns) */
      usersched_tsc_freq_hz =
          ((__uint128_t)__tsc_end - __tsc_begin) // elapsed tsc
          * (1000 * 1000 * 1000)                 // 10^9
          / (__ns_end - __ns_begin);             // elapsed ns
      /* (TSC per us) = (TSC per sec.) / 10^6 */
      usersched_tsc_1us = usersched_tsc_freq_hz / (1000 * 1000);
    }

    /* Check Invariant TSC support. */
    {
      uint32_t __eax = 0x80000007, __edx;
      x86_cpuid(&__eax, NULL, NULL, &__edx);
      usersched_support_invariant_tsc = !!(__edx & (1 << 8));
    }

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
    if (!inhibit_umwait) {
#endif

#ifdef _NO_UMWAIT
      usersched_support_umwait = 0;
#else
    /* Check UMWAIT support. */
    {
      uint32_t __eax = 7, __ecx = 0;
      x86_cpuidex(&__eax, NULL, &__ecx, NULL);
      usersched_support_umwait = !!(__ecx & (1 << 5));
    }
#endif

#ifdef _FORCE_UMWAIT
      log_abort_if_false(usersched_support_umwait);
#endif

#if !defined(_FORCE_UMWAIT) && !defined(_NO_UMWAIT)
    }
#endif

    _usersched_inited = 1;
  }

  return _usersched_inited;
}
