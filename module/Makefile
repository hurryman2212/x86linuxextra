# Add compiler flag(s).
ccflags-y += -frecord-gcc-switches # Save all compile-time information
ccflags-y += -Wall -Werror -Wno-missing-braces -std=gnu11 -D_GNU_SOURCE # Forcing standard
ccflags-y += -O2 -DNDEBUG -march=native -ftree-vectorize # Optimization
ccflags-y += -I$(M)/include -I/usr/include -I/usr/local/include
ccflags-y +=
ldflags-y +=

# Add path(s) of external C source or static library file(s) to temporarily copy to the local build directory.
EXTERN-HEADERS := ../include
EXTERN-OBJS :=
EXTERN-SRCS := $(shell printf " ../src/%s" bitset.c log.c spsc.c)

KBUILD_EXTRA_SYMBOLS +=

# DO NOT EDIT THE BELOW!

SHELL := /bin/bash

M ?= $(shell pwd)
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

PACKAGE_NAME := $(shell grep '^PACKAGE_NAME=' $(M)/dkms.conf | cut -d'=' -f2 | tr -d '"')
X86LINUXEXTRA_VERSION ?= $(shell if [ -r "$(M)/../generate-version.sh" ]; then sh "$(M)/../generate-version.sh"; else printf '%s\n' unknown; fi)
PACKAGE_VERSION := $(X86LINUXEXTRA_VERSION)
DKMS_SRC_DIR := /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
ccflags-y += -DX86LINUXEXTRA_VERSION=\"$(PACKAGE_VERSION)\"

EXTERN := $(EXTERN-HEADERS) $(EXTERN-OBJS) $(EXTERN-SRCS)

# These will be passed to the Makefile of the kernel source.
MY-SRCS += $(filter-out $(wildcard $(M)/*.mod.c), $(wildcard $(M)/*.c))
MY-OBJS += $(MY-SRCS:.c=.o)
obj-m += $(PACKAGE_NAME).o
$(PACKAGE_NAME)-objs += $(notdir $(MY-OBJS)) $(notdir $(EXTERN-OBJS))

.PHONY: all build copy_extern remove_extern install uninstall load unload clean distclean

all: build

copy_extern:
ifndef source_tree
	extern="$(EXTERN)"; \
	if [ -n "$$extern" ]; then \
		cp -rp $$extern $(M)/; \
	fi
else
	rsync -av $(source_tree)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/ $(M)/ --exclude=Makefile >/dev/null
endif
remove_extern:
ifndef source_tree
	rm -rf $(notdir $(EXTERN-OBJS))
endif

LLVM_OPT := $(shell grep -q '^#define CONFIG_CC_IS_GCC 1' $(KDIR)/include/generated/autoconf.h 2>/dev/null || echo LLVM=1)

build: copy_extern
	@set -e; \
    exit_code=0; \
    $(MAKE) -C $(KDIR) M=$(M) $(LLVM_OPT) X86LINUXEXTRA_VERSION="$(PACKAGE_VERSION)" modules || exit_code=$$?; \
    $(MAKE) remove_extern; \
    if [ $$exit_code -ne 0 ]; then exit $$exit_code; fi; \
    ar -rcs $(PACKAGE_NAME).a $(MY-OBJS)

install: copy_extern
	@set -e; \
	trap '$(MAKE) remove_extern' EXIT; \
	if dkms status -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)" 2>/dev/null | grep -Fq '$(PACKAGE_NAME)/$(PACKAGE_VERSION)'; then \
		dkms remove -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)" --all; \
	fi; \
	rm -rf "$(DKMS_SRC_DIR)"; \
	install -d "$(DKMS_SRC_DIR)"; \
	for file in "$(M)/Makefile" "$(M)"/*.c "$(M)"/*.h "$(M)"/*.sh "$(M)"/*.rules $(addprefix $(M)/,$(notdir $(EXTERN))); do \
		[ -e "$$file" ] || continue; \
		case "$$file" in *.mod.c) continue ;; esac; \
		cp -a "$$file" "$(DKMS_SRC_DIR)/"; \
	done; \
	sed 's/^PACKAGE_VERSION=.*/PACKAGE_VERSION="$(PACKAGE_VERSION)"/' "$(M)/dkms.conf" > "$(DKMS_SRC_DIR)/dkms.conf"; \
	dkms add "$(DKMS_SRC_DIR)"; \
	dkms build -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)"; \
	dkms install -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)"; \
	for rules_file in "$(M)"/*.rules; do \
		[ -f "$$rules_file" ] || continue; \
		cp "$$rules_file" /etc/udev/rules.d/; \
	done

uninstall:
	@if dkms status -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)" 2>/dev/null | grep -Fq '$(PACKAGE_NAME)/$(PACKAGE_VERSION)'; then \
		dkms remove -m "$(PACKAGE_NAME)" -v "$(PACKAGE_VERSION)" --all; \
	else \
		echo '$(PACKAGE_NAME)/$(PACKAGE_VERSION) is not registered with DKMS'; \
	fi
	rm -rf "$(DKMS_SRC_DIR)"
	@for rules_file in "$(M)"/*.rules; do \
		[ -f "$$rules_file" ] || continue; \
		rm -f "/etc/udev/rules.d/$${rules_file##*/}"; \
	done

load: all
	insmod $(PACKAGE_NAME).ko
unload:
	rmmod $(PACKAGE_NAME)

clean: TEMP_COMPILE_CMDS := $(shell mktemp)
clean:
	cp compile_commands.json $(TEMP_COMPILE_CMDS) 2>/dev/null || :
	$(MAKE) -C $(KDIR) M=$(M) clean
	rm -rf $(notdir $(EXTERN-OBJS))
	cp $(TEMP_COMPILE_CMDS) compile_commands.json
	rm -f $(TEMP_COMPILE_CMDS)
distclean:
	git clean -xddf
