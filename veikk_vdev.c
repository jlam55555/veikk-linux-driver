// veikk driver device-specific code
/**
 * For each Veikk device:
 * - Create the appropriate struct veikk_device_info
 * - Set up the appropriate handlers (or use the S640 handlers as a fallback)
 * - Set up the module parameters
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include "veikk.h"

/** BEGIN S640-SPECIFIC CODE **/
// allocate input_dev(s); register input_dev(s) after this; called on probe
// and on module parameter change (on module parameter change, input_devs are
// scrapped and then re-setup with new parameters)
static int veikk_s640_alloc_input_devs(struct veikk *veikk) {
    struct hid_device *hdev = veikk->hdev;

    // devres_open/close_group to make managing multiple device-associated
    // allocs easier to clean up; not really useful for the s640 (only has one
    // input_dev to allocate) but will be useful for the more featured tablets
    if(!devres_open_group(&hdev->dev, veikk, GFP_KERNEL))
        return -ENOMEM;

    if (!(veikk->pen_input = devm_input_allocate_device(&hdev->dev))) {
        return -ENOMEM;
        // TODO: cleanup
    }

    devres_close_group(&hdev->dev, veikk);
    return 0;
}

// assume that proper input_dev(s) already allocated, now set up their props
// and then call input_register_device; this is called after alloc_input_devs
static int veikk_s640_setup_and_register_input_devs(struct veikk *veikk) {
    struct hid_device *hdev = veikk->hdev;
    struct input_dev *pen_input = veikk->pen_input;
    int error;

    // set up input_dev properties
    pen_input->name = veikk->vdinfo->name;
    pen_input->phys = hdev->phys;
    pen_input->open = veikk_input_open;
    pen_input->close = veikk_input_close;
    pen_input->uniq = hdev->uniq;
    pen_input->id.bustype = hdev->bus;
    pen_input->id.vendor = hdev->vendor;
    pen_input->id.product = hdev->product;
    pen_input->id.version = hdev->version;

    // input's internal struct device (and thus its data store) not the same as
    // that of hdev, so must set its data to point to veikk as well
    input_set_drvdata(pen_input, veikk);

    // set up pen capabilities
    pen_input->evbit[0] |= BIT_MASK(EV_KEY)|BIT_MASK(EV_ABS);
    __set_bit(INPUT_PROP_DIRECT, pen_input->propbit);
    __set_bit(INPUT_PROP_POINTER, pen_input->propbit);

    __set_bit(BTN_TOUCH, pen_input->keybit);
    __set_bit(BTN_STYLUS, pen_input->keybit);
    __set_bit(BTN_STYLUS2, pen_input->keybit);

    input_set_abs_params(pen_input, veikk->x_map_axis, veikk->map_rect.x_start,
                         veikk->map_rect.x_start+veikk->map_rect.width, 0, 0);
    input_set_abs_params(pen_input, veikk->y_map_axis, veikk->map_rect.y_start,
                         veikk->map_rect.y_start+veikk->map_rect.height, 0, 0);
    // TODO: work on pressure mapping
    input_set_abs_params(pen_input, ABS_PRESSURE, 0,
                         veikk->vdinfo->pressure_max, 0, 0);

    // TODO: fix resolution (and fuzz, flat) values
    input_abs_set_res(pen_input, veikk->x_map_axis, veikk->x_map_dir);
    input_abs_set_res(pen_input, veikk->y_map_axis, veikk->y_map_dir);

    if((error = input_register_device(pen_input)))
        return error;
    return 0;
}

// emit events from input_dev on input reports
static int veikk_s640_handle_raw_data(struct veikk *veikk, u8 *data, int size,
                                      unsigned int report_id) {
    struct input_dev *pen_input = veikk->pen_input;
    struct veikk_pen_report *pen_report;

    switch(report_id) {
    case VEIKK_PEN_REPORT:
        // validate size
        if(size != sizeof(struct veikk_pen_report))
            return -EINVAL;

        // dispatch events with input_dev
        pen_report = (struct veikk_pen_report *) data;
        input_report_abs(pen_input, veikk->x_map_axis,
                         veikk->x_map_dir*pen_report->x);
        input_report_abs(pen_input, veikk->y_map_axis,
                         veikk->y_map_dir*pen_report->y);
        input_report_abs(pen_input, ABS_PRESSURE, pen_report->pressure);

        input_report_key(pen_input, BTN_TOUCH, pen_report->buttons&0x1);
        input_report_key(pen_input, BTN_STYLUS, pen_report->buttons&0x2);
        input_report_key(pen_input, BTN_STYLUS2, pen_report->buttons&0x4);
        break;
    default:
        hid_info(veikk->hdev, "Unknown input report with id %d\n", report_id);
        return 0;
    }

    // on successful data parse and event emission, emit EV_SYN on input_devs
    input_sync(pen_input);
    return 0;
}
// handle module parameter changes by providing all the necessary calculations
// and setup necessary before refreshing the input device with the new
// parameters. This makes the assumption that all module parameters except for
// the one specified by modparm are updated; the value of the new modparm is
// stored in val
static int veikk_s640_handle_modparm_change(struct veikk *veikk, void *val,
                                            enum veikk_modparm modparm) {
    int error;

    // update parameters depending on which parameter was changed
    switch(modparm) {
    case VEIKK_MP_SCREEN_MAP:
        // TODO: put under spinlock
        veikk_configure_input_devs(*((u64 *) val), veikk_screen_size,
                                   veikk_orientation, veikk);
        break;
    case VEIKK_MP_SCREEN_SIZE:
        // TODO: put under spinlock
        veikk_configure_input_devs(veikk_screen_map, (*(u32 *) val),
                                   veikk_orientation, veikk);
        break;
    case VEIKK_MP_ORIENTATION:
        // TODO: put under spinlock
        veikk_configure_input_devs(veikk_screen_map, veikk_screen_size,
                                   *((u32 *) val), veikk);
        break;
    default:
        // TODO: reword
        hid_info(veikk->hdev, "invalid module parameter selected\n");
        return -EINVAL;
    }

    // un-register device
    input_unregister_device(veikk->pen_input);
    input_free_device(veikk->pen_input);

    // re-alloc device
    if((error = (*veikk->vdinfo->alloc_input_devs)(veikk))) {
        hid_err(veikk->hdev, "alloc_input_devs failed\n");
        return error;
    }

    // re-register device
    // TODO: deal with error codes more efficiently
    if((error = (*veikk->vdinfo->setup_and_register_input_devs)(veikk))) {
        hid_err(veikk->hdev, "setup_and_register_input_devs failed\n");
        return error;
    }

    hid_info(veikk->hdev, "successfully updated module parameters\n");
    return 0;
}
/** END S640-SPECIFIC CODE **/

/** LIST ALL struct veikk_device_info HERE; see declaration for details **/
// for loading into module device table (hotplugging)
struct veikk_device_info veikk_device_info_0x0001 = {
    .name = "VEIKK S640", .prod_id = 0x0001,
    .x_max = 32768, .y_max = 32768, .pressure_max = 8192,
    .setup_and_register_input_devs = veikk_s640_setup_and_register_input_devs,
    .alloc_input_devs = veikk_s640_alloc_input_devs,
    .handle_raw_data = veikk_s640_handle_raw_data,
    .handle_modparm_change = veikk_s640_handle_modparm_change
};
// TODO: the following struct veikk_device_infos are provisional, and use the
//       same handlers as for the S640
struct veikk_device_info veikk_device_info_0x0002 = {
    .name = "VEIKK A30", .prod_id = 0x0002,
    .x_max = 32768, .y_max = 32768, .pressure_max = 8192,
    .setup_and_register_input_devs = veikk_s640_setup_and_register_input_devs,
    .alloc_input_devs = veikk_s640_alloc_input_devs,
    .handle_raw_data = veikk_s640_handle_raw_data,
    .handle_modparm_change = veikk_s640_handle_modparm_change
};
struct veikk_device_info veikk_device_info_0x0003 = {
    .name = "VEIKK A50", .prod_id = 0x0003,
    .x_max = 32768, .y_max = 32768, .pressure_max = 8192,
    .setup_and_register_input_devs = veikk_s640_setup_and_register_input_devs,
    .alloc_input_devs = veikk_s640_alloc_input_devs,
    .handle_raw_data = veikk_s640_handle_raw_data,
    .handle_modparm_change = veikk_s640_handle_modparm_change
};
/** END struct veikk_device LIST **/

#define VEIKK_DEVICE(prod)\
    HID_USB_DEVICE(VEIKK_VENDOR_ID, prod),\
    .driver_data = (long unsigned int) &veikk_device_info_##prod
const struct hid_device_id veikk_ids[] = {
    { VEIKK_DEVICE(0x0001) },
    { VEIKK_DEVICE(0x0002) },
    { VEIKK_DEVICE(0x0003) },
    // TODO: add more devices here
    {}
};
MODULE_DEVICE_TABLE(hid, veikk_ids);
