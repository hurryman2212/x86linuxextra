ccflags-y += -I/usr/local/include -I/usr/include -Wall -Werror -D_GNU_SOURCE -save-temps -fverbose-asm -DNDEBUG -march=native -O2 -Wl,-s -std=gnu11

# Add path(s) of C source or static library file(s) here!
MY-AR += ../src/bitwise.c ../src/spsc.c

# DO NOT EDIT THE BELOW!

ifeq ($(M),)
M := $(shell pwd)
endif

TARGET := $(shell grep '^PACKAGE_NAME=' $(M)/dkms.conf | cut -d'=' -f2 | tr -d '"')
TARGET_VERSION := $(shell grep '^PACKAGE_VERSION=' $(M)/dkms.conf | cut -d'=' -f2 | tr -d '"')

obj-m += $(TARGET).o
MY-SRCS += $(filter-out $(wildcard $(M)/*.mod.c), $(wildcard $(M)/*.c))
MY-OBJS += $(MY-SRCS:.c=.o)

$(TARGET)-objs += $(notdir $(MY-OBJS)) $(filter-out $(notdir $(wildcard $(M)/*.c)), $(notdir $(MY-AR)))
KDIR ?= /lib/modules/$(shell uname -r)

all:
	cp $(MY-AR) $(M)/ 2>/dev/null || true
	make -C $(KDIR)/build M=$(shell pwd) modules
	ar -rcs $(TARGET).a $(MY-OBJS)
	cp /var/lib/dkms/$(TARGET)/$(TARGET_VERSION)/build/$(TARGET).a /usr/src/$(TARGET)-$(TARGET_VERSION)/ 2>/dev/null || true
	rm -f$(shell printf " $(M)/%s" $(notdir $(MY-AR)))

install:
	cp $(MY-AR) $(M)/ 2>/dev/null || true
	dkms install "$(M)" --force
	rm -f$(shell printf " $(M)/%s" $(notdir $(MY-AR)))

	cp *.rules /etc/udev/rules.d/ 2>/dev/null || true
uninstall:
	dkms remove $(TARGET)/$(shell cat dkms.conf | grep PACKAGE_VERSION | sed 's/^[^\.]\+\=//' | sed -e 's/["]//g') --all || true
	rm -rf /usr/src/$(TARGET)-$(TARGET_VERSION)

	rm -f$(shell printf " /etc/udev/rules.d/%s" $(notdir $(shell ls $(M)/*.rules 2>/dev/null))) 2>/dev/null || true

load: all
	insmod $(TARGET).ko
unload:
	rmmod $(TARGET)

clean:
	make -C $(KDIR)/build M=$(shell pwd) clean
	rm -f$(shell printf " $(M)/%s" $(notdir $(MY-AR)))
