#include <sys/mman.h>
#include <sys/syscall.h>

#include <linux/perf_event.h>

#include "x86linux/helper.h"

int usersched_support_invariant_tsc;
int usersched_support_umwait;

uint64_t usersched_tsc_freq_hz = 1000 * 1000 * 1000;
uint32_t usersched_tsc_1us = 1000;

unsigned long long _usersched_schedule_start(uint32_t timeout_tsc) noexcept {
  /* Do not return UINT64_MAX and 0 for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!timeout_tsc || (timeout_tsc == UINT32_MAX))
    return ((unsigned long long)timeout_tsc << 32) | timeout_tsc;

  const unsigned long long _tsc = _rdtsc() + timeout_tsc;
  return unlikely(!_tsc) ? 1 : (_tsc == UINT64_MAX ? UINT64_MAX - 1 : _tsc);
}
uint32_t
_usersched_reschedule(unsigned long long abs_timeout_tsc, uint32_t oldval32,
                      const volatile uint32_t *restrict uaddr32) noexcept {
  /* Do not evaluate (*uaddr32 != oldval32 ) first! */

  /* Check immediate timeout. */
  if (!abs_timeout_tsc)
    return 0;

  /* Check non-indefinite timeout. */
  const int _indefinite = abs_timeout_tsc == UINT64_MAX;
#ifndef _USERSCHED_NO_UMWAIT
  int _tsc_overflow = 0;
#endif
  if (!_indefinite) {
    const unsigned long long _tsc = _rdtsc();
#ifndef _USERSCHED_NO_UMWAIT
    _tsc_overflow = abs_timeout_tsc < _tsc;
#endif
    if ((int64_t)(abs_timeout_tsc - _tsc) <= 0)
      /* Timeout expired; Return 0. */
      return 0;
  }

  /* Start snooping if uaddr32 is non-NULL. */
  if (uaddr32) {
    /* Busy loop optimization: Do PAUSE or UMWAIT. */

#if !defined(_USERSCHED_FORCE_UMWAIT) && !defined(_USERSCHED_NO_UMWAIT)
    if (usersched_support_umwait) {
#endif

#ifndef _USERSCHED_NO_UMWAIT
      do {
        /* UMWAIT may return before its deadline, even with a cleared CF. */
        x86_umwait(uaddr32, oldval32, 0,
                   _indefinite || unlikely(_tsc_overflow) ? UINT64_MAX
                                                          : abs_timeout_tsc);
        if (__atomic_load_n(uaddr32, __ATOMIC_ACQUIRE) != oldval32)
          return 1;
        if (!_indefinite && (int64_t)(abs_timeout_tsc - _rdtsc()) <= 0)
          return 0;
      } while (_indefinite);
      return 1;
#endif

#if !defined(_USERSCHED_FORCE_UMWAIT) && !defined(_USERSCHED_NO_UMWAIT)
    } else {
#endif

#ifndef _USERSCHED_FORCE_UMWAIT
      do
        if (__atomic_load_n(uaddr32, __ATOMIC_ACQUIRE) == oldval32)
          _mm_pause();
        else
          return 1;
      while (_indefinite);
#endif

#if !defined(_USERSCHED_FORCE_UMWAIT) && !defined(_USERSCHED_NO_UMWAIT)
    }
#endif
  }

  return 1;
}
uint32_t
_usersched_update_timeout_tsc(unsigned long long abs_timeout_tsc) noexcept {
  /* Do not return UINT64_MAX for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!abs_timeout_tsc || (abs_timeout_tsc == UINT64_MAX))
    return (uint32_t)abs_timeout_tsc;

  const int64_t _remaining = abs_timeout_tsc - _rdtsc();
  return _remaining <= 0 ? 0 : (uint32_t)_remaining;
}

static __attribute((nonnull(1))) int
_acquire_lock(volatile uint32_t *restrict lock) {
  /* Keep the contended state when acquiring so remaining waiters are woken. */
  return __atomic_exchange_n(lock, 2, __ATOMIC_ACQUIRE) == 0;
}
int usersched_lock(volatile uint64_t *restrict lock64, int flags,
                   uint32_t usersched_timeout_tsc,
                   const struct timespec *restrict kernel_timeout) noexcept {
  const uint32_t _usersched_timeout_tsc_save = usersched_timeout_tsc;

  volatile uint32_t *_lock = (typeof(_lock))lock64;

  /* 0: unlocked, 1: locked, 2: locked with possible waiters. */
  if (!__sync_val_compare_and_swap(_lock, 0, 1))
    return 0;

  while (!_acquire_lock(_lock)) { // Early trial.
    /* Failed; Use usersched. */
    usersched_schedule(usersched_timeout_tsc, USERSCHED_COND_SCHEDULE) {
      if (_acquire_lock(_lock))
        return 0;
    }
    usersched_reschedule(&usersched_timeout_tsc, _lock, 2);

    /* Usersched failed; Use the real system call. */
    if (syscall(SYS_futex, _lock,
                FUTEX_WAIT_BITSET |
                    (flags & (FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)),
                2, kernel_timeout, NULL, FUTEX_BITSET_MATCH_ANY) &&
        !(errno == EAGAIN && flags & USERSCHED_NOEAGAIN) &&
        !(errno == EINTR && flags & SA_RESTART))
      return -1;
    errno = 0;

    if (flags & USERSCHED_RESTART)
      usersched_timeout_tsc = _usersched_timeout_tsc_save;
  }

  return 0;
}
int usersched_unlock(volatile uint64_t *restrict lock64, int flags) noexcept {
  volatile uint32_t *_lock = (typeof(_lock))lock64;
  if (__atomic_exchange_n(_lock, 0, __ATOMIC_RELEASE) == 2)
    return syscall(SYS_futex, _lock, FUTEX_WAKE | (flags & FUTEX_PRIVATE_FLAG),
                   1);
  return 0;
}

static __attribute((nonnull(1, 3))) int
_acquire_lock_pi(volatile uint32_t *restrict lock, pid_t tid,
                 uint32_t *restrict lock_save) {
  return (*lock_save = __sync_val_compare_and_swap(lock, 0, tid)) == 0;
}
int usersched_lock_pi2(
    volatile uint32_t *restrict lock, pid_t tid, int flags,
    uint32_t usersched_timeout_tsc,
    const struct timespec *restrict kernel_timeout) noexcept {
  const uint32_t _usersched_timeout_tsc_save = usersched_timeout_tsc;
  if (tid <= 0 || ((uint32_t)tid & ~FUTEX_TID_MASK)) {
    errno = EINVAL;
    return -1;
  }
  uint32_t _lock_save;
  while (!_acquire_lock_pi(lock, tid, &_lock_save)) { // Early trial.
    if ((_lock_save & FUTEX_TID_MASK) == (uint32_t)tid) {
      errno = EDEADLK;
      return -1;
    }
    /* Failed; Use usersched. */
    usersched_schedule(usersched_timeout_tsc, USERSCHED_COND_SCHEDULE) {
      if (_acquire_lock_pi(lock, tid, &_lock_save))
        return 0;
    }
    usersched_reschedule(&usersched_timeout_tsc, lock, _lock_save);

    /* Usersched failed; Use the real system call. */
    const int _ret = syscall(
        SYS_futex, lock,
        FUTEX_LOCK_PI2 | (flags & (FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)),
        0, kernel_timeout);
    if (!_ret)
      break;
    else if (!(errno == EAGAIN &&
               flags & USERSCHED_NOEAGAIN)) // No need to check EINTR here.
      return -1;
    errno = 0;

    if (flags & USERSCHED_RESTART)
      usersched_timeout_tsc = _usersched_timeout_tsc_save;
  }

  return 0;
}
int usersched_unlock_pi(volatile uint32_t *restrict lock, pid_t tid,
                        int flags) noexcept {
  if (tid <= 0 || ((uint32_t)tid & ~FUTEX_TID_MASK)) {
    errno = EINVAL;
    return -1;
  }
  if (__sync_val_compare_and_swap(lock, tid, 0) != tid)
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
                    uint32_t usersched_timeout_tsc,
                    const struct timespec *restrict kernel_timeout,
                    const sigset_t *restrict set,
                    sigset_t *restrict oldset) noexcept {
  sigset_t _oldset;
  int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset, &_oldset);
  if (_ret) {
    errno = _ret;
    return -1;
  }

  _ret = usersched_lock(lock64, flags, usersched_timeout_tsc, kernel_timeout);
  if (_ret) {
    const int _errno = errno;
    pthread_sigmask(SIG_SETMASK, &_oldset, NULL);
    errno = _errno;
  } else
    *(oldset ? oldset : &_oset) = _oldset;
  return _ret;
}
int usersched_punlock(volatile uint64_t *restrict lock64, int flags,
                      const sigset_t *restrict set) noexcept {
  if (usersched_unlock(lock64, flags) == -1)
    return -1;

  const int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_oset, NULL);
  if (_ret) {
    errno = _ret;
    return -1;
  }
  return 0;
}
int usersched_plock_pi2(volatile uint32_t *restrict lock, pid_t tid, int flags,
                        uint32_t usersched_timeout_tsc,
                        const struct timespec *restrict kernel_timeout,
                        const sigset_t *restrict set,
                        sigset_t *restrict oldset) noexcept {
  sigset_t _oldset;
  int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset, &_oldset);
  if (_ret) {
    errno = _ret;
    return -1;
  }

  _ret = usersched_lock_pi2(lock, tid, flags, usersched_timeout_tsc,
                            kernel_timeout);
  if (_ret) {
    const int _errno = errno;
    pthread_sigmask(SIG_SETMASK, &_oldset, NULL);
    errno = _errno;
  } else
    *(oldset ? oldset : &_oset) = _oldset;
  return _ret;
}
int usersched_punlock_pi(volatile uint32_t *restrict lock, pid_t tid, int flags,
                         const sigset_t *restrict set) noexcept {
  if (usersched_unlock_pi(lock, tid, flags) == -1)
    return -1;

  const int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_oset, NULL);
  if (_ret) {
    errno = _ret;
    return -1;
  }
  return 0;
}

/* SPSC queue */

uint32_t _usersched_spsc_prepare_read(uint32_t *restrict pos_r,
                                      const volatile uint32_t *restrict pos_w,
                                      uint32_t pos_end, uint32_t size,
                                      uint32_t *restrict usersched_tsc,
                                      uint32_t *restrict pos_w_save) noexcept {
  uint32_t _rpeek = 0;

  /* Save initial `pos_w` value to local stack variable. */
  *pos_w_save = __atomic_load_n(pos_w, __ATOMIC_ACQUIRE);
  uint32_t _pos_w_save;
  if (!size)
    return 0;

  usersched_schedule(*usersched_tsc, USERSCHED_COND_SCHEDULE) {
    /* Compare the latest `pos_w` value with constant `pos_r` value. */
    _pos_w_save = __atomic_load_n(pos_w, __ATOMIC_ACQUIRE);
    spsc_rewind_read(0, pos_r, _pos_w_save, pos_end);
    if ((_rpeek = spsc_read_peek(*pos_r, _pos_w_save, pos_end, size)))
      usersched_cond_set(USERSCHED_COND_BREAK);
  }
  usersched_reschedule(usersched_tsc, pos_w, _pos_w_save);

  return _rpeek;
}
uint32_t _usersched_spsc_prepare_write(const volatile uint32_t *restrict pos_r,
                                       uint32_t *restrict pos_w,
                                       uint32_t pos_end, uint32_t size,
                                       uint32_t *restrict usersched_tsc,
                                       uint32_t *restrict pos_r_save) noexcept {
  uint32_t _wpeek = 0;

  /* Save initial `pos_r` value to local stack variable. */
  *pos_r_save = __atomic_load_n(pos_r, __ATOMIC_ACQUIRE);
  uint32_t _pos_r_save;
  if (!size)
    return 0;

  usersched_schedule(*usersched_tsc, USERSCHED_COND_SCHEDULE) {
    /* Compare the latest `pos_r` value with constant `pos_w` value. */
    _pos_r_save = __atomic_load_n(pos_r, __ATOMIC_ACQUIRE);
    spsc_rewind_write(0, _pos_r_save, pos_w, pos_end);
    if ((_wpeek = spsc_write_peek(_pos_r_save, *pos_w, pos_end, size)))
      usersched_cond_set(USERSCHED_COND_BREAK);
  }
  usersched_reschedule(usersched_tsc, pos_r, _pos_r_save);

  return _wpeek;
}

/* Initialization */

static volatile int _usersched_inited;
static int _usersched_init_fail(int error) {
  __atomic_store_n(&_usersched_inited, 0, __ATOMIC_RELEASE);
  errno = error ? error : EIO;
  return -1;
}
int usersched_init(int inhibit_umwait) noexcept {
  if (!__sync_val_compare_and_swap(&_usersched_inited, 0, -1)) {
    /* Get the TSC frequency. */
    struct perf_event_attr _attr = {
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
    int _use_fast_path = 1;
    const int _fd = syscall(SYS_perf_event_open, &_attr, 0, -1, -1, 0);
    /* Check if perf_event_open() system call has succeeded. */
    if (_fd == -1)
      _use_fast_path = 0;
    else {
      /* Use the faster path. */
      if (log_msg(LOG_DEBUG,
                  "Using the faster path with perf_event_open()...") == -1) {
        const int _errno_save = errno;
        (void)close(_fd);
        return _usersched_init_fail(_errno_save);
      }

      const long _page_size = sysconf(_SC_PAGESIZE);
      if (_page_size <= 0) {
        const int _errno_save = errno ? errno : EINVAL;
        (void)close(_fd);
        return _usersched_init_fail(_errno_save);
      }
      const struct perf_event_mmap_page *_page =
          mmap(NULL, _page_size, PROT_READ, MAP_SHARED, _fd, 0);
      if (_page == MAP_FAILED)
        _use_fast_path = 0;
      else {
        uint32_t _time_mult;
        uint16_t _time_shift;
        int _cap_user_time;
        for (;;) {
          const uint32_t _seq = __atomic_load_n(&_page->lock, __ATOMIC_ACQUIRE);
          if (_seq & 1) {
            _mm_pause();
            continue;
          }
          _cap_user_time = _page->cap_user_time;
          _time_mult = _page->time_mult;
          _time_shift = _page->time_shift;
          barrier();
          if (_seq == __atomic_load_n(&_page->lock, __ATOMIC_RELAXED))
            break;
        }

        /* Check the user time support and conversion parameters. */
        if (!_cap_user_time || !_time_mult || _time_shift >= 64)
          _use_fast_path = 0;
        else {
          const __uint128_t _freq =
              ((__uint128_t)(1000 * 1000 * 1000) << _time_shift) / _time_mult;
          if (_freq < 1000 * 1000 || _freq / (1000 * 1000) > UINT32_MAX)
            _use_fast_path = 0;
          else {
            usersched_tsc_freq_hz = _freq;
            /* (TSC per us) = (TSC per sec.) / 10^6 */
            usersched_tsc_1us = usersched_tsc_freq_hz / (1000 * 1000);
          }
        }

        if (munmap((void *)_page, (size_t)_page_size) == -1) {
          const int _errno_save = errno;
          (void)close(_fd);
          return _usersched_init_fail(_errno_save);
        }
      }

      /* Clean up. */
      if (close(_fd) == -1) {
        return _usersched_init_fail(errno);
      }
    }

    if (!_use_fast_path) {
      /* Use the slower (and imprecise) path. */
      if (log_msg(LOG_DEBUG, "Using the slower (and imprecise) path with "
                             "clock_gettime()...") == -1)
        return _usersched_init_fail(errno);

      /* Check RDTSCP support which is mandatory in this path. */
      {
        uint32_t _eax = 0x80000001, _edx;
        x86_cpuid(&_eax, NULL, NULL, &_edx);
        if (!(_edx & 1 << 27)) {
          return _usersched_init_fail(ENOTSUP);
        }
      }

      unsigned int _tsc_aux_start, _tsc_aux_end;
      unsigned long long _tsc_begin, _tsc_end, _ns_begin, _ns_end;
      do {
        struct timespec _ts;

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &_ts) == -1) {
          return _usersched_init_fail(errno);
        }
        _ns_begin =
            (unsigned long long)_ts.tv_sec * 1000000000ull + _ts.tv_nsec;
        _tsc_begin = __rdtscp(&_tsc_aux_start);

        /* Sleep 10ms (probably enough to get accurate TSC value for 1us). */
        if (usleep(10000) == -1) {
          return _usersched_init_fail(errno);
        }

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &_ts) == -1) {
          return _usersched_init_fail(errno);
        }
        _ns_end = (unsigned long long)_ts.tv_sec * 1000000000ull + _ts.tv_nsec;
        _tsc_end = __rdtscp(&_tsc_aux_end);

        /* Check if core migration is happend. */
      } while (_tsc_aux_end != _tsc_aux_start);

      /* (TSC freq.) = (TSC per sec.) = (elapsed TSC) * 10^9 / (elapsed ns) */
      usersched_tsc_freq_hz =
          (__uint128_t)(_tsc_end - _tsc_begin) // elapsed tsc
          * (1000 * 1000 * 1000)               // 10^9
          / (_ns_end - _ns_begin);             // elapsed ns
      /* (TSC per us) = (TSC per sec.) / 10^6 */
      usersched_tsc_1us = usersched_tsc_freq_hz / (1000 * 1000);
    }

    /* Check Invariant TSC support. */
    {
      uint32_t _eax = 0x80000007, _edx;
      x86_cpuid(&_eax, NULL, NULL, &_edx);
      usersched_support_invariant_tsc = !!(_edx & (1 << 8));
    }

#if !defined(_USERSCHED_FORCE_UMWAIT) && !defined(_USERSCHED_NO_UMWAIT)
    if (!inhibit_umwait) {
#endif

#ifdef _USERSCHED_NO_UMWAIT
      usersched_support_umwait = 0;
#else
    /* Check UMWAIT support. */
    {
      uint32_t _eax = 7, _ecx = 0;
      x86_cpuidex(&_eax, NULL, &_ecx, NULL);
      usersched_support_umwait = !!(_ecx & (1 << 5));
    }
#endif

#ifdef _USERSCHED_FORCE_UMWAIT
      if (!usersched_support_umwait) {
        return _usersched_init_fail(ENOTSUP);
      }
#endif

#if !defined(_USERSCHED_FORCE_UMWAIT) && !defined(_USERSCHED_NO_UMWAIT)
    }
#endif

    __atomic_store_n(&_usersched_inited, 1, __ATOMIC_RELEASE);
  }

  if (__atomic_load_n(&_usersched_inited, __ATOMIC_ACQUIRE) == 1)
    return 0;
  errno = EAGAIN;
  return -1;
}
