# Veikk S640 Drawing Tablet driver for Linux

Very early version, do not use

Using the [Wacom driver][1] as guidance

To setup, make the scripts executable

    chmod +x build.sh remove.sh

---

### Compile instructions

    ./build.sh

---

### Install instructions

This compiles the driver and runs `insmod` on it.

    ./install.sh

---

### Remove instructions

    ./remove.sh

[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
