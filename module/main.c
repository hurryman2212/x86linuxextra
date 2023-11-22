#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

static int x86linuxextra_init(void) { return 0; }

static void x86linuxextra_exit(void) {}

module_init(x86linuxextra_init);
module_exit(x86linuxextra_exit);
