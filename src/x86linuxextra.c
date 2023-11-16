#include "x86linux/helper.h"

static __attribute__((constructor(101))) void _x86linuxextra_init(void) {
  /* Suppress logging first if required. */

  const char *env_log_suppress = getenv("LOG_SUPPRESS");
  if (env_log_suppress)
    /* Do not log here! */
    log_suppress = 1;

#ifndef NDEBUG
  fprintf(
      stderr,
      "libx86linuxextra 0.1.0 (Debug) - Jihong Min (hurryman2212@gmail.com)\n");
#else
  fprintf(stderr, "libx86linuxextra 0.1.0 (Release) - Jihong Min "
                  "(hurryman2212@gmail.com)\n");
#endif

  usersched_init();
}
