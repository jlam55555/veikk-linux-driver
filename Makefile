MOD_NAME := veikk
BUILD_DIR := /lib/modules/$(shell uname -r)/build

obj-m := $(MOD_NAME).o

all:
	make -C $(BUILD_DIR) M=$(CURDIR) modules

clean:
	make -C $(BUILD_DIR) M=$(CURDIR) clean

install:
	make -C $(BUILD_DIR) M=$(CURDIR) modules_install
	depmod
	modprobe veikk

uninstall:
	modprobe -r $(MOD_NAME)
	rm /lib/modules/$(shell uname -r)/extra/$(MOD_NAME).ko*
	# problems with this line for some reason for some users:
	# rm $(BUILD_DIR)/../extra/$(MOD_NAME).ko*
	depmod
