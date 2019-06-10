# Veikk S640 Drawing Tablet driver for Linux

Very early version, currently only for testing

Using the [Wacom driver][1] (heavily) as guidance.

---

### Setup instructions

Right now, `hid-generic` seems to want to gobble up the driver the moment it's plugged in, so need to unload it from `hid-generic`. Do this before installing the driver; if the driver is already installed, remove it and reinstall it after running the following command:

    sudo cat -n "xxxx:2FEB:0001.xxxx" > /sys/bus/hid/drivers/hid-generic/unload

replacing the `x`s with the corresponding values from `ls /sys/bus/hid/devices`. (`2FEB:0001` is the vendor ID, product ID of the S640 product.) If this is successful, running `ls /sys/bus/hid/drivers/veikk` should list the Veikk S640 device.

---

### Install instructions

    make
    sudo make install

---

### Remove instructions

Script to remove driver coming soon


[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
