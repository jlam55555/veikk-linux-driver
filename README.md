# An [official driver][official-driver] has been released by VEIKK.

This project should provide basic functionality for tablets but is no longer
actively maintained. The original author is too busy with school/work.

---

# VEIKK Linux Driver
A driver for [VEIKK][0]-brand digitizers/drawing tablets using the `usbhid` API.
Configuration options are exposed in sysfs and configurable with an associated
[GUI][10]. Connects low-level hardware events to the input subsystem (e.g.,
libinput), with some processing/mapping logic in between. This draws heavily 
from the [Wacom driver for Linux][1].

![image highlighting the v2 release][11]

---

### Features:
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
Make sure you have `make` and the appropriate linux headers installed 
(`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][4]
for more details.

    make
    sudo make all install clean
    
If you are getting a `Required key not available` error, please see
[this issue][6].

Check out the [issues tab][5] for known setup issues and solutions.

##### AUR (dkms) package
Thanks to [@artixnous][7], this driver is available as an AUR package with
`dkms`! See [input-veikk-dkms<sup>AUR</sup>][8]. Make sure to run
`modprobe veikk` after installation to load the driver.

##### Uninstallation
    sudo make uninstall
        
---

### Configuration
Currently, the configurable parameters are `screen_size`, `screen_map`,
`orientation`, and `pressure_map`, and are all available in sysfs under
`/sys/module/veikk/parameters` with permissions `0644`. Documentation on the
parameters is available in [`veikk_modparms.c`][9]. You can update a parameter
by simply writing the new value to it as root.

The visual configuration utility is available at
[@jlam55555/veikk-linux-driver-gui][10].

---

### Changelog:
- v2.0: Renamed from veikk-s640-driver, redesigned from the ground up to be more
    extensible.
- v1.1: Basic command-line configuration utility is created! Includes ability to
    change tablet orientation, mapping onto section of screen, and mapping raw 
    pressure input to pressure output.
- v1.0: Basic digitizer capabilities (e.g., pressure sensitivity) for S640 and
    similar tablets.
    
---

### Development status
Development was focused on the S640, A50, and VK1560, devices that are owned
by the author of this driver.

---

### Media
- [Original tweet][2]
- [Original blog post][3]
- [v3 update blog post][v3-update-blog-post]
    
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
[v3-update-blog-post]: http://everything-is-sheep.herokuapp.com/posts/veikk-linux-driver-v3-notes
[official-driver]: https://github.com/jlam55555/veikk-linux-driver/issues/71
