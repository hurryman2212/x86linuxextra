#include "x86linux/helper.h"

static __attribute__((constructor(101))) void _x86linuxextra_init(void) {
  log_info("libx86linuxextra 0.1.0 - Jihong Min (hurryman2212@gmail.com)");
#ifndef NDEBUG
  log_info("You are using `Debug` build.");
#else
  log_info("You are using `Release` build.");
#endif

  const char *env_suppress_log = getenv("X86LINUX_SUPPRESS_LOG");
  if (env_suppress_log) {
    log_warn("X86LINUX_SUPPRESS_LOG=%s -> All logging will be suppressed!",
             env_suppress_log);
    _suppress_log = 1;
  }
}
