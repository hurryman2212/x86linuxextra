# Add compiler flag(s).
ccflags-y += -frecord-gcc-switches # Save all compile-time information
ccflags-y += -Wall -Werror -Wno-missing-braces -std=gnu11 -D_GNU_SOURCE # Forcing standard
ccflags-y += -O2 -DNDEBUG -march=native -ftree-vectorize # Optimization
ccflags-y += -I/usr/include -I/usr/local/include
ccflags-y +=
ldflags-y +=

# Add path(s) of external C source or static library file(s) to temporarily copy to the local build directory.
EXTERN-HEADER := $(shell printf " ../include/%s" x86linux)
EXTERN-OBJECT :=
EXTERN-SOURCE := $(shell printf " ../src/%s" bitset.c log.c spsc.c)

KBUILD_EXTRA_SYMBOLS +=

# DO NOT EDIT THE BELOW!

SHELL := /bin/bash

M ?= $(shell pwd)
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

PACKAGE_NAME := $(shell grep '^PACKAGE_NAME=' $(M)/dkms.conf | cut -d'=' -f2 | tr -d '"')
PACKAGE_VERSION := $(shell grep '^PACKAGE_VERSION=' $(M)/dkms.conf | cut -d'=' -f2 | tr -d '"')

EXTERN := $(EXTERN-HEADER) $(EXTERN-OBJECT) $(EXTERN-SOURCE)

# These will be passed to the Makefile of the kernel source.
MY-SRCS += $(filter-out $(wildcard $(M)/*.mod.c), $(wildcard $(M)/*.c))
MY-OBJS += $(MY-SRCS:.c=.o)
obj-m += $(PACKAGE_NAME).o
$(PACKAGE_NAME)-objs += $(notdir $(MY-OBJS)) $(notdir $(EXTERN-OBJECT))

all: build

copy_extern:
ifndef source_tree
	extern="$(EXTERN)"; \
	if [ -n "$$extern" ]; then \
		cp -r $$extern $(M)/; \
	fi
else
	rsync -av $(source_tree)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/ $(M)/ --exclude=Makefile >/dev/null
endif
remove_extern:
ifndef source_tree
	rm -rf $(notdir $(EXTERN))
endif

LLVM_OPT := $(shell grep -q '^#define CONFIG_CC_IS_GCC 1' $(KDIR)/include/generated/autoconf.h 2>/dev/null || echo LLVM=1)

build: copy_extern
	@set -e; \
    exit_code=0; \
    $(MAKE) -C $(KDIR) M=$(M) $(LLVM_OPT) modules || exit_code=$$?; \
    $(MAKE) remove_extern; \
    if [ $$exit_code -ne 0 ]; then exit $$exit_code; fi; \
    ar -rcs $(PACKAGE_NAME).a $(MY-OBJS)

install: copy_extern
	@set -e; \
    exit_code=0; \
	dkms install "$(M)" --force || exit_code=$$?; \
	$(MAKE) remove_extern; \
	if [ $$exit_code -eq 0 ]; then \
		ls *.rules 2>/dev/null | while IFS= read -r rules_file; do \
			cp "$$rules_file" /etc/udev/rules.d/; \
		done; \
	fi; \
	if [ $$exit_code -ne 0 ]; then exit $$exit_code; fi

uninstall:
	-dkms remove $(PACKAGE_NAME)/$(PACKAGE_VERSION) --all --force
	rm -rf /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
	ls *.rules 2>/dev/null | while IFS= read -r rules_file; do \
		rm -f "/etc/udev/rules.d/$$rules_file"; \
	done

load: all
	insmod $(PACKAGE_NAME).ko
unload:
	rmmod $(PACKAGE_NAME)

clean: TEMP_COMPILE_COMMANDS := $(shell mktemp)
clean:
	cp compile_commands.json $(TEMP_COMPILE_COMMANDS) 2>/dev/null || :
	$(MAKE) -C $(KDIR) M=$(M) clean
	rm -rf $(notdir $(EXTERN))
	cp $(TEMP_COMPILE_COMMANDS) compile_commands.json
	rm -f $(TEMP_COMPILE_COMMANDS)
distclean:
	git clean -xdf
