# VEIKK Linux Driver
A driver for [VEIKK][0]-brand digitizers/drawing tablets using the `usbhid` API.
Configuration options are exposed in sysfs. This draws heavily from the [Wacom
driver for Linux][1].

---

### Features:
Currently, a small set of basic digitizer features are supported, such as:
- Full range and resolution for tablet pressure and spatial sensitivity
- Configurable screen mapping and orientation

More features are planned for the near future, such as:
- Button remapping
- Support for gesture pads and additional buttons (model-dependent)
- Device/model-specific configuration options
- A UI to configure options and enable options on boot (persistent options)

---

### Installation
Make sure you have `make` and the appropriate linux headers installed 
(`linux-headers-$(uname -r)` on Ubuntu, `linux-headers` on Arch). See [this][4]
for more details.

    make
    sudo make all install clean
    
If you are getting a `Required key not available` error, please see [this issue]
[6].

Check out the [issues tab][5] for known setup issues and solutions.

##### Uninstallation
    sudo make uninstall
    
##### v1.0 installation
Thanks to [@artixnous][7], v1.0 of this driver is available as an AUR package!
See [input-veikk-dkms<sup>AUR</sup>][8].
    
---

### Configuration
Currently, the configurable parameters are `screen_size`, `screen_map`, and
`orientation`, and are all available in sysfs under
`/sys/module/veikk/parameters` with permissions `0644`. Documentation on the
parameters is available in [`veikk_modparms.c`][9]. You can update a parameter
by simply writing the new value to it as root.

##### Example
Assume that you have a dual-monitor setup, with two 1920x1080p monitors sitting
side-by-side, each in landscape format. Then the entire screen size is
3840x1080p, and thus `screen_size` should be set to:

    screen_size=(3840<<16)|1080=251659320
    
Assume you wanted your tablet only to map to the right monitor, i.e., the
rectangle starting at (1920,0) with width 1920 and height 1080. Thus
`screen_map` should be set to:

    screen_map=(1920<<48)|(0<<32)|(1920<<16)|1080=540431955410289720
    
Assume finally that you wish to use your tablet in an orientation rotated 90deg
CCW from the default orientation. Thus, `orientation` should be set to:

    orientation=1
    
Altogether, the terminal commands to do this might look like:

    cd /sys/module/veikk/parameters
    su
    echo 251659320 >screen_size
    echo 540431955410289720 >screen_map
    echo 1 >orientation

Note that due to the workings of the `struct input_dev` interface, only the
ratios between `screen_map` to `screen_size` matters. Thus, using
`screen_size=(2<<16)|1` and `screen_map=(1<<48)|(1<<16)|1` should produce the
same effect. This may be useful if you know the ratio of mapped screen area to
screen size is a clean fraction. Which method is used is up to the user's
discretion.

---

### Changelog:
- v2.0: Renamed from veikk-s640-driver, redesigned from the ground up to be more
    extensible.
- v1.0: Basic digitizer capabilities (e.g., pressure sensitivity) for S640 and
    similar tablets.
    
---

### Development status
Currently, development is focused on the S640, but PR's are welcome. The
developer only has an S640 at the moment and is unable to directly develop new
features for other models, but open to feedback and questions at jonlamdev
at gmail dot com.

---

### Media
- [Original tweet][2]
- [Original blog post][3]
- (Future blog post with updates soon.)
    
[0]: https://www.veikk.com/
[1]: https://github.com/torvalds/linux/blob/master/drivers/hid/wacom_wac.c
[2]: https://twitter.com/jlam55555/status/1138285016209854464?s=20
[3]: http://eis.jonlamdev.com/posts/on-developing-a-linux-driver
[4]: https://askubuntu.com/questions/554624/how-to-resolve-the-lib-modules-3-13-0-27-generic-build-no-such-file-or-direct
[5]: https://github.com/jlam55555/veikk-s640-driver/issues
[6]: https://github.com/jlam55555/veikk-linux-driver/issues/3
[7]: https://github.com/artixnous
[8]: https://aur.archlinux.org/packages/input-veikk-dkms/
[9]: ./veikk_modparms.c