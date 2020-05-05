MOD_NAME := veikk
BUILD_DIR := /lib/modules/$(shell uname -r)/build

obj-m := $(MOD_NAME).o
$(MOD_NAME)-objs := veikk_drv.o veikk_vdev.o veikk_modparms.o

all:
	make -C $(BUILD_DIR) M=$(CURDIR) modules

clean:
	make -C $(BUILD_DIR) M=$(CURDIR) clean

install:
	make -C $(BUILD_DIR) M=$(CURDIR) modules_install
	mkdir -p /etc/modules-load.d
	modprobe veikk
	echo "veikk" > /etc/modules-load.d/veikk.conf

uninstall:
	modprobe -r $(MOD_NAME)
	rm -f $(shell modinfo -n veikk)
	rm -f /etc/modprobe.d/$(MOD_NAME).conf /etc/modules-load.d/$(MOD_NAME).conf
	depmod
