KERNEL_SRC := /usr/src/linux-headers-`uname -r`
BUILD_DIR = `pwd`

MODULES := rtled.o rtswitch.o

obj-m := $(MODULES)

all: $(MODULES:.o=.ko)

rtled.ko: rtled.c
	make -C $(KERNEL_SRC) M=$(BUILD_DIR) V=1 modules

rtswitch.ko: rtswitch.c
	make -C $(KERNEL_SRC) M=$(BUILD_DIR) V=1 modules

clean:
	make -C $(KERNEL_SRC) M=$(BUILD_DIR) V=1 clean

install: rtled.ko rtswitch.ko uninstall
	insmod rtled.ko
	insmod rtswitch.ko
	[ -e /dev/rtswitch2 ] || sleep 1
	chmod 666 /dev/rt*

uninstall:
	[ ! -e /dev/rtled0 ] || rmmod rtled.ko
	[ ! -e /dev/rtswitch0 ] || rmmod rtswitch.ko

