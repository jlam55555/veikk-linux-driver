
# VEIKK Linux Driver (v3-alpha)
A driver for [VEIKK][0]-brand digitizers/drawing tablets, along with associated userspace configuration tools for X server users. Early versions draw heavily from the [Wacom driver for Linux][1].

*[need a v3 release image, coming soon]*

---

### Features:

*[this section has to be updated for v3 -- see the changelog]*

Currently, a set of basic basic digitizer features are supported, such as:
- Full range and resolution for tablet pressure and spatial sensitivity
- Configurable screen mapping, orientation, and (cubic) pressure mapping
- Driver (using `/etc/modules-load.d/`) and options (using `/etc/modprobe.d`)
  persist after reboots

More features are planned for the near future, such as:
- Button remapping
- Support for gesture pads and additional buttons (model-dependent)
- Device/model-specific configuration options

---

### Installation
Make sure you have `make` and the appropriate linux headers installed (`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][4] for more details.

```bash
make
sudo make all install clean
```

If you are getting a `Required key not available` error, please see [this issue][6].

Check out the [issues tab][5] for known setup issues and solutions.

##### AUR (dkms) package
Thanks to [@artixnous][7], this driver is available as an AUR package with `dkms`! See [input-veikk-dkms<sup>AUR</sup>][8]. Make sure to run `modprobe veikk` after installation to load the driver.

##### Uninstallation
```bash
sudo make uninstall
```

---

### Configuration
*[configuration in v3 will be completely different, using common X server utilities -- see the changelog]*

<strike>Currently, the configurable parameters are `screen_size`, `screen_map`, `orientation`, and `pressure_map`, and are all available in sysfs under `/sys/module/veikk/parameters` with permissions `0644`. Documentation on the parameters is available in [`veikk_modparms.c`][9]. You can update a parameter by simply writing the new value to it as root.</strike>

<strike>The **(new!)** visual configuration utility is available at [@jlam55555/veikk-linux-driver-gui][10].</strike>

---

### Changelog:
- v3-alpha: Improved stability on older systems, button support, improved user-space configuration tools
	- Provides more consistent and intuitive view of device to X server, showing a Pen device and Keyboard device (when applicable) that emit their respective events. (Now ignores third, proprietary interface.)
	- Buttons work! (Working on this.)
	- Mapping all happens through the use of `xinput` or `xbindkey`, configuration utilities provided by the X server. This achieves a number of goals:
		- Simplifies the driver.
		- Separation-of-concerns between kernel-space driver "mechanism" and user-space X server "policy" (see [*Linux Device Drivers, 3rd ed.*, p. 2: "Role of the Device Driver"][role-device-drivers]).
	- Style change: attempting to adhere more closely to the [Linux kernel coding style guide][code-style].
- v2.0: Renamed from veikk-s640-driver, redesigned from the ground up to be more extensible.
- v1.1: Basic command-line configuration utility is created! Includes ability to change tablet orientation, mapping onto section of screen, and mapping raw pressure input to pressure output.
- v1.0: Basic digitizer capabilities (e.g., pressure sensitivity) for S640 and similar tablets.
 
---

### Support / Compatibility
**ACTIVELY LOOKING FOR TESTERS FOR V3! Email the address listed under feedback if you'd like to help out. Specifically looking for those with an A15, A15 Pro, A30, and VK2200, but any others are welcome.**

*[this section may rapidly change as testing occurs]*

- v3-alpha:
	- Tablets: S640, A50, VK1560
	- Operating Systems: Ubuntu 14.04 (kernel 4.4), Arch Linux (kernel 5.6)
- v2.0: Basic pen functionality is available for the S640, A30, A50, A15, A15 Pro, and VK1560 models. This is compatible with v2.0 of the configuration GUI. Kernel versions 4.18+ on Ubuntu (16.04+), Fedora, and Arch have been tested.

---

### Media
- [Original tweet][2]
- [Original blog post][3]

---

### Feedback
jonlamdev at gmail dot com
    
[0]: https://www.veikk.com/
[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
[2]: https://twitter.com/jlam55555/status/1138285016209854464?s=20
[3]: https://everything-is-sheep.herokuapp.com/posts/on-developing-a-linux-driver
[4]: https://askubuntu.com/questions/554624/how-to-resolve-the-lib-modules-3-13-0-27-generic-build-no-such-file-or-direct
[5]: https://github.com/jlam55555/veikk-s640-driver/issues
[6]: https://github.com/jlam55555/veikk-linux-driver/issues/3
[7]: https://github.com/artixnous
[8]: https://aur.archlinux.org/packages/input-veikk-dkms/
[9]: ./veikk_modparms.c
[10]: https://github.com/jlam55555/veikk-linux-driver-gui
[11]: https://i.imgur.com/Mug8gRn.jpg
[code-style]: https://www.kernel.org/doc/html/v5.7/process/coding-style.html
[role-device-drivers]: https://static.lwn.net/images/pdf/LDD3/ch01.pdf

