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
	modprobe veikk

uninstall:
	modprobe -r $(MOD_NAME)
	rm $(shell modinfo -n veikk)
	depmod
