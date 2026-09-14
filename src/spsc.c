#ifndef __KERNEL__

#include <string.h>

#endif

#include "x86linux/helper.h"

uint32_t spsc_read_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                        uint32_t size) noexcept {
  uint32_t _ret;
  if (pos_r <= pos_w) {
    if ((pos_w - pos_r) < size)
      _ret = pos_w - pos_r;
    else
      _ret = size;
  } else {
    /* Write position is behind Read position. */
    if ((pos_end - pos_r) < size)
      /* Rewind might be needed. */
      _ret = pos_end - pos_r;
    else
      _ret = size;
  }
  return _ret;
}
uint32_t spsc_write_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                         uint32_t size) noexcept {
  uint32_t _ret;
  if (pos_w < pos_r) {
    if ((pos_r - pos_w - 1) < size)
      _ret = pos_r - pos_w - 1; // Do not allow overlapping!
    else
      _ret = size;
  } else {
    /* Read position is behind or same as Write position. */
    if ((pos_end - pos_w) < size)
      /* Rewind might be needed. */
      _ret = pos_end - pos_w;
    else
      _ret = size;
  }
  return _ret;
}

uint32_t spsc_read(const void *restrict buf, void *restrict dest,
                   uint32_t *restrict pos_r, uint32_t size) noexcept {
  memcpy(dest, buf + *pos_r, size);
  __sync_fetch_and_add(pos_r, size); // (good for request-response throughput?)
  return size;
}
uint32_t spsc_write(void *restrict buf, const void *restrict src,
                    uint32_t *restrict pos_w, uint32_t size) noexcept {
  memcpy(buf + *pos_w, src, size);
  __sync_fetch_and_add(pos_w, size); // This include full memory barrier.
  return size;
}

int spsc_rewind_read(uint32_t pos_start, uint32_t *pos_r, uint32_t pos_w,
                     uint32_t pos_end) noexcept {
  if (unlikely(pos_end == *pos_r) // If true, rewind is needed.
      && pos_w != *pos_r) {
    __atomic_store_n(pos_r, pos_start, __ATOMIC_RELEASE);
    return 1;
  }
  return 0;
}
int spsc_rewind_write(uint32_t pos_start, uint32_t pos_r, uint32_t *pos_w,
                      uint32_t pos_end) noexcept {
  if (unlikely(pos_end == *pos_w) // If true, rewind is needed.
      && pos_r != pos_start) {
    __atomic_store_n(pos_w, pos_start, __ATOMIC_RELEASE);
    return 1;
  }
  return 0;
}

#ifndef __KERNEL__

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

  user_schedule(*usersched_tsc, USERSCHED_COND_SCHEDULE) {
    /* Compare the latest `pos_w` value with constant `pos_r` value. */
    _pos_w_save = __atomic_load_n(pos_w, __ATOMIC_ACQUIRE);
    spsc_rewind_read(0, pos_r, _pos_w_save, pos_end);
    if ((_rpeek = spsc_read_peek(*pos_r, _pos_w_save, pos_end, size)))
      user_cond_set(USERSCHED_COND_BREAK);
  }
  user_reschedule(usersched_tsc, pos_w, _pos_w_save);

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

  user_schedule(*usersched_tsc, USERSCHED_COND_SCHEDULE) {
    /* Compare the latest `pos_r` value with constant `pos_w` value. */
    _pos_r_save = __atomic_load_n(pos_r, __ATOMIC_ACQUIRE);
    spsc_rewind_write(0, _pos_r_save, pos_w, pos_end);
    if ((_wpeek = spsc_write_peek(_pos_r_save, *pos_w, pos_end, size)))
      user_cond_set(USERSCHED_COND_BREAK);
  }
  user_reschedule(usersched_tsc, pos_r, _pos_r_save);

  return _wpeek;
}

#endif
