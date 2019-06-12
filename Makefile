MOD_NAME := veikk
BUILD_DIR := /lib/modules/$(shell uname -r)

obj-m := $(MOD_NAME).o

all:
	make -C $(BUILD_DIR)/build M=$(CURDIR) modules

clean:
	make -C $(BUILD_DIR)/build M=$(CURDIR) clean

install:
	make -C $(BUILD_DIR)/build M=$(CURDIR) modules_install
	depmod
	modprobe veikk

uninstall:
	modprobe -r $(MOD_NAME)
	rm $(BUILD_DIR)/extra/$(MOD_NAME).ko
	depmod
