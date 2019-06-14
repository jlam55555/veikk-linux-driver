# Veikk S640 Drawing Tablet driver for Linux

![Veikk S640 Tablet Driver "Hello World" with GIMP and Krita][5]

v1.0

**Note**: Pen capabilities may also work on the Veikk A30 and A50 as it is identical to the S640, but the author of this repo does not own these devices to verify. Additional capabilities for other devices  (e.g., tablet buttons) have yet to be added.

A simple driver for the [Veikk S640 drawing tablet][0], using the `usbhid` HID API. This draws heavily off of the [Wacom driver][1], and is simplified to tailor to the S640's capabilities.

The driver interfaces (absolute) cursor movement, pressure sensitivity, and the two stylus buttons. Full 32768x32768 cursor position sensitivity and 8192-level pressure sensitivity are included.

I also [wrote a blog post][4] about the development of this driver, which also acts as a no-prior-knowledge-necessary tutorial for writing Linux drivers. Check it out!

---

### Setup instructions

**Arch users**: [input-veikk-dkms<sup>AUR</sup>][10], thanks to [@artixnous][11]

Run the install instructions, and the driver should be set up! Woo hoo!

Check out the [issues tab][9] for known setup issues and solutions.

---

### Install instructions

Make sure you have `make` and the appropriate linux headers installed (`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][3] for more details.

    make
    sudo make install clean

If you are getting a `Required key not available` error, please see [this issue][7].

---

### Uninstall instructions

    sudo make uninstall

---

### Future updates

- Visual configuration interface (e.g., pressure sensitivity mapping)
- Integration for more Veikk devices

---

### Disclaimers

This was hastily tailored for the S640 *only* (the author of this package can only afford the cheapest tablet), but PRs for other [Veikk tablets][2] are very welcome. [It's reported that pen capabilities work for the A50 as well][9].

This is also my first Linux driver. If there are optimizations or conventions that would be optimal to follow, please let me know or create a PR.

I am also not an artist. My sister is the artist, and this is her tablet. If any of the input mappings are incorrect, or if there are any useful input mappings to implement for some artistic reason I wouldn't know, please reach out or make a PR.

[0]: http://www.veikk.com/s640/
[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
[2]: http://www.veikk.com/pen-tablet/
[3]: https://askubuntu.com/questions/554624/how-to-resolve-the-lib-modules-3-13-0-27-generic-build-no-such-file-or-direct
[4]: http://eis.jonlamdev.com/posts/on-developing-a-linux-driver
[5]: http://eis.jonlamdev.com/res/img/headers/on-developing-a-linux-driver.jpg
[6]: https://support.displaylink.com/knowledgebase/articles/1181617-how-to-use-displaylink-ubuntu-driver-with-uefi-sec
[7]: https://github.com/jlam55555/veikk-s640-driver/issues/3
[8]: https://github.com/jlam55555/veikk-s640-driver/issues
[9]: https://github.com/jlam55555/veikk-s640-driver/pull/1
[10]: https://aur.archlinux.org/packages/input-veikk-dkms/
[11]: https://github.com/artixnous
