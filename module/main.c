#include "x86linux/helper.h"

MODULE_LICENSE("Dual BSD/GPL");

static int x86linuxextra_init(void) { return 0; }
static void x86linuxextra_exit(void) {}
module_init(x86linuxextra_init);
module_exit(x86linuxextra_exit);

EXPORT_SYMBOL(x86_test_bit);
EXPORT_SYMBOL(x86_set_bit_nonatomic);
EXPORT_SYMBOL(x86_unset_bit_nonatomic);
EXPORT_SYMBOL(x86_set_bit_atomic);
EXPORT_SYMBOL(x86_unset_bit_atomic);
EXPORT_SYMBOL(x86_search_lowest_bit);
EXPORT_SYMBOL(x86_search_lowest_common_bit);

EXPORT_SYMBOL(spsc_read_peek);
EXPORT_SYMBOL(spsc_write_peek);
EXPORT_SYMBOL(spsc_rewind_read);
EXPORT_SYMBOL(spsc_rewind_write);

EXPORT_SYMBOL(log_enabled);

MODULE_VERSION(__TARGET_VERSION__);
