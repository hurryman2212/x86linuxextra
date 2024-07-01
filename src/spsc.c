#include "x86linux/helper.h"

#ifndef __KERNEL__

#include <string.h>

#endif

uint32_t spsc_read_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                        uint32_t size) {
  uint32_t __ret;
  if (pos_r <= pos_w) {
    if ((pos_w - pos_r) < size)
      __ret = pos_w - pos_r;
    else
      __ret = size;
  } else {
    /* Write position is behind Read position. */
    if ((pos_end - pos_r) < size)
      /* Rewind might be needed. */
      __ret = pos_end - pos_r;
    else
      __ret = size;
  }
  return __ret;
}
uint32_t spsc_write_peek(uint32_t pos_r, uint32_t pos_w, uint32_t pos_end,
                         uint32_t size) {
  uint32_t __ret;
  if (pos_w < pos_r) {
    if ((pos_r - pos_w - 1) < size)
      __ret = pos_r - pos_w - 1; // Do not allow overlapping!
    else
      __ret = size;
  } else {
    /* Read position is behind or same as Write position. */
    if ((pos_end - pos_w) < size)
      /* Rewind might be needed. */
      __ret = pos_end - pos_w;
    else
      __ret = size;
  }
  return __ret;
}

uint32_t spsc_read(const void *restrict buf, void *restrict dest,
                   uint32_t *restrict pos_r, uint32_t size) {
  memcpy(dest, buf + *pos_r, size);
  __sync_fetch_and_add(pos_r, size); // (good for request-response throughput?)
  return size;
}
uint32_t spsc_write(void *restrict buf, const void *restrict src,
                    uint32_t *restrict pos_w, uint32_t size) {
  memcpy(buf + *pos_w, src, size);
  __sync_fetch_and_add(pos_w, size); // This include full memory barrier.
  return size;
}

int spsc_rewind_read(uint32_t pos_start, uint32_t *restrict pos_r,
                     uint32_t pos_w, uint32_t pos_end) {
  if (unlikely(pos_end == *pos_r) // If true, rewind is needed.
      && pos_w != *pos_r) {
    *pos_r = pos_start;
    return 1;
  }
  return 0;
}
int spsc_rewind_write(uint32_t pos_start, uint32_t pos_r,
                      uint32_t *restrict pos_w, uint32_t pos_end) {
  if (unlikely(pos_end == *pos_w) // If true, rewind is needed.
      && pos_r != pos_start) {
    *pos_w = pos_start;
    return 1;
  }
  return 0;
}
