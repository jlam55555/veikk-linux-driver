ifneq ($(KERNELRELEASE),)
	obj-m := veikk.o
	veikk-objs := hid-veikk.o

else
	KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install
	depmod
	modprobe veikk
	mkdir -p /etc/modules-load.d
	echo "veikk" > /etc/modules-load.d/veikk.conf

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

uninstall:
	modprobe -r veikk

test: uninstall all install clean
	sudo modprobe veikk
	
endif
