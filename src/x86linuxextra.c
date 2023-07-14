#include "x86linux/helper.h"

static __attribute__((constructor(101))) void _x86linuxextra_init(void) {
  log_info("libx86linuxextra 0.1.0 - Jihong Min (hurryman2212@gmail.com)");
#ifndef NDEBUG
  log_info("You are using `Debug` build.");
#else
  log_info("You are using `Release` build.");
#endif
}
