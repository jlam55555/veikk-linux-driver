/*
 * Linux driver for VEIKK digitizers, v3.0.0
 * S640, A30, A50, A15, A15 Pro, VK1560 tested on GIMP, Krita, kernels 4.18+
 * GitHub: https://www.github.com/jlam55555/veikk-linux-driver
 * See GitHub for descriptions of VEIKK device quirks, setup information, and
 * 		detailed testing environments/data
 */

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

// comment the following line to disable debugging output in kernel log (dmesg)
#define VEIKK_DEBUG_MODE	1

#define VEIKK_VENDOR_ID		0x2FEB
#define VEIKK_DRIVER_VERSION	"3.0.0"
#define VEIKK_DRIVER_DESC	"VEIKK digitizer driver"
#define VEIKK_DRIVER_AUTHOR	"Jonathan Lam <jlam55555@gmail.com>"
#define VEIKK_DRIVER_LICENSE	"GPL"

// TODO: update/remove these
#define VEIKK_PEN_REPORT	0x1
#define VEIKK_STYLUS_REPORT	0x2
#define VEIKK_KEYBOARD_REPORT	0x3

#define VEIKK_BTN_TOUCH		0x1
#define VEIKK_BTN_STYLUS	0x2
#define VEIKK_BTN_STYLUS2	0x4

// raw report structures
// TODO: update/remove these
struct veikk_pen_report {
	u8 report_id;
	u8 btns;
	u16 x, y, pressure;
};
struct veikk_buttons_report {
	u8 report_id, ctrl_modifier;
	u8 btns[6];
};

// veikk model characteristics
struct veikk_model {
	const char *name;
	const int prod_id;
	const int x_max, y_max, pressure_max;

	// TODO: remove this
	const int *pusage_keycode_map;

	const int has_buttons, has_gesture_pad;
};

// veikk device descriptor
struct veikk_device {
	const struct veikk_model *model;
	struct input_dev *pen_input, *buttons_input, *gesture_pad_input;
	struct delayed_work setup_pen_work, setup_buttons_work,
			setup_gesture_pad_work;
	struct hid_device *hid_dev;
};

// circular llist of struct veikk_devices
struct list_head veikk_devs;
struct mutex veikk_devs_mutex;
LIST_HEAD(veikk_devs);
DEFINE_MUTEX(veikk_devs_mutex);

// See BUTTON_MAPPING.txt for an in-depth explanation of button mapping
#define VK_BTN_0		KEY_KP0
#define	VK_BTN_1		KEY_KP1
#define VK_BTN_2		KEY_KP2
#define VK_BTN_3		KEY_KP3
#define	VK_BTN_4		KEY_KP4
#define VK_BTN_5		KEY_KP5
#define VK_BTN_6		KEY_KP6
#define VK_BTN_7		KEY_KP7
#define VK_BTN_ENTER		KEY_ENTER
#define VK_BTN_LEFT		KEY_KPLEFTPAREN
#define VK_BTN_RIGHT		KEY_KPRIGHTPAREN
#define VK_BTN_UP		KEY_KPPLUS
#define VK_BTN_DOWN		KEY_KPMINUS

#define VK_MOD_CTRL		1
#define VK_MOD_SHIFT		1
#define VK_MOD_ALT		1

// don't change this unless a new scancode has been found
#define VEIKK_BTN_COUNT		13

// 1 for default mode, 0 for regular mode
// TODO: turn this into a sysfs parameter; macro used just for simplicity
#define VEIKK_DFL_BTNS		0

static const s8 usage_pusage_map[64] = {
//	  0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
	 -1, -1, -1, -1, -1, -1,  4, -1, -1, -1, -1, -1,  1, -1, -1, -1, // 0
	 -1, -1, -1, -1, -1, -1,  7, -1, -1,  3, -1, -1, -1,  6, -1, -1, // 1
	 -1, -1, -1, -1, -1, -1, -1, -1,  8, -1, -1, -1,  2,  9, 10, 11, // 2
	 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0, -1, // 3
};

// default map (control modifier will be placed separately)
static const int dfl_pusage_key_map[VEIKK_BTN_COUNT] = {
	KEY_F5, KEY_I, KEY_SPACE, KEY_V, KEY_C, KEY_V, KEY_Z, KEY_S,
	KEY_ENTER, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE
};

// use this map as a symbol for the device having no buttons (e.g., for S640)
static const int veikk_no_btns[VEIKK_BTN_COUNT];

// veikk_event and veikk_report are only used for debugging
#ifdef VEIKK_DEBUG_MODE
static int veikk_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	hid_info(hdev, "in veikk_event: usage %x value %d", usage->hid, value);
	return 0;
}
void veikk_report(struct hid_device *hid_dev, struct hid_report *report)
{
	hid_info(hid_dev, "in veikk_report: report id %d", report->id);
}
#endif	// VEIKK_DEBUG_MODE

/*
 * We only use the proprietary veikk interface (usage 0xFF0A), since this emits
 * nicer events than the other interfaces and we don't have to worry about
 * the inconsistent grouping of events under different interfaces. In earlier
 * versions of the driver (v1.0-2.0), we used the generic interfaces, but this
 * caused many issues with button mapping and inconsistencies between
 * interfaces. The Windows/Mac drivers also use the proprietary interface only.
 */
static int veikk_is_proprietary(struct hid_device *hid_dev)
{
	// dev_rdesc and dev_rsize are the device report descriptor and its
	// length, respectively
	u8 *rdesc = hid_dev->dev_rdesc;
	unsigned int rsize = hid_dev->dev_rsize, i;

	return rsize >= 3 && rdesc[0] == 0x06 && rdesc[1] == 0x0A
			&& rdesc[2] == 0xFF;
}

/*
 * This sends the magic bytes to the VEIKK tablets that enable the proprietary
 * interfaces for the pen, buttons, and gesture pad. It seems that sending
 * them in two short of a time interval doesn't enable them all, so these
 * are sent out using delayed workqueue tasks with different delays.
 * 
 * Device types: 0 = pen, 1 = buttons, 2 = gesture pad
 *
 * Note: return code of this is never used
 *
 * TODO: make sure device exists, since this happens asynchronously. (E.g., if
 * user quickly plugs/unplugs device)
 */
static const u8 pen_output_report[9] =
		{ 0x09, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 buttons_output_report[9] = 
		{ 0x09, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 gesture_pad_output_report[9] = 
		{ 0x09, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int veikk_setup_feature(struct work_struct *work, int device_type) {
	struct delayed_work *dwork;
	struct veikk_device *veikk_dev;
	struct hid_device *hid_dev;
	u8 *buf, *output_report;
	int buf_len = sizeof(pen_output_report);

	dwork = container_of(work, struct delayed_work, work);

	switch (device_type) {
	case 0:
		veikk_dev = container_of(dwork, struct veikk_device,
				setup_pen_work);
		output_report = pen_output_report;
		break;
	case 1:
		veikk_dev = container_of(dwork, struct veikk_device,
				setup_buttons_work);
		output_report = buttons_output_report;
		break;
	case 2:
		veikk_dev = container_of(dwork, struct veikk_device,
				setup_gesture_pad_work);
		output_report = gesture_pad_output_report;
		break;
	default:
		return -EINVAL;
	}

	hid_dev = veikk_dev->hid_dev;
	if (!(buf = devm_kzalloc(&hid_dev->dev, buf_len, GFP_KERNEL))) {
		return -ENOMEM;
	}

	memcpy(buf, output_report, buf_len);
	hid_dev->ll_driver->output_report(hid_dev, buf, buf_len);
	return 0;
}
static void veikk_setup_pen(struct work_struct *work)
{
	veikk_setup_feature(work, 0);
}
static void veikk_setup_buttons(struct work_struct *work)
{
	veikk_setup_feature(work, 1);
}
static void veikk_setup_gesture_pad(struct work_struct *work)
{
	veikk_setup_feature(work, 2);
}

static int veikk_pen_event(struct veikk_pen_report *evt,
		struct input_dev *input)
{
	input_report_abs(input, ABS_X, evt->x);
	input_report_abs(input, ABS_Y, evt->y);
	input_report_abs(input, ABS_PRESSURE, evt->pressure);

	input_report_key(input, BTN_TOUCH, evt->btns & VEIKK_BTN_TOUCH);
	input_report_key(input, BTN_STYLUS, evt->btns & VEIKK_BTN_STYLUS);
	input_report_key(input, BTN_STYLUS2, evt->btns & VEIKK_BTN_STYLUS2);

	return 0;
}

static int veikk_buttons_event(struct veikk_buttons_report *evt,
		struct input_dev *input, const int *pusage_keycode_map)
{
	u8 pusages[VEIKK_BTN_COUNT] = { 0 }, i, keys_pressed;
	s8 pusage;
	const int *pusage_key_map;

	// fill pseudo-usages map; this is independent of device
	for (i = 0; i < 6 && evt->btns[i]; ++i) {
		if ((pusage = usage_pusage_map[evt->btns[i]]) == -1)
			return -EINVAL;
		++pusages[pusage];
	}

	// if both Ctrl+V and V are pressed (A50)
	if (pusages[3] && evt->ctrl_modifier) {
		--pusages[3];
		++pusages[5];
	}

	// if default mapping; also report ctrl
	if (VEIKK_DFL_BTNS || pusage_keycode_map == veikk_no_btns) {
		pusage_key_map = dfl_pusage_key_map;
		input_report_key(input, KEY_LEFTCTRL, evt->ctrl_modifier);
	} else {
		pusage_key_map = pusage_keycode_map;
		// if any keys pressed, add Ctrl+Alt+Shift modifiers
		keys_pressed = !!evt->btns[0];
		if (VK_MOD_CTRL)
			input_report_key(input, KEY_LEFTCTRL, keys_pressed);
		if (VK_MOD_ALT)
			input_report_key(input, KEY_LEFTALT, keys_pressed);
		if (VK_MOD_SHIFT)
			input_report_key(input, KEY_LEFTSHIFT, keys_pressed);
	}

	for (i = 0; i < VEIKK_BTN_COUNT; i++) {
		#ifdef VEIKK_DEBUG_MODE
		hid_info((struct hid_device *) input_get_drvdata(input),
				"KEY %d VALUE %d", pusage_key_map[i],
				pusages[i]);
		#endif	// VEIKK_DEBUG_MODE
		input_report_key(input, pusage_key_map[i], pusages[i]);
	}
	return 0;
}

static int veikk_raw_event(struct hid_device *hid_dev,
		struct hid_report *report, u8 *data, int size)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	struct input_dev *input;
	int err;

	switch (report->id) {
	case VEIKK_PEN_REPORT:
	case VEIKK_STYLUS_REPORT:
		if (size != sizeof(struct veikk_pen_report))
			return -EINVAL;

		input = veikk_dev->pen_input;
		if (!input) {
			printk("TESTING 1 2 3");
			return -1;
		}

		if ((err = veikk_pen_event((struct veikk_pen_report *) data,
					input)))
			return err;
		break;
	case VEIKK_KEYBOARD_REPORT:
		if (size != sizeof(struct veikk_buttons_report))
			return -EINVAL;

		#ifdef VEIKK_DEBUG_MODE
		hid_info(hid_dev, "%2x %2x %2x %2x %2x %2x %2x %2x ",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);
		#endif	// VEIKK_DEBUG_MODE

		input = veikk_dev->buttons_input;
		if (!input) {
			printk("TESTING 1 2 5");
			return -1;
		}
		if ((err = veikk_buttons_event(
				(struct veikk_buttons_report *) data, input,
				veikk_dev->model->pusage_keycode_map)))
			return err;
		break;
	default:
		hid_info(hid_dev, "unknown report with id %d", report->id);
		return 0;
	}

	// emit EV_SYN on successful event emission
	input_sync(input);
	return 1;
}

// called by struct input_dev instances, not called directly
static int veikk_input_open(struct input_dev *dev)
{
	return hid_hw_open((struct hid_device *) input_get_drvdata(dev));
}
static void veikk_input_close(struct input_dev *dev)
{
	hid_hw_close((struct hid_device *) input_get_drvdata(dev));
}

static int veikk_setup_pen_input(struct input_dev *input,
		struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	const struct veikk_model *model = veikk_dev->model;
	char *input_name;

	// input name = model name + " Pen"
	if (!(input_name = devm_kzalloc(&input->dev, strlen(model->name)+5,
			GFP_KERNEL)))
		return -ENOMEM;
	sprintf(input_name, "%s Pen", model->name);
	input->name = input_name;

	__set_bit(INPUT_PROP_POINTER, input->propbit);

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);

	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(BTN_STYLUS, input->keybit);
	__set_bit(BTN_STYLUS2, input->keybit);

	// TODO: not sure what resolution, fuzz values should be;
	// these ones seem to work fine for now
	input_set_abs_params(input, ABS_X, 0, model->x_max, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, model->y_max, 0, 0);
	input_set_abs_params(input, ABS_PRESSURE, 0, model->pressure_max, 0, 0);
	input_abs_set_res(input, ABS_X, 100);
	input_abs_set_res(input, ABS_Y, 100);
	return 0;
}

static int veikk_setup_buttons_input(struct input_dev *input,
		struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	const struct veikk_model *model = veikk_dev->model;
	char *input_name;
	int i;

	// input name = model name + " Keyboard"
	if (!(input_name = devm_kzalloc(&input->dev, strlen(model->name)+10,
			GFP_KERNEL)))
		return -ENOMEM;
	sprintf(input_name, "%s Keyboard", model->name);
	input->name = input_name;

	__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_MSC, input->evbit);
	__set_bit(EV_REP, input->evbit);
	__set_bit(MSC_SCAN, input->mscbit);

	// modifiers; sent out both by default and regular map
	__set_bit(KEY_LEFTCTRL, input->keybit);
	__set_bit(KEY_LEFTALT, input->keybit);
	__set_bit(KEY_LEFTSHIFT, input->keybit);

	// possible keys sent out by regular map
	for (i = 0; i < VEIKK_BTN_COUNT; ++i)
		__set_bit(model->pusage_keycode_map[i], input->keybit);

	// possible keys sent out by default map
	for (i = 0; i < VEIKK_BTN_COUNT; ++i)
		__set_bit(dfl_pusage_key_map[i], input->keybit);

	input_enable_softrepeat(input, 100, 33);
	return 0;
}

// TODO: working here
static int veikk_setup_gesture_pad_input(struct input_dev *input,
		struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	const struct veikk_model *model = veikk_dev->model;
	char *input_name;
	int i;

	// input name = model name + " Gesture Pad"
	if (!(input_name = devm_kzalloc(&input->dev, strlen(model->name)+13,
			GFP_KERNEL)))
		return -ENOMEM;
	sprintf(input_name, "%s Gesture Pad", model->name);
	input->name = input_name;
	return 0;
}

/*
 * register input for a struct hid_device. This depends on the device type.
 * returns -errno on failure
 */
/*static int veikk_register_input(struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	const struct veikk_model *model = veikk_dev->model;
	struct input_dev *pen_input, *buttons_input;
	int err;

	// setup appropriate input capabilities
	//if (veikk_dev->type == VEIKK_PROPRIETARY) {
		pen_input = veikk_dev->pen_input;
		if ((err = veikk_register_pen_input(pen_input, model)))
			return err;
	//}
	//if (veikk_dev->type == VEIKK_PROPRIETARY) {
	//} else if (veikk_dev->model->pusage_keycode_map != veikk_no_btns
	//		&& veikk_dev->type == VEIKK_KEYBOARD) {
		buttons_input = veikk_dev->buttons_input;
		if ((err = veikk_register_buttons_input(buttons_input, model)))
			return err;*/
	/*} else {
		// for S640, since it has no buttons input
		return 0;
	}*/

	// common registration for pen and buttons
/*	pen_input->open = veikk_input_open;
	pen_input->close = veikk_input_close;
	pen_input->phys = hid_dev->phys;
	pen_input->uniq = hid_dev->uniq;
	pen_input->id.bustype = hid_dev->bus;
	pen_input->id.vendor = hid_dev->vendor;
	pen_input->id.product = hid_dev->product;
	pen_input->id.version = hid_dev->version;

	buttons_input->open = veikk_input_open;
	buttons_input->close = veikk_input_close;
	buttons_input->phys = hid_dev->phys;
	buttons_input->uniq = hid_dev->uniq;
	buttons_input->id.bustype = hid_dev->bus;
	buttons_input->id.vendor = hid_dev->vendor;
	buttons_input->id.product = hid_dev->product;
	buttons_input->id.version = hid_dev->version;

	// needed for veikk_input_open/veikk_input_close
	input_set_drvdata(pen_input, hid_dev);

	input_set_drvdata(buttons_input, hid_dev);

	input_register_device(pen_input);
	return input_register_device(buttons_input);
}*/

static int veikk_register_input(struct input_dev *input,
			struct hid_device *hid_dev)
{
	input->open = veikk_input_open;
	input->close = veikk_input_close;
	input->phys = hid_dev->phys;
	input->uniq = hid_dev->uniq;
	input->id.bustype = hid_dev->bus;
	input->id.vendor = hid_dev->vendor;
	input->id.product = hid_dev->product;
	input->id.version = hid_dev->version;

	input_set_drvdata(input, hid_dev);
	return input_register_device(input);
}

static int veikk_allocate_setup_register_inputs(struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	int err;
	
	// allocate struct input_devs
	// TODO: fill this out
	//devres_open_group();
	if (!(veikk_dev->pen_input = devm_input_allocate_device(&hid_dev->dev))
			|| (veikk_dev->model->has_buttons
				&& !(veikk_dev->buttons_input =
				devm_input_allocate_device(&hid_dev->dev)))
			|| (veikk_dev->model->has_gesture_pad
				&& !(veikk_dev->gesture_pad_input =
				devm_input_allocate_device(&hid_dev->dev)))) {
		err = -ENOMEM;
		goto bad_alloc;
	}
	// TODO: fill this out
	//devres_close_group();

	// setup struct input_devs
	if ((err = veikk_setup_pen_input(veikk_dev->pen_input, hid_dev))
			|| (veikk_dev->model->has_buttons
				&& (err = veikk_setup_buttons_input(
				veikk_dev->buttons_input, hid_dev)))
			|| (veikk_dev->model->has_gesture_pad
				&& (err = veikk_setup_gesture_pad_input(
				veikk_dev->gesture_pad_input, hid_dev))))
		goto bad_setup;

	// register struct input_devs
	// TODO: refactor like the above
	if ((err = veikk_register_input(veikk_dev->pen_input, hid_dev)))
		goto bad_register;
	if (veikk_dev->model->has_buttons && (err = veikk_register_input(
			veikk_dev->buttons_input, hid_dev)))
		goto bad_register;
	if (veikk_dev->model->has_gesture_pad && (err = veikk_register_input(
			veikk_dev->gesture_pad_input, hid_dev)))
		goto bad_register;

	// setup workqueues to initialize proprietary devices correctly
	// TODO: make the delays customizable (e.g., put in a macro)
	INIT_DELAYED_WORK(&veikk_dev->setup_pen_work, veikk_setup_pen);
	schedule_delayed_work(&veikk_dev->setup_pen_work, 100);
	if (veikk_dev->model->has_buttons) {
		INIT_DELAYED_WORK(&veikk_dev->setup_buttons_work,
				veikk_setup_buttons);
		schedule_delayed_work(&veikk_dev->setup_buttons_work, 200);
	}
	if (veikk_dev->model->has_gesture_pad) {
		INIT_DELAYED_WORK(&veikk_dev->setup_gesture_pad_work,
				veikk_setup_gesture_pad);
		schedule_delayed_work(&veikk_dev->setup_gesture_pad_work, 300);
	}
	return 0;

bad_register:
	// input_unregister_device()
bad_setup:
	// no special unwinding needed here
bad_alloc:
	// TODO: fill this out
	//devres_release_group();
fail:
	return err;
}

// new device inserted
static int veikk_probe(struct hid_device *hid_dev,
		const struct hid_device_id *id)
{
	struct veikk_device *veikk_dev;
	int err;

	// ignore the generic HID interfaces
	if (!veikk_is_proprietary(hid_dev))
		return 0;

	if (!id->driver_data) {
		hid_err(hid_dev, "id->driver_data missing");
		err = -EINVAL;
		goto fail;
	}

	if (!(veikk_dev = devm_kzalloc(&hid_dev->dev,
			sizeof(struct veikk_device), GFP_KERNEL))) {
		hid_err(hid_dev, "error allocating veikk device descriptor");
		err = -ENOMEM;
		goto fail;
	}
	veikk_dev->model = (struct veikk_model *) id->driver_data;
	veikk_dev->hid_dev = hid_dev;
	hid_set_drvdata(hid_dev, veikk_dev);

	if ((err = hid_parse(hid_dev))) {
		hid_err(hid_dev, "hid_parse error");
		goto fail;
	}

	if ((err = veikk_allocate_setup_register_inputs(hid_dev))) {
		hid_err(hid_dev, "error allocating or registering inputs");
		goto fail;
	}

	if ((err = hid_hw_start(hid_dev, HID_CONNECT_HIDRAW
			| HID_CONNECT_DRIVER))) {
		hid_err(hid_dev, "error signaling hardware start");
		goto fail;
	}

	#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "%s probed successfully.", veikk_dev->model->name);
	#endif	// VEIKK_DEBUG_MODE
	return 0;

fail:
	return err;
}

// TODO: be more careful with resources if unallocated
static void veikk_remove(struct hid_device *hid_dev) {
	struct veikk_device *veikk_dev;

	// didn't set up the generic devices, so don't need to clean them up
	if (!veikk_is_proprietary(hid_dev))
		return;

	veikk_dev = hid_get_drvdata(hid_dev);

	if (veikk_dev) {

		hid_hw_stop(hid_dev);
	}

	#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "device removed successfully.");
	#endif	// VEIKK_DEBUG_MODE
}

/*
 * List of veikk models; see BUTTON_MAPPING.txt for more information on the
 * pusage_keycode_map field
 *
 * TODO: need to get button map for some devices
 */
static struct veikk_model veikk_model_0x0001 = {
	.name = "VEIKK S640", .prod_id = 0x0001,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.pusage_keycode_map = veikk_no_btns,
};
static struct veikk_model veikk_model_0x0002 = {
	.name = "VEIKK A30", .prod_id = 0x0002,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.pusage_keycode_map = (int[]) {
		VK_BTN_2, VK_BTN_1, VK_BTN_0, 0,
		0, 0, VK_BTN_3, 0,
		0, VK_BTN_DOWN, VK_BTN_UP, VK_BTN_LEFT, VK_BTN_RIGHT
	},
	.has_buttons = 1, .has_gesture_pad = 1
};
static struct veikk_model veikk_model_0x0003 = {
	.name = "VEIKK A50", .prod_id = 0x0003,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.pusage_keycode_map = (int[]) {
		VK_BTN_0, VK_BTN_1, VK_BTN_2, VK_BTN_3,
		VK_BTN_4, VK_BTN_5, VK_BTN_6, VK_BTN_7,
		0, VK_BTN_DOWN, VK_BTN_UP, VK_BTN_LEFT, VK_BTN_RIGHT
	},
	.has_buttons = 1, .has_gesture_pad = 1
};
static struct veikk_model veikk_model_0x0004 = {
	.name = "VEIKK A15", .prod_id = 0x0004,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.pusage_keycode_map = veikk_no_btns,
	.has_buttons = 1, .has_gesture_pad = 1
};
static struct veikk_model veikk_model_0x0006 = {
	.name = "VEIKK A15 Pro", .prod_id = 0x0006,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.pusage_keycode_map = veikk_no_btns,
	.has_buttons = 1, .has_gesture_pad = 1
};
static struct veikk_model veikk_model_0x1001 = {
	.name = "VEIKK VK1560", .prod_id = 0x1001,
	.x_max = 27536, .y_max = 15488, .pressure_max = 8192,
	.pusage_keycode_map = (int[]) {
		VK_BTN_0, VK_BTN_1, VK_BTN_2, 0,
		VK_BTN_3, VK_BTN_4, VK_BTN_5, VK_BTN_6,
		VK_BTN_ENTER, VK_BTN_DOWN, VK_BTN_UP, VK_BTN_LEFT, VK_BTN_RIGHT
	},
	.has_buttons = 1
};
// TODO: add more tablets

// register models for hotplugging
#define VEIKK_MODEL(prod)\
	HID_USB_DEVICE(VEIKK_VENDOR_ID, prod),\
	.driver_data = (u64) &veikk_model_##prod
static const struct hid_device_id veikk_model_ids[] = {
	{ VEIKK_MODEL(0x0001) },	// S640
	{ VEIKK_MODEL(0x0002) },	// A30
	{ VEIKK_MODEL(0x0003) },	// A50
	{ VEIKK_MODEL(0x0004) },	// A15
	{ VEIKK_MODEL(0x0006) },	// A15 Pro
	{ VEIKK_MODEL(0x1001) },	// VK1560
	{ }
};
MODULE_DEVICE_TABLE(hid, veikk_model_ids);

// register driver
static struct hid_driver veikk_driver = {
	.name = "veikk",
	.id_table = veikk_model_ids,
	.probe = veikk_probe,
	.remove = veikk_remove,
	.raw_event = veikk_raw_event,

	/*
	 * the following are for debugging, .raw_event is the only one used
	 * for real event reporting
	 */
	#ifdef VEIKK_DEBUG_MODE
	.report = veikk_report,
	.event = veikk_event,
	#endif	// VEIKK_DEBUG_MODE
};
module_hid_driver(veikk_driver);

MODULE_VERSION(VEIKK_DRIVER_VERSION);
MODULE_AUTHOR(VEIKK_DRIVER_AUTHOR);
MODULE_DESCRIPTION(VEIKK_DRIVER_DESC);
MODULE_LICENSE(VEIKK_DRIVER_LICENSE);
