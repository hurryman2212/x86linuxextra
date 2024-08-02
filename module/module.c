#include "x86linux/helper.h"

MODULE_LICENSE("Dual BSD/GPL");

static int x86linuxextra_init(void) { return 0; }
module_init(x86linuxextra_init);
static void x86linuxextra_exit(void) {}
module_exit(x86linuxextra_exit);

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
