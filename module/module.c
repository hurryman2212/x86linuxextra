#include "x86linux/helper.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION(X86LINUXEXTRA_VERSION);

static const char _version[] = X86LINUXEXTRA_VERSION;

static int _init(void) {
  log_msg(LOG_DEBUG,
          "libx86linuxextra (version %s) by Jihong Min "
          "(hurryman2212@gmail.com)",
          _version);

  return 0;
}
module_init(_init);
static void _exit(void) {}
module_exit(_exit);

/* Bitset operations */

EXPORT_SYMBOL(bitset_test);
EXPORT_SYMBOL(bitset_set);
EXPORT_SYMBOL(bitset_unset);
EXPORT_SYMBOL(bitset_search_lowest);
EXPORT_SYMBOL(bitset_search_lowest_common);

/* Logger */

EXPORT_SYMBOL(_log_lvl);

/* SPSC (free size) */

EXPORT_SYMBOL(spsc_read_peek);
EXPORT_SYMBOL(spsc_write_peek);
EXPORT_SYMBOL(spsc_read);
EXPORT_SYMBOL(spsc_write);
EXPORT_SYMBOL(spsc_rewind_read);
EXPORT_SYMBOL(spsc_rewind_write);
