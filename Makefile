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

install_signed:
	# see https://superuser.com/questions/1214116/no-openssl-sign-file-signing-key-pem-leads-to-error-while-loading-kernel-modules for info on signing the module
	# make sure openssl is installed
	printf "[ req ]\ndefault_bits = 4096\ndistinguished_name = req_distinguished_name\nprompt = no\nstring_mask = utf8only\nx509_extensions = myexts\n\n[ req_distinguished_name ]\nCN = Modules\n\n[ myexts ]\nbasicConstraints=critical,CA:FALSE\nkeyUsage=digitalSignature\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid" > x509.genkey
	openssl req -new -nodes -utf8 -sha512 -days 36500 -batch -x509 -config x509.genkey -outform DER -out $(BUILD_DIR)/certs/signing_key.x509 -keyout $(BUILD_DIR)/certs/signing_key.pem

	# regular make
	make install

	# cleanup
	shred -u x509.genkey $(BUILD_DIR)/certs/signing_key.pem $(BUILD_DIR)/certs/signing_key.x509

uninstall:
	modprobe -r $(MOD_NAME)
	rm $(BUILD_DIR)/../extra/$(MOD_NAME).ko*
	depmod
