#include "x86linux/helper.h"

#include "usersched.h"

static __attribute__((constructor(101))) void _x86linuxextra_init(void) {
  /* Suppress logging first if required. */

  const char *env_suppress_log = getenv("X86LINUX_SUPPRESS_LOG");
  if (env_suppress_log)
    /* Do not log here! */
    _suppress_log = 1;

#ifndef NDEBUG
  fprintf(
      stderr,
      "libx86linuxextra 0.1.0 (Debug) - Jihong Min (hurryman2212@gmail.com)\n");
#else
  fprintf(stderr, "libx86linuxextra 0.1.0 (Release) - Jihong Min "
                  "(hurryman2212@gmail.com)\n");
#endif

  _usersched_init();
}
