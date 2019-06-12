# Veikk S640 Drawing Tablet driver for Linux

![Veikk S640 Tablet Driver "Hello World" with GIMP and Krita][5]

v1.0

A simple driver for the [Veikk S640 drawing tablet][0], using the `usbhid` HID API. This draws heavily off of the [Wacom driver][1], and is simplified to tailor to the S640's capabilities.

The driver interfaces (absolute) cursor movement, pressure sensitivity, and the two stylus buttons. Full 32768x32768 cursor position sensitivity and 8192-level pressure sensitivity are included.

I also [wrote a blog post][4] about the development of this driver, which also acts as a no-prior-knowledge-necessary tutorial for writing Linux drivers. Check it out!

---

### Setup instructions

Run the install instructions, and the driver should be set up! Woo hoo!

But for some older kernel versions (this appeared to be happening in kernel 4.15, but not in 4.18 or 5.1), `hid-generic` seems to want to gobble up the driver the moment it's plugged in, so need to unload it from `hid-generic`. Do this before installing the driver; if the driver is already installed, remove it and reinstall it after running the following command:

    sudo cat -n "xxxx:2FEB:0001.xxxx" > /sys/bus/hid/drivers/hid-generic/unload

replacing the `x`s with the corresponding values from `ls /sys/bus/hid/devices`. (`2FEB:0001` is the vendor ID, product ID of the S640 product.) If this is successful, running `ls /sys/bus/hid/drivers/veikk` should list the Veikk S640 device.

(The cause of this problem is not confirmed to be the kernel version, but I will do additional testing and see if `hid-generic` has had any changes in newer kernel versions.)

---

### Install instructions

Make sure you have `make` and the appropriate linux headers installed (`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][3] for more details.

    make
    sudo make install clean

---

### Uninstall instructions

    sudo make uninstall

---

### Future updates

- Visual configuration interface (e.g., pressure sensitivity mapping)
- Integration for more Veikk devices
- Signing the module to allow with Safe Boot enabled

---

### Known issues

- The module is unsigned -- you may have [to turn off Safe Boot][6] to allow this to work.
- There are known problems with `hid-generic` on kernel 4.15. If that is the case, try the extra instructions under "Setup Instructions" or try installing a kernel version 4.18+ (e.g., using [Ukuu][7]).

---

### Disclaimers

This was hastily tailored for the S640 *only* (the author of this package can only afford the cheapest tablet), but PRs for other [Veikk tablets][2] are very welcome.

This is also my first Linux driver. If there are optimizations or conventions that would be optimal to follow, please let me know or create a PR.

I am also not an artist. My sister is the artist, and this is her tablet. If any of the input mappings are incorrect, or if there are any useful input mappings to implement for some artistic reason I wouldn't know, please reach out or make a PR.

[0]: http://www.veikk.com/s640/
[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
[2]: http://www.veikk.com/pen-tablet/
[3]: https://askubuntu.com/questions/554624/how-to-resolve-the-lib-modules-3-13-0-27-generic-build-no-such-file-or-direct
[4]: https://everything-is-sheep.herokuapp.com/posts/on-developing-a-linux-driver
[5]: https://everything-is-sheep.herokuapp.com/res/img/headers/on-developing-a-linux-driver.jpg
[6]: https://support.displaylink.com/knowledgebase/articles/1181617-how-to-use-displaylink-ubuntu-driver-with-uefi-sec
[7]: https://vitux.com/update-linux-kernel-on-ubuntu-through-ukuu/
