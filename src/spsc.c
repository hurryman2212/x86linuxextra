#include "x86linux/helper.h"

size_t spsc_read_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                      size_t req_size) {
  size_t ret;
  if (pos_r <= pos_w) {
    if ((pos_w - pos_r) < req_size)
      ret = pos_w - pos_r;
    else
      ret = req_size;
  } else {
    /* Write position is behind Read position. */
    if ((pos_end - pos_r) < req_size)
      /* Rewind might be needed. */
      ret = pos_end - pos_r;
    else
      ret = req_size;
  }
  return ret;
}
size_t spsc_write_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                       size_t req_size) {
  size_t ret;
  if (pos_w < pos_r) {
    if ((pos_r - pos_w - 1) < req_size)
      ret = pos_r - pos_w - 1; // Do not allow overlapping!
    else
      ret = req_size;
  } else {
    /* Read position is behind or same as Write position. */
    if ((pos_end - pos_w) < req_size)
      /* Rewind might be needed. */
      ret = pos_end - pos_w;
    else
      ret = req_size;
  }
  return ret;
}

int spsc_rewind_read(size_t pos_start, size_t *__restrict pos_r, size_t pos_w,
                     size_t pos_end) {
  if (unlikely(pos_end == *pos_r) // If true, rewind is needed.
      && pos_w != *pos_r) {
    *pos_r = pos_start;
    return 1;
  }
  return 0;
}
int spsc_rewind_write(size_t pos_start, size_t pos_r, size_t *__restrict pos_w,
                      size_t pos_end) {
  if (unlikely(pos_end == *pos_w) // If true, rewind is needed.
      && pos_r != pos_start) {
    *pos_w = pos_start;
    return 1;
  }
  return 0;
}
