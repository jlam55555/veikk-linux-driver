/**
 * Module parameters for Veikk devices. Opens up a (sys)fs interface to
 * customize veikk devices.
 * <p>
 * Module parameters are handled directly below. Basic error checking for each
 * module parameter will occur, and (on success) be delegated to the device-
 * specific handler (in the struct veikk_device_info). See the comments for each
 * module parameter and its formatting in sysfs.
 * <p>
 * For now, all Veikk devices using this driver will use the same parameters.
 *
 * TODO: have more customization options for multiple attached Veikk devices.
 *       e.g., device-specific module parameters? e.g., ones for gesture pad
 */

#include <linux/moduleparam.h>
#include "veikk.h"

// GLOBAL MODULE PARAMETERS
// Note: by spec, unsigned long long is 64+ bits, so functions designed for
//       unsigned long long are used for u64 module parameters (and the same
//       for unsigned int functions for u32 parameters)
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
 * format: serialized struct veikk_screen_size
 * default: 0 (default mapping)
 */
static u32 veikk_screen_size_serial;
struct veikk_rect veikk_screen_size = {
    .x = 0,
    .y = 0,
    .width = 0,
    .height = 0
};
static int veikk_set_veikk_screen_size(const char *val,
                                 const struct kernel_param *kp) {
    int error;
    u32 ss;
    struct list_head *lh;
    struct veikk *veikk;
    struct veikk_screen_size screen_size;

    if((error = kstrtouint(val, 10, &ss)))
        return error;

    // check that entire number is zero, or that width, height > 0 (see desc)
    screen_size = *(struct veikk_screen_size *)&ss;
    if(ss && (screen_size.width<=0 || screen_size.height<=0))
        return -EINVAL;

    // see note in header about deserializing with modparm-specific type and
    // storing in more generic veikk_rect type
    veikk_screen_size = (struct veikk_rect) {
        .x = 0,
        .y = 0,
        .width = screen_size.width,
        .height = screen_size.height
    };

    // call device-specific handlers
    mutex_lock(&vdevs_mutex);
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk)))
            return error;
    }
    mutex_unlock(&vdevs_mutex);
    return param_set_uint(val, kp);
}
static const struct kernel_param_ops veikk_veikk_screen_size_ops = {
    .set = veikk_set_veikk_screen_size,
    .get = param_get_uint
};
module_param_cb(screen_size, &veikk_veikk_screen_size_ops,
                &veikk_screen_size_serial, 0664);
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
 * format: serialized struct veikk_screen_map (64 bits)
 * default: 0 (default mapping)
 */
static u64 veikk_screen_map_serial;
struct veikk_rect veikk_screen_map = {
    .x = 0,
    .y = 0,
    .width = 0,
    .height = 0
};
static int veikk_set_veikk_screen_map(const char *val,
                                      const struct kernel_param *kp) {
    int error;
    u64 sm;
    struct list_head *lh;
    struct veikk *veikk;
    struct veikk_screen_map screen_map;

    if((error = kstrtoull(val, 10, &sm)))
        return error;

    // check that entire number is zero, or that width, height > 0 (see desc)
    screen_map = *(struct veikk_screen_map *)&sm;
    if(sm && (screen_map.width<=0 || screen_map.height<=0))
        return -EINVAL;

    veikk_screen_map = (struct veikk_rect) {
        .x = screen_map.x,
        .y = screen_map.y,
        .width = screen_map.width,
        .height = screen_map.height
    };

    // call device-specific handlers
    mutex_lock(&vdevs_mutex);
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk)))
            return error;
    }
    mutex_unlock(&vdevs_mutex);
    return param_set_ullong(val, kp);
}
static const struct kernel_param_ops veikk_veikk_screen_map_ops = {
        .set = veikk_set_veikk_screen_map,
        .get = param_get_ullong
};
module_param_cb(screen_map, &veikk_veikk_screen_map_ops,
                &veikk_screen_map_serial, 0664);
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
static u32 veikk_orientation_serial;
enum veikk_orientation veikk_orientation;
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

    veikk_orientation = (enum veikk_orientation) or;

    // call device-specific handlers
    mutex_lock(&vdevs_mutex);
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk)))
            return error;
    }
    mutex_unlock(&vdevs_mutex);
    return param_set_uint(val, kp);
}
static const struct kernel_param_ops veikk_orientation_ops = {
    .set = veikk_set_veikk_orientation,
    .get = param_get_uint
};
module_param_cb(orientation, &veikk_orientation_ops, &veikk_orientation_serial,
                0664);
/**
 * pressure_map: cubic coefficients for a pressure mapping
 * <p>
 * Let the output pressure be P, and input pressure be p. This parameter defines
 * a3,a1,a2,a0 such that P=a3*p**3+a2*p**2+a1*p+a0, where p,P are in [0,1]. The
 * pressure function will be scaled to the full pressure input resolution as the
 * resulting pressure is being calculated. Any integer values for any/all of
 * these parameters is valid, but a callback is still available for any desired
 * processing (nothing is done in the default callback).
 * <p>
 * given a3,a2,a1,a0 are signed 16-bit integers (s16)
 * format: serialized struct veikk_pressure_map; in other words,
 *         (((u64)((u16)(100*a3)))<<48) |
 *         (((u64)((u16)(100*a2)))<<32) |
 *         (((u64)((u16)(100*a1)))<<16) |
 *         (((u64)((u16)(100*a0)))<<0)
 * default: 100<<16=6553600 (linear mapping, a3=a2=a0=0, a1=1, P=p)
 * <p>
 * Note: Each coefficient is stored multiplied by 100 in a short int (16-bit)
 * representation. I.e., to represent the coefficients a3=a0=0, a1=54.64,
 * a2=2.5, the pressure_map orientation would be (5464<<32)|(250<<16). This
 * means that any coefficient in the range [-327.68:0.01:327.67] is
 * representable in this format, which should cover reasonable pressure mappings
 * (more extreme pressure mappings may not be representable like this).
 */
static u64 veikk_pressure_map_serial = 100<<16;
// global ease-of-use struct; note that these coefficients still have to be
// divided by 100 and scaled to device pressure resolution later
struct veikk_pressure_map veikk_pressure_map = {
    .a0 = 0,
    .a1 = 100,
    .a2 = 0,
    .a3 = 0
};
static int veikk_set_pressure_map(const char *val,
                                  const struct kernel_param *kp) {
    int error;
    u64 pm;
    struct list_head *lh;
    struct veikk *veikk;

    if((error = kstrtoull(val, 10, &pm)))
        return error;

    // no checks to perform except that it is an integral value
    veikk_pressure_map = *((struct veikk_pressure_map *) &pm);

    // call device-specific handlers
    mutex_lock(&vdevs_mutex);
    list_for_each(lh, &vdevs) {
        veikk = list_entry(lh, struct veikk, lh);

        // TODO: if error, revert all previous changes for consistency?
        if((error = (*veikk->vdinfo->handle_modparm_change)(veikk)))
            return error;
    }
    mutex_unlock(&vdevs_mutex);
    return param_set_ullong(val, kp);
}
static const struct kernel_param_ops veikk_pressure_map_ops = {
        .set = veikk_set_pressure_map,
        .get = param_get_ullong
};
module_param_cb(pressure_map, &veikk_pressure_map_ops,
                &veikk_pressure_map_serial, 0664);

// TODO: module parameter(s) for stylus buttons

/**
 * Helper to perform calculations given screen size/screen map/veikk_orientation,
 * calculating x/y bounds, axes, and directions based on the parameters, so that
 * not much further calculation needs to be done on registering inputs and
 * handling input reports. See veikk_s640_setup_and_register_input_devs and
 * veikk_s640_handle_raw_data for usage examples. Can pass in custom parms to
 * avoid using the global ones (but for now, this function only called with
 * the global ones).
 * <p>
 * In particular, this sets the following settings of the provided struct veikk:
 * - x_map_axis:    ABS_X if the tablet's x-axis maps to screen's +/- x-axis,
 *                  else ABS_Y
 * - y_map_axis:    same as above, but for tablet's y-axis
 * - x_map_dir:     +1 if tablet's x-axis maps to screen's + x/y-axis, else -1
 * - y_map_axis:    same as above, but for tablet's y-axis
 * - map_rect:      dimensions
 */
void veikk_configure_input_devs(struct veikk_rect ss,
                                struct veikk_rect sm,
                                enum veikk_orientation or,
                                struct veikk *veikk) {
    // set veikk_orientation parameters
    veikk->x_map_axis = (or==VEIKK_OR_DFL||or==VEIKK_OR_FLIP) ? ABS_X : ABS_Y;
    veikk->y_map_axis = (or==VEIKK_OR_DFL||or==VEIKK_OR_FLIP) ? ABS_Y : ABS_X;
    veikk->x_map_dir = (or==VEIKK_OR_DFL||or==VEIKK_OR_CW) ? 1 : -1;
    veikk->y_map_dir = (or==VEIKK_OR_DFL||or==VEIKK_OR_CCW) ? 1 : -1;

    // if either sm or ss has zero dimensions, or if sm equal to ss then map to
    // full screen (default mapping; see description for veikk_screen_size and
    // veikk_screen_map)
    if(!sm.width || !sm.height || !ss.width || !ss.height)
        sm = ss = (struct veikk_rect) {
            .x = 0,
            .y = 0,
            .width = 1,
            .height = 1
        };

    // perform the necessary arithmetic based on veikk_orientation to calculate
    // bounds for input_dev
    // TODO: document these calculations
    veikk->map_rect = (struct veikk_rect) {
        .x = -(veikk->x_map_axis==ABS_X
                        ? (sm.x+(veikk->x_map_dir<0)*sm.width)
                            * veikk->vdinfo->x_max/sm.width
                        : (sm.y+(veikk->x_map_dir<0)*sm.height)
                            * veikk->vdinfo->x_max/sm.height),
        .y = -(veikk->y_map_axis==ABS_X
                        ? (sm.x+(veikk->y_map_dir<0)*sm.width)
                            * veikk->vdinfo->y_max/sm.width
                        : (sm.y+(veikk->y_map_dir<0)*sm.height)
                            * veikk->vdinfo->y_max/sm.height),
        .width = veikk->x_map_axis==ABS_X
                    ? ss.width*veikk->vdinfo->x_max/sm.width
                    : ss.height*veikk->vdinfo->x_max/sm.height,
        .height = veikk->y_map_axis==ABS_X
                 ? ss.width*veikk->vdinfo->y_max/sm.width
                 : ss.height*veikk->vdinfo->y_max/sm.height
    };
}
/**
 * Helper to calculate mapped pressure from input pressure and coefficients.
 * The coefficients are for a cubic on a 1x1 region, but we want the output
 * cubic mapping to be on a [pres_max (domain)]x[pres_max (output)] region. This
 * does the appropriate scaling in both x- and y-directions. Additionally, this
 * also scales each coefficient by 1/100 to imitate floating point precision.
 * <p>
 * Based on the size of the numbers (pressure approx. 2^13, coefficients 2^16),
 * all arithmetic fits in (signed) 64-bit integers. Division (in order from
 * smaller to larger dividends) is done after multiplication to increase
 * precision. Bounds are not checked (it should be capped automatically by
 * libinput). Signed 64 bit arithmetic is used throughout, and result is
 * returned as a signed 32-bit integer (int).
 */
int veikk_map_pressure(s64 pres, s64 pres_max,
                       struct veikk_pressure_map *coef) {
    static const int sf = 100;  // constant scale factor of 100 for all coefs
    return (s32) (((coef->a3*pres*pres*pres/pres_max/pres_max)
                 + (coef->a2*pres*pres/pres_max)
                 + (coef->a1*pres)
                 + (coef->a0*pres_max))/sf);
}
