/**
 * Module parameters for Veikk devices. Opens up a (sys)fs interface to
 * customize veikk devices.
 * <p>
 * Module parameters are handled directly below. Basic error checking for each
 * module parameter will occur, and (on success) be delegated to the device-
 * specific handler (in the struct veikk_device_info). See the comments for each
 * module parameter and its formatting in sysfs.
 * <p>
 * For now, all Veikk devices using this driver will have the same parameters.
 *
 * TODO: have more customization options for multiple attached Veikk devices.
 *       e.g., device-specific module parameters? e.g., ones for gesture pad
 * TODO: create a tool to enable options on startup.
 * TODO: worry about concurrency issues/locking
 */

#include <linux/moduleparam.h>
#include "veikk.h"

// local helper functions
static struct veikk_rect veikk_ss_to_rect(u32 ss);
static struct veikk_rect veikk_sm_to_rect(u64 sm);

// GLOBAL MODULE PARAMETERS
// Note: by spec, unsigned long long is 64+ bits, so functions designed for
//       unsigned long long are used for u64 module parameters (and the same
//       for unsigned int functions for u32 parameters)
/**
 * veikk_screen_map: region of the screen to map
 * <p>
 * Sets the start x, start y, width, and height of the mapped region on the
 * screen. Should make sure veikk_screen_size is also valid. Valid values:
 * - all zero (default mapping); or
 * - width,height>0 (start_x,start_y can be any number)
 * Note that if either veikk_screen_map or veikk_screen_size are all zero, then
 * the mapping will be set to the default.
 * <p>
 * format: (u64) ((((u16)start_x)<<48) | (((u16)start_y)<<32)
 *                | (((u16)width)<<16) | ((u16)height))
 * default: 0 (default mapping)
 */
u64 veikk_screen_map;
static int veikk_set_veikk_screen_map(const char *val,
                                const struct kernel_param *kp) {
    int error;
    u64 sm;
    struct list_head *lh;
    struct veikk *veikk;
    struct veikk_rect rect;

    if((error = kstrtoull(val, 10, &sm)))
        return error;

    // check that entire number is zero, or that width, height > 0 (see desc)
    rect = veikk_sm_to_rect(sm);
    if(sm && (rect.width<=0 || rect.height<=0))
        return -EINVAL;

    // call device-specific handlers
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk, &sm,
                                                          VEIKK_MP_SCREEN_MAP)))
            return error;
    }
    return param_set_ullong(val, kp);
}
static const struct kernel_param_ops veikk_veikk_screen_map_ops = {
    .set = veikk_set_veikk_screen_map,
    .get = param_get_ullong
};
module_param_cb(screen_map, &veikk_veikk_screen_map_ops, &veikk_screen_map,
                0664);
/**
 * veikk_screen_size: total size of the screen area
 * <p>
 * Set the dimensions (width, height) of the entire screen region. Is necessary
 * to make the veikk_screen_map work correctly. Valid values:
 * - all zero (default mapping); or
 * - width,height>0
 * Note that if either veikk_screen_map or veikk_screen_size are all zero,
 * then the mapping will be set to the default.
 * <p>
 * format: (u32) ((((u16)width)<<16) | ((u16)height))
 * default: 0 (default mapping)
 */
u32 veikk_screen_size;
static int veikk_set_veikk_screen_size(const char *val,
                                 const struct kernel_param *kp) {
    int error;
    u32 ss;
    struct list_head *lh;
    struct veikk *veikk;
    struct veikk_rect rect;

    if((error = kstrtouint(val, 10, &ss)))
        return error;

    // check that entire number is zero, or that width, height > 0 (see desc)
    rect = veikk_ss_to_rect(ss);
    if(ss && (rect.width<=0 || rect.height<=0))
        return -EINVAL;

    // call device-specific handlers
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk, &ss,
                                                         VEIKK_MP_SCREEN_SIZE)))
            return error;
    }
    return param_set_uint(val, kp);
}
static const struct kernel_param_ops veikk_veikk_screen_size_ops = {
    .set = veikk_set_veikk_screen_size,
    .get = param_get_uint
};
module_param_cb(screen_size, &veikk_veikk_screen_size_ops, &veikk_screen_size,
                0664);
/**
 * pressure_map: cubic coefficients for a pressure mapping
 * <p>
 * Let the output pressure be P, and input pressure be p. This parameter defines
 * a3,a1,a2,a0 such that P=a3*p^3+a2*p^2+a1*p+a0. Any integer values for any/all
 * of these parameters is valid.
 * <p>
 * format: (u32) ((((u8)a3)<<24) | (((u8)a2)<<16) | (((u8)a1)<<8) | ((u8)a0))
 * default: 256 (linear mapping, a3=a2=a0=0, a1=1, P=p)
 */
u32 pressure_map;
/**
 * veikk_orientation: set device veikk_orientation
 * <p>
 * Set device veikk_orientation to one of the following values:
 * - 0 (default)
 * - 1 (rotated cw 90deg)
 * - 2 (rotated 180deg)
 * - 3 (rotated ccw 90deg)
 * No other values are valid.
 * <p>
 * format: the veikk_orientation number [0|1|2|3]
 * default: 0
 */
// TODO: change this to a smaller type (i.e., u8/byte)
u32 veikk_orientation;
static int veikk_set_veikk_orientation(const char *val,
                                 const struct kernel_param *kp) {
    int error;
    u32 or;
    struct list_head *lh;
    struct veikk *veikk;

    if((error = kstrtouint(val, 10, &or)))
        return error;

    // check that number is an integer in [0, 3]
    if(or > 3)
        return -ERANGE;

    // call device-specific handlers
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk, &or,
                                                         VEIKK_MP_ORIENTATION)))
            return error;
    }
    return param_set_uint(val, kp);
}
static const struct kernel_param_ops veikk_veikk_orientation_ops = {
    .set = veikk_set_veikk_orientation,
    .get = param_get_uint
};
module_param_cb(orientation, &veikk_veikk_orientation_ops, &veikk_orientation,
                0664);

// TODO: module parameter(s) for stylus buttons

// HELPER FUNCTIONS CORRESPONDING TO THE DESCRIPTIONS ABOVE
// screen size to struct veikk rect
static struct veikk_rect veikk_ss_to_rect(u32 ss) {
    struct veikk_rect rect = {
        .x_start = 0,
        .y_start = 0,
        .width = (u16)(ss>>16),
        .height = (u16)ss
    };
    return rect;
}
// screen map to struct veikk_rect
static struct veikk_rect veikk_sm_to_rect(u64 sm) {
    struct veikk_rect rect = {
        .x_start = (u16)(sm>>48),
        .y_start = (u16)(sm>>32),
        .width = (u16)(sm>>16),
        .height = (u16)sm
    };
    return rect;
}
/**
 * Helper to perform calculations given screen size/screen map/veikk_orientation,
 * calculating x/y bounds, axes, and directions based on the parameters, so that
 * not much further calculation needs to be done on registering inputs and
 * handling input reports. See veikk_s640_setup_and_register_input_devs and
 * veikk_s640_handle_raw_data for usage examples.
 * <p>
 * In particular, this sets the following settings of the provided struct veikk:
 * - x_map_axis:    ABS_X if the tablet's x-axis maps to screen's +/- x-axis,
 *                  else ABS_Y
 * - y_map_axis:    same as above, but for tablet's y-axis
 * - x_map_dir:     +1 if tablet's x-axis maps to screen's + x/y-axis, else -1
 * - y_map_axis:    same as above, but for tablet's y-axis
 * - map_rect:      dimensions
 */
void veikk_configure_input_devs(u64 sm, u32 ss, enum veikk_orientation or,
                                struct veikk *veikk) {
    struct veikk_rect ss_rect, sm_rect;

    // set veikk_orientation parameters
    veikk->x_map_axis = (or==VEIKK_OR_DFL||or==VEIKK_OR_FLIP) ? ABS_X : ABS_Y;
    veikk->y_map_axis = (or==VEIKK_OR_DFL||or==VEIKK_OR_FLIP) ? ABS_Y : ABS_X;
    veikk->x_map_dir = (or==VEIKK_OR_DFL||or==VEIKK_OR_CW) ? 1 : -1;
    veikk->y_map_dir = (or==VEIKK_OR_DFL||or==VEIKK_OR_CCW) ? 1 : -1;

    // if either sm or ss is all zeroes, then map to full screen (default
    // mapping; see description for veikk_screen_size and veikk_screen_map)
    if(!ss || !sm) {
        // setting veikk_screen_map equal to veikk_screen_size makes the default
        // mapping
        ss = (u32) ((1<<16)|1);
        sm = (u64) ss;
    }

    // perform the necessary arithmetic based on veikk_orientation to calculate
    // bounds for input_dev
    // TODO: document these calculations
    ss_rect = veikk_ss_to_rect(ss);
    sm_rect = veikk_sm_to_rect(sm);
    veikk->map_rect = (struct veikk_rect) {
        .x_start = -(veikk->x_map_axis==ABS_X
                        ? (sm_rect.x_start+(veikk->x_map_dir<0)*sm_rect.width)
                            * veikk->vdinfo->x_max/sm_rect.width
                        : (sm_rect.y_start+(veikk->x_map_dir<0)*sm_rect.height)
                            * veikk->vdinfo->x_max/sm_rect.height),
        .y_start = -(veikk->y_map_axis==ABS_X
                        ? (sm_rect.x_start+(veikk->y_map_dir<0)*sm_rect.width)
                            * veikk->vdinfo->y_max/sm_rect.width
                        : (sm_rect.y_start+(veikk->y_map_dir<0)*sm_rect.height)
                            * veikk->vdinfo->y_max/sm_rect.height),
        .width = veikk->x_map_axis==ABS_X
                    ? ss_rect.width*veikk->vdinfo->x_max/sm_rect.width
                    : ss_rect.height*veikk->vdinfo->x_max/sm_rect.height,
        .height = veikk->y_map_axis==ABS_X
                 ? ss_rect.width*veikk->vdinfo->y_max/sm_rect.width
                 : ss_rect.height*veikk->vdinfo->y_max/sm_rect.height
    };
}
