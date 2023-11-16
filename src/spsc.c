#include <sys/types.h>

size_t spsc_read_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                      size_t req_size) {
  return (pos_r <= pos_w)
             ? (((pos_w - pos_r) < req_size) ? (pos_w - pos_r) : req_size)
         : ((pos_end - pos_r) < req_size) ? (pos_end - pos_r)
                                          : req_size;
}
size_t spsc_write_peek(size_t pos_r, size_t pos_w, size_t pos_end,
                       size_t req_size) {
  return (pos_w < pos_r)
             ? (((pos_r - pos_w) < req_size) ? (pos_r - pos_w) : req_size)
         : ((pos_end - pos_w) < req_size) ? (pos_end - pos_w)
                                          : req_size;
}
