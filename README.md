# Veikk S640 Drawing Tablet driver for Linux

Very early version, currently only for testing

Using the [Wacom driver][1] (heavily) as guidance.

---

### Setup instructions

Make the scripts executable.

    chmod +x build.sh remove.sh install.sh

Right now, `hid-generic` seems to want to gobble up the driver the moment it's plugged in, so need to unload it from `hid-generic`. Do this before installing the driver; if the driver is already installed, remove it and reinstall it after running the following command:

    sudo cat -n "xxxx:2FEB:0001.xxxx" > /sys/bus/hid/drivers/hid-generic/unload

replacing the `x`s with the corresponding values from `ls /sys/bus/hid/devices`. (`2FEB:0001` is the vendor ID, product ID of the S640 product.)

---

### Compile instructions

Compile using the `Makefile`.

    ./build.sh

---

### Install instructions

This compiles the driver and runs `insmod` on it.

    ./install.sh

---

### Debug instructions

Watch the output of the driver in `dmesg`.

  dmesg -w

---

### Remove instructions

Runs `rmmod` on the driver.

    ./remove.sh

[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
