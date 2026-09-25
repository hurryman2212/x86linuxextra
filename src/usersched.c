#include <sys/mman.h>
#include <sys/syscall.h>

#include <linux/perf_event.h>

#include "x86linux/helper.h"

int usersched_support_invariant_tsc;
int usersched_support_umwait;

uint64_t usersched_tsc_freq_hz = 1000 * 1000 * 1000;
uint32_t usersched_tsc_1us = 1000;

static int _usersched_diff_timespec_wide(const struct timespec *end,
                                         const struct timespec *start,
                                         __int128 *seconds, long *nanoseconds) {
  if (end->tv_nsec < 0 || end->tv_nsec >= 1000000000L || start->tv_nsec < 0 ||
      start->tv_nsec >= 1000000000L || end->tv_sec < start->tv_sec ||
      (end->tv_sec == start->tv_sec && end->tv_nsec < start->tv_nsec)) {
    errno = EINVAL;
    return -1;
  }
  /* Widen before subtracting seconds that may have opposite signs. */
  *seconds = (__int128)end->tv_sec - (__int128)start->tv_sec;
  *nanoseconds = end->tv_nsec - start->tv_nsec;
  if (*nanoseconds < 0) {
    --*seconds;
    *nanoseconds += 1000000000L;
  }
  return 0;
}

int usersched_diff_timespec(const struct timespec *end,
                            const struct timespec *start,
                            struct timespec *elapsed) noexcept {
  __int128 seconds;
  long nanoseconds;
  if (_usersched_diff_timespec_wide(end, start, &seconds, &nanoseconds))
    return -1;
  const time_t tv_sec = (time_t)seconds;
  if ((__int128)tv_sec != seconds) {
    errno = EOVERFLOW;
    return -1;
  }
  *elapsed = (struct timespec){.tv_sec = tv_sec, .tv_nsec = nanoseconds};
  return 0;
}

int usersched_gettsc_elapsed(const struct timespec *restrict end,
                             const struct timespec *restrict start,
                             uint64_t *restrict elapsed) noexcept {
  const uint64_t frequency = usersched_tsc_freq_hz;
  if (!frequency) {
    errno = EINVAL;
    return -1;
  }
  __int128 seconds;
  long nanoseconds;
  if (_usersched_diff_timespec_wide(end, start, &seconds, &nanoseconds))
    return -1;
  /* Split seconds and nanoseconds so even the full time_t range fits 128 bits.
   */
  const __uint128_t ticks =
      (__uint128_t)seconds * frequency +
      (__uint128_t)nanoseconds * frequency / 1000000000ULL;
  if (ticks > UINT64_MAX) {
    errno = EOVERFLOW;
    return -1;
  }
  *elapsed = (uint64_t)ticks;
  return 0;
}

int usersched_gettime_elapsed(uint64_t tsc_now, uint64_t tsc_past,
                              struct timespec *restrict elapsed) noexcept {
  const uint64_t frequency = usersched_tsc_freq_hz;
  if (!frequency) {
    errno = EINVAL;
    return -1;
  }
  const __uint128_t ns = (__uint128_t)usersched_diff_tsc(tsc_now, tsc_past) *
                         1000000000ULL / frequency;
  const __uint128_t seconds = ns / 1000000000ULL;
  const time_t tv_sec = (time_t)seconds;
  if (tv_sec < 0 || (__uint128_t)tv_sec != seconds) {
    errno = EOVERFLOW;
    return -1;
  }
  *elapsed = (struct timespec){
      .tv_sec = tv_sec,
      .tv_nsec = (long)(ns - (__uint128_t)tv_sec * 1000000000ULL),
  };
  return 0;
}

unsigned long long _usersched_schedule_start(uint32_t timeout_tsc) noexcept {
  /* Do not return UINT64_MAX and 0 for valid absolute TSC value! */

  /* Ignore expansive RDTSC instruction. */
  if (!timeout_tsc || (timeout_tsc == UINT32_MAX))
    return ((unsigned long long)timeout_tsc << 32) | timeout_tsc;

  const unsigned long long _tsc = x86_rdtsc() + timeout_tsc;
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
    const unsigned long long _tsc = x86_rdtsc();
#ifndef _USERSCHED_NO_UMWAIT
    _tsc_overflow = abs_timeout_tsc < _tsc;
#endif
    if ((int64_t)usersched_diff_tsc(abs_timeout_tsc, _tsc) <= 0)
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
        x86_umwait(uaddr32, oldval32, 0,
                   _indefinite || unlikely(_tsc_overflow) ? UINT64_MAX
                                                          : abs_timeout_tsc);
        if (__atomic_load_n(uaddr32, __ATOMIC_ACQUIRE) != oldval32)
          return 1;
        if (!_indefinite &&
            (int64_t)usersched_diff_tsc(abs_timeout_tsc, x86_rdtsc()) <= 0)
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
          x86_pause();
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

  const int64_t _remaining =
      (int64_t)usersched_diff_tsc(abs_timeout_tsc, x86_rdtsc());
  return _remaining <= 0 ? 0 : (uint32_t)_remaining;
}

struct _usersched_lock_time {
  uint64_t start_tsc;
  struct timespec timeout;
  bool timed;
};

static uint64_t _usersched_lock_timestamp(void) {
  lfence();
  const uint64_t now = x86_rdtsc();
  lfence();
  return now;
}

static int _usersched_lock_time_init(struct _usersched_lock_time *time,
                                     int flags, uint32_t polling_tsc,
                                     const struct timespec *timeout,
                                     bool absolute) {
  if (timeout && (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
                  timeout->tv_nsec >= 1000000000L)) {
    errno = EINVAL;
    return -1;
  }
  *time = (struct _usersched_lock_time){0};
  if (timeout && (timeout->tv_sec || timeout->tv_nsec)) {
    if (!usersched_tsc_freq_hz) {
      errno = EINVAL;
      return -1;
    }
    time->timed = true;
  }
  if (time->timed && absolute) {
    struct timespec now;
    /* Anchor before the clock read. Any intervening preemption then consumes
     * the allowance instead of pairing a stale clock with a fresh TSC and
     * granting polling beyond the absolute deadline. The read interval is
     * conservatively deducted from the clock-derived remaining duration. */
    time->start_tsc = _usersched_lock_timestamp();
    if (clock_gettime(flags & FUTEX_CLOCK_REALTIME ? CLOCK_REALTIME
                                                   : CLOCK_MONOTONIC,
                      &now))
      return -1;
    if (timeout->tv_sec > now.tv_sec ||
        (timeout->tv_sec == now.tv_sec && timeout->tv_nsec > now.tv_nsec)) {
      if (usersched_diff_timespec(timeout, &now, &time->timeout))
        return -1;
    }
  } else {
    if ((polling_tsc && polling_tsc != UINT32_MAX) || time->timed)
      time->start_tsc = _usersched_lock_timestamp();
    if (time->timed)
      time->timeout = *timeout;
  }
  return 0;
}

static uint64_t
_usersched_lock_poll_deadline(uint64_t start, uint32_t polling_tsc,
                              const struct timespec *remaining) {
  if (!polling_tsc)
    return 0;
  if (!remaining && polling_tsc == UINT32_MAX)
    return UINT64_MAX;
  uint64_t budget = polling_tsc;
  if (remaining) {
    const __uint128_t ticks =
        (__uint128_t)remaining->tv_sec * usersched_tsc_freq_hz +
        (__uint128_t)remaining->tv_nsec * usersched_tsc_freq_hz / 1000000000ULL;
    if (polling_tsc == UINT32_MAX || ticks < budget)
      /* The polling helper compares signed TSC differences. */
      budget = ticks > INT64_MAX ? INT64_MAX : (uint64_t)ticks;
  }
  if (!budget)
    return 0;
  const uint64_t deadline = start + budget;
  /* Preserve the polling helper's zero/infinite sentinel values. */
  return !deadline ? 1 : deadline == UINT64_MAX ? UINT64_MAX - 1 : deadline;
}

static int _usersched_lock_remaining(const struct _usersched_lock_time *time,
                                     uint64_t now, struct timespec *remaining) {
  struct timespec elapsed;
  if (usersched_gettime_elapsed(now, time->start_tsc, &elapsed)) {
    if (errno != EOVERFLOW)
      return -1;
    /* Elapsed time exceeding time_t also exceeds any valid lock timeout. */
    errno = 0;
    *remaining = (struct timespec){0};
    return 0;
  }
  if (elapsed.tv_sec > time->timeout.tv_sec ||
      (elapsed.tv_sec == time->timeout.tv_sec &&
       elapsed.tv_nsec >= time->timeout.tv_nsec)) {
    *remaining = (struct timespec){0};
    return 0;
  }
  if (usersched_diff_timespec(&time->timeout, &elapsed, remaining))
    return -1;
  return 1;
}

static int _acquire_lock(volatile uint64_t *restrict lock, bool registered,
                         uint64_t *restrict observed) {
  *observed = __atomic_load_n(lock, __ATOMIC_RELAXED);
  if (*observed & _USERSCHED_LOCK_HELD)
    return 0;
  uint64_t desired = *observed | _USERSCHED_LOCK_HELD;
  if (registered)
    desired -= UINT64_C(1) << _USERSCHED_LOCK_WAITER_SHIFT;
  return __atomic_compare_exchange_n(lock, observed, desired, false,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}
/* Keep polling and kernel-wait state out of the immediate acquisition path. */
int _usersched_lock_deep(volatile uint64_t *restrict lock64, int flags,
                         uint32_t usersched_timeout_tsc,
                         const struct timespec *restrict lock_timeout,
                         uint64_t _lock_save) noexcept {
  if (flags & USERSCHED_LOCK_TRYLOCK) {
    /* Retain existing waiter registrations when an unlocked mutex is taken. */
    while (!(_lock_save & _USERSCHED_LOCK_HELD))
      if (__atomic_compare_exchange_n(lock64, &_lock_save,
                                      _lock_save | _USERSCHED_LOCK_HELD, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return 0;
    errno = EBUSY;
    return -1;
  }
  if (lock_timeout && !lock_timeout->tv_sec && !lock_timeout->tv_nsec) {
    /* The inline CAS can miss an unlocked word with registered waiters. */
    if (_acquire_lock(lock64, false, &_lock_save))
      return 0;
    errno = ETIMEDOUT;
    return -1;
  }

  volatile uint32_t *_lock = (typeof(_lock))lock64;
  struct _usersched_lock_time time;
  if (_usersched_lock_time_init(&time, flags, usersched_timeout_tsc,
                                lock_timeout, false))
    return -1;
  uint64_t polling_deadline = _usersched_lock_poll_deadline(
      time.start_tsc, usersched_timeout_tsc, time.timed ? &time.timeout : NULL);
  struct timespec remaining;

  bool registered = false;
  int err_num;
  for (;;) {
    do {
      if (_acquire_lock(lock64, registered, &_lock_save))
        return 0;
      /* A failed CAS on an unlocked word must retry, not sleep on zero.
       * Check the budget even when lock ownership changes continuously. */
    } while (polling_deadline &&
             _usersched_reschedule(polling_deadline, (uint32_t)_lock_save,
                                   _lock_save & _USERSCHED_LOCK_HELD ? _lock
                                                                     : NULL));
    polling_deadline = 0;

    if (!registered) {
      /* Expiry before registration requires no futex wait or wake. */
      const int time_left =
          time.timed ? _usersched_lock_remaining(
                           &time, _usersched_lock_timestamp(), &remaining)
                     : 1;
      if (time_left <= 0) {
        if (!time_left)
          errno = ETIMEDOUT;
        goto fail;
      }
      _lock_save = __atomic_load_n(lock64, __ATOMIC_RELAXED);
      for (;;) {
        if ((_lock_save >> _USERSCHED_LOCK_WAITER_SHIFT) == UINT32_MAX) {
          errno = EOVERFLOW;
          goto fail;
        }
        if (__atomic_compare_exchange_n(
                lock64, &_lock_save,
                _lock_save + (UINT64_C(1) << _USERSCHED_LOCK_WAITER_SHIFT),
                false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
          break;
      }
      registered = true;
    }

    if (_acquire_lock(lock64, true, &_lock_save))
      return 0;
    if (!(_lock_save & _USERSCHED_LOCK_HELD))
      continue;

    do {
      if (time.timed) {
        const int time_left = _usersched_lock_remaining(
            &time, _usersched_lock_timestamp(), &remaining);
        if (time_left <= 0) {
          if (!time_left)
            errno = ETIMEDOUT;
          goto fail;
        }
      }
      /* Registration precedes the atomic futex value check, so an unlock
       * either wakes us or makes this wait observe an unlocked word. */
      const int _ret = syscall(
          SYS_futex, _lock,
          FUTEX_WAIT | (flags & (FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)),
          _USERSCHED_LOCK_HELD, time.timed ? &remaining : lock_timeout);
      const int wait_errno = _ret ? errno : 0;
      /* Waking need not imply ownership. Even timeout/interruption may race an
       * unlock; one final userspace attempt can consume our registration. */
      if ((!_ret || wait_errno == EAGAIN || wait_errno == EINTR ||
           wait_errno == ETIMEDOUT) &&
          _acquire_lock(lock64, true, &_lock_save)) {
        errno = 0;
        return 0;
      }
      if (_ret && !(wait_errno == EAGAIN && flags & USERSCHED_LOCK_NOEAGAIN) &&
          !(wait_errno == EINTR && flags & SA_RESTART)) {
        errno = wait_errno;
        goto fail;
      }
      errno = 0;
      if ((flags & USERSCHED_LOCK_RESTART) && usersched_timeout_tsc) {
        const uint64_t now = time.timed || usersched_timeout_tsc != UINT32_MAX
                                 ? _usersched_lock_timestamp()
                                 : 0;
        if (time.timed) {
          const int time_left =
              _usersched_lock_remaining(&time, now, &remaining);
          if (time_left <= 0) {
            if (!time_left)
              errno = ETIMEDOUT;
            goto fail;
          }
        }
        polling_deadline = _usersched_lock_poll_deadline(
            now, usersched_timeout_tsc, time.timed ? &remaining : NULL);
      }
      /* With no renewed poll, the last failed acquisition already tried;
       * recompute the remaining duration just before the next futex wait. */
    } while (!polling_deadline && (_lock_save & _USERSCHED_LOCK_HELD));
  }

fail:
  err_num = errno;
  if (registered) {
    const uint64_t previous = __atomic_fetch_sub(
        lock64, UINT64_C(1) << _USERSCHED_LOCK_WAITER_SHIFT, __ATOMIC_ACQ_REL);
    /* Pass on a wake that this departing waiter may have consumed. */
    if (!(previous & _USERSCHED_LOCK_HELD) &&
        (previous >> _USERSCHED_LOCK_WAITER_SHIFT) > 1)
      syscall(SYS_futex, _lock, FUTEX_WAKE | (flags & FUTEX_PRIVATE_FLAG), 1);
  }
  errno = err_num;
  return -1;
}
int _usersched_unlock_deep(volatile uint64_t *restrict lock64,
                           int flags) noexcept {
  return syscall(SYS_futex, (volatile uint32_t *)lock64,
                 FUTEX_WAKE | (flags & FUTEX_PRIVATE_FLAG), 1);
}

int _usersched_lock_pi2_deep(volatile uint32_t *restrict lock, pid_t tid,
                             int flags, uint32_t usersched_timeout_tsc,
                             const struct timespec *restrict lock_timeout,
                             uint32_t _lock_save) noexcept {
  if ((_lock_save & FUTEX_TID_MASK) == (uint32_t)tid) {
    errno = EDEADLK;
    return -1;
  }
  if (flags & USERSCHED_LOCK_TRYLOCK) {
    errno = EBUSY;
    return -1;
  }
  if (lock_timeout && !lock_timeout->tv_sec && !lock_timeout->tv_nsec) {
    errno = ETIMEDOUT;
    return -1;
  }

  struct _usersched_lock_time time;
  if (_usersched_lock_time_init(&time, flags, usersched_timeout_tsc,
                                lock_timeout, true))
    return -1;
  uint64_t polling_deadline = _usersched_lock_poll_deadline(
      time.start_tsc, usersched_timeout_tsc, time.timed ? &time.timeout : NULL);
  for (;;) {
    do {
      _lock_save = __sync_val_compare_and_swap(lock, 0, tid);
      if (!_lock_save)
        return 0;
      if ((_lock_save & FUTEX_TID_MASK) == (uint32_t)tid) {
        errno = EDEADLK;
        return -1;
      }
    } while (polling_deadline &&
             _usersched_reschedule(polling_deadline, _lock_save, lock));
    polling_deadline = 0;

    do {
      if (time.timed) {
        struct timespec remaining;
        const int time_left = _usersched_lock_remaining(
            &time, _usersched_lock_timestamp(), &remaining);
        if (time_left <= 0) {
          if (!time_left)
            errno = ETIMEDOUT;
          return -1;
        }
      }
      /* Polling exhausted; use the kernel wait. */
      const int _ret =
          syscall(SYS_futex, lock,
                  FUTEX_LOCK_PI2 |
                      (flags & (FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)),
                  0, lock_timeout);
      if (!_ret)
        return 0;
      const int wait_errno = errno;
      if ((wait_errno == EAGAIN || wait_errno == EINTR ||
           wait_errno == ETIMEDOUT) &&
          !__sync_val_compare_and_swap(lock, 0, tid)) {
        errno = 0;
        return 0;
      }
      if (!(wait_errno == EAGAIN && flags & USERSCHED_LOCK_NOEAGAIN) &&
          !(wait_errno == EINTR && flags & SA_RESTART)) {
        errno = wait_errno;
        return -1;
      }
      errno = 0;
      if ((flags & USERSCHED_LOCK_RESTART) && usersched_timeout_tsc) {
        const uint64_t now = time.timed || usersched_timeout_tsc != UINT32_MAX
                                 ? _usersched_lock_timestamp()
                                 : 0;
        struct timespec remaining;
        if (time.timed) {
          const int time_left =
              _usersched_lock_remaining(&time, now, &remaining);
          if (time_left <= 0) {
            if (!time_left)
              errno = ETIMEDOUT;
            return -1;
          }
        }
        polling_deadline = _usersched_lock_poll_deadline(
            now, usersched_timeout_tsc, time.timed ? &remaining : NULL);
      }
      /* With no renewed poll, the last failed CAS already tried acquisition. */
    } while (!polling_deadline);
  }
}
int _usersched_unlock_pi_deep(volatile uint32_t *restrict lock,
                              int flags) noexcept {
  return syscall(SYS_futex, lock,
                 flags & FUTEX_PRIVATE_FLAG ? FUTEX_UNLOCK_PI_PRIVATE
                                            : FUTEX_UNLOCK_PI);
}

const sigset_t _fset = {
    .__val = {[0 ... sizeof_elem(_fset, __val) / sizeof_elem(_fset, __val, *) -
               1] = (typeof_elem(_fset, __val, *))UINT64_MAX}};
thread_local __attribute((tls_model("initial-exec"))) sigset_t _oset;
int usersched_plock(volatile uint64_t *restrict lock64, int flags,
                    uint32_t usersched_timeout_tsc,
                    const struct timespec *restrict lock_timeout,
                    const sigset_t *restrict set,
                    sigset_t *restrict oldset) noexcept {
  sigset_t _oldset;
  int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset, &_oldset);
  if (_ret) {
    errno = _ret;
    return -1;
  }

  _ret = usersched_lock(lock64, flags, usersched_timeout_tsc, lock_timeout);
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
                        const struct timespec *restrict lock_timeout,
                        const sigset_t *restrict set,
                        sigset_t *restrict oldset) noexcept {
  sigset_t _oldset;
  int _ret = pthread_sigmask(SIG_SETMASK, set ? set : &_fset, &_oldset);
  if (_ret) {
    errno = _ret;
    return -1;
  }

  _ret =
      usersched_lock_pi2(lock, tid, flags, usersched_timeout_tsc, lock_timeout);
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
      log_msg(LOG_DEBUG, "Using the faster path with perf_event_open()...");

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
            x86_pause();
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
      log_msg(LOG_DEBUG, "Using the slower (and imprecise) path with "
                         "clock_gettime()...");

      /* Check RDTSCP support which is mandatory in this path. */
      {
        uint32_t _eax = 0x80000001, _edx;
        x86_cpuid(&_eax, NULL, NULL, &_edx);
        if (!(_edx & 1 << 27)) {
          return _usersched_init_fail(ENOTSUP);
        }
      }

      unsigned int _tsc_aux_start, _tsc_aux_end;
      unsigned long long _tsc_begin, _tsc_end;
      struct timespec _time_begin, _time_end;
      do {
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &_time_begin) == -1) {
          return _usersched_init_fail(errno);
        }
        _tsc_begin = x86_rdtscp(&_tsc_aux_start);

        /* Sleep 10ms (probably enough to get accurate TSC value for 1us). */
        if (usleep(10000) == -1) {
          return _usersched_init_fail(errno);
        }

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &_time_end) == -1) {
          return _usersched_init_fail(errno);
        }
        _tsc_end = x86_rdtscp(&_tsc_aux_end);

        /* Check if core migration is happend. */
      } while (_tsc_aux_end != _tsc_aux_start);

      struct timespec _elapsed;
      if (usersched_diff_timespec(&_time_end, &_time_begin, &_elapsed))
        return _usersched_init_fail(errno);
      const __uint128_t _elapsed_ns =
          (__uint128_t)_elapsed.tv_sec * 1000000000ULL + _elapsed.tv_nsec;
      if (!_elapsed_ns)
        return _usersched_init_fail(EIO);

      /* (TSC freq.) = (TSC per sec.) = (elapsed TSC) * 10^9 / (elapsed ns) */
      usersched_tsc_freq_hz =
          (__uint128_t)usersched_diff_tsc(_tsc_end, _tsc_begin) // elapsed tsc
          * (1000 * 1000 * 1000)                                // 10^9
          / _elapsed_ns;
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
