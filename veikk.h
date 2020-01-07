/*
 * Veikk S640 (and others) driver tablet -- still in production
 * Heavily guided by the Wacom driver (drivers/hid/wacom*)
 * @author Jonathan Lam <jlam55555@gmail.com>
 */

#ifndef VEIKK_H
#define VEIKK_H /*empty*/

#include <linux/hid.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/usb.h>

#define VEIKK_VENDOR_ID         0x2FEB

#define VEIKK_DRIVER_VERSION    "2.0"
#define VEIKK_DRIVER_AUTHOR     "Jonathan Lam <jlam55555@gmail.com>"
#define VEIKK_DRIVER_DESC       "USB VEIKK drawing tablet driver"

#define VEIKK_PEN_REPORT        0x0001

// supported module parameter types
enum veikk_modparm {
    VEIKK_MP_SCREEN_MAP,
    VEIKK_MP_SCREEN_SIZE,
    VEIKK_MP_PRESSURE_MAP,
    VEIKK_MP_ORIENTATION
};
// supported orientations
enum veikk_orientation {
    VEIKK_OR_DFL=0,
    VEIKK_OR_CCW,
    VEIKK_OR_FLIP,
    VEIKK_OR_CW
};
// struct for representing rectangular geometries (physical/mappings)
struct veikk_rect {
    int x_start, y_start, width, height;
};

// pen input report -- structure of input report from tablet
struct veikk_pen_report {
    u8 report_id;
    u8 buttons;
    u16 x, y, pressure;
};

// device-specific properties; one created for every device. These
// characteristics should not be modified anywhere in the program; any
// modifiable properties (e.g., mapped characteristics) should be copied
// over to the device's struct veikk before modifying
struct veikk;
struct veikk_device_info {
    // identifiers
    const char *name;
    const int prod_id;

    // physical characteristics; acts as defaults for mapped characteristics
    const int x_max, y_max, pressure_max;

    // device-specific handlers
    int (*alloc_input_devs)(struct veikk *veikk);
    int (*setup_and_register_input_devs)(struct veikk *veikk);
    int (*handle_raw_data)(struct veikk *veikk, u8 *data, int size,
                           unsigned int report_id);
    int (*handle_modparm_change)(struct veikk *veikk, void *val,
                                 enum veikk_modparm modparm);
};

// common properties for veikk devices
struct veikk {
    // hardware details
    struct hid_device *hdev;

    // device-specific properties
    const struct veikk_device_info *vdinfo;

    // mapped digitizer characteristics; initialized with defaults from
    // struct veikk_device_info
    struct veikk_rect map_rect;
    int pressure_map[4];
    // these are used for orientation mapping
    int x_map_axis, y_map_axis, x_map_dir, y_map_dir;

    struct input_dev *pen_input;
    struct list_head lh;
};

// from veikk_drv.c
extern struct list_head vdevs;
int veikk_input_open(struct input_dev *dev);
void veikk_input_close(struct input_dev *dev);

// from veikk_vdev.c
extern const struct hid_device_id veikk_ids[];

// from veikk_modparms.c
extern u64 veikk_screen_map;
extern u32 veikk_screen_size;
extern u32 veikk_pressure_map;
extern u32 veikk_orientation;

// module parameter (configuration) helper
void veikk_configure_input_devs(u64 sm, u32 ss, enum veikk_orientation or,
                                struct veikk *veikk);
#endif
