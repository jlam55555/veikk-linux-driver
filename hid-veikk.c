// Linux driver for VEIKK digitizers, v3.0.0
// S640, A30, A50, A15, A15 Pro, VK1560 tested on GIMP, Krita, kernels 4.4+
// GitHub: https://www.github.com/jlam55555/veikk-linux-driver
// See GitHub for descriptions of VEIKK device quirks, setup information, and
// 		detailed testing environments/data

#include <linux/kernel.h>

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/usb.h>

// comment the following line to disable debugging output in kernel log (dmesg)
//#define VEIKK_DEBUG_MODE	1

#define VEIKK_VENDOR_ID		0x2FEB
#define VEIKK_DRIVER_VERSION	"3.0.0"
#define VEIKK_DRIVER_DESC	"VEIKK digitizer driver"
#define VEIKK_DRIVER_AUTHOR	"Jonathan Lam <jlam55555@gmail.com>"
#define VEIKK_DRIVER_LICENSE	"GPL"

#define VEIKK_PEN_REPORT	0x1
#define VEIKK_STYLUS_REPORT	0x2
#define VEIKK_KEYBOARD_REPORT	0x3

#define VEIKK_BTN_TOUCH		0x1
#define VEIKK_BTN_STYLUS	0x2
#define VEIKK_BTN_STYLUS2	0x4

// TODO: turn this into a sysfs parameter; macro used just for simplicity
#define VEIKK_DFL_BTNS		0

// raw report structures
struct veikk_pen_report {
	u8 report_id;
	u8 buttons;
	u16 x, y, pressure;
};
struct veikk_keyboard_report {
	u8 report_id, ctrl_modifier;
	u8 btns[6];
};

// types of veikk hid_devices; see quirks for more details because keyboard
// and digitizer inputs are not exact
enum veikk_hid_type {
	VEIKK_UNKNOWN = -EINVAL,	// erroroneous report format
	VEIKK_PROPRIETARY = 0,		// this input to be ignored
	VEIKK_KEYBOARD,			// keyboard input
	VEIKK_PEN			// pen (drawing pad) input
};

// veikk model descriptor
struct veikk_model {
	// basic parameters
	const char *name;
	const int prod_id;

	// drawing pad physical characteristics
	const int x_max, y_max, pressure_max;

	// keyboard physical characteristics
	const int button_count;
};

// veikk hid device descriptor
struct veikk_device {
	// hardware details
	struct usb_device *usb_dev;

	// device type
	enum veikk_hid_type type;

	// model-specific properties
	const struct veikk_model *model;

	// input devices
	struct input_dev *pen_input, *keyboard_input;

	// linked-list header
	struct list_head lh;
};

// circular llist of struct veikk_devices
struct list_head veikk_devs;
struct mutex veikk_devs_mutex;
LIST_HEAD(veikk_devs);
DEFINE_MUTEX(veikk_devs_mutex);

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

// possible VEIKK buttons; these indices will be "pseudo-usages" used to remap
// to specific keys, depending on the device
//  0,  1,  2,  3,  4,        5,  6,  7,  8,  9, 10, 11, 12
// 3e, 0c, 2c, 19, 06, 19+Ctrl*, 1d, 16, 28, 2d, 2e, 2f, e0
// note that the usage 19 (KEY_V) can be used with or without a Ctrl modifier
// on the A50, but this is resolved in the handler
static const s8 usage_pseudousage_map[64] = {
//	  0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
	 -1, -1, -1, -1, -1, -1,  4, -1, -1, -1, -1, -1,  1, -1, -1, -1, // 0
	 -1, -1, -1, -1, -1, -1,  7, -1, -1,  3, -1, -1, -1,  6, -1, -1, // 1
	 -1, -1, -1, -1, -1, -1, -1, -1,  8, -1, -1, -1,  2,  9, 10, 11, // 2
	 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0, -1, // 3
};

// default map (control modifier will be placed separately)
// TODO: declare this value 13 in a const somewhere
static int dfl_pseudousage_key_map[13] = {
	KEY_F5, KEY_I, KEY_SPACE, KEY_V, KEY_C, KEY_V, KEY_Z, KEY_S,
	KEY_ENTER, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE
};

// for testing; this is the A50 map
static int a50_pseudousage_key_map[13] = {
	BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7,
	0, KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT
};

static int veikk_raw_event(struct hid_device *hid_dev,
		struct hid_report *report, u8 *data, int size)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	struct veikk_pen_report *pen_report;
	struct veikk_keyboard_report *keyboard_report;
	struct input_dev *input;

	u8 pseudousages[13] = { 0 }, i;
	s8 pseudousage;
	int *pseudousage_key_map;

	//dump_stack();

	// TODO: move these into their own functions, nested too deep
	switch (report->id) {
	case VEIKK_PEN_REPORT:
	case VEIKK_STYLUS_REPORT:
		// validate size
		if (size != sizeof(struct veikk_pen_report))
			return -EINVAL;

		pen_report = (struct veikk_pen_report *) data;
		input = veikk_dev->pen_input;

		input_report_abs(input, ABS_X, pen_report->x);
		input_report_abs(input, ABS_Y, pen_report->y);
		input_report_abs(input, ABS_PRESSURE, pen_report->pressure);

		input_report_key(input, BTN_TOUCH,
				pen_report->buttons & VEIKK_BTN_TOUCH);
		input_report_key(input, BTN_STYLUS,
				pen_report->buttons & VEIKK_BTN_STYLUS);
		input_report_key(input, BTN_STYLUS2,
				pen_report->buttons & VEIKK_BTN_STYLUS2);
		break;
	case VEIKK_KEYBOARD_REPORT:
		#ifdef VEIKK_DEBUG_MODE
		hid_info(hid_dev, "%2x %2x %2x %2x %2x %2x %2x %2x ",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);
		#endif	// VEIKK_DEBUG_MODE

		if (size != sizeof(struct veikk_keyboard_report))
			return -EINVAL;

		keyboard_report = (struct veikk_keyboard_report *) data;
		input = veikk_dev->keyboard_input;

		// fill pseudo-usages map; this is independent of device
		for (i = 0; i < 6 && keyboard_report->btns[i]; ++i) {
			if ((pseudousage = usage_pseudousage_map[keyboard_report->btns[i]]) == -1)
				return -EINVAL;
			++pseudousages[pseudousage];
		}

		// if both Ctrl+V and V are pressed (A50)
		if (pseudousages[3] && keyboard_report->ctrl_modifier) {
			--pseudousages[3];
			++pseudousages[5];
		}

		// if default mapping; also report ctrl
		if (!VEIKK_DFL_BTNS) {
			input_report_key(input, KEY_LEFTCTRL,
					keyboard_report->ctrl_modifier);
			pseudousage_key_map = dfl_pseudousage_key_map;
		} else {
			// mapping = veikk->model->button_map;
			pseudousage_key_map = a50_pseudousage_key_map;
		}

		for (i = 0; i < 13; i++) {
			hid_info(hid_dev, "KEY %d VALUE %d", pseudousage_key_map[i], pseudousages[i]);
			input_report_key(input, pseudousage_key_map[i], pseudousages[i]);
		}

		break;
	default:
		hid_info(hid_dev, "unknown report with id %d", report->id);
		return 0;
	}

	// emit EV_SYN on successful event emission
	input_sync(input);
	return 1;
}

// identify veikk type by report parsing; pen input, keyboard input, or
// proprietary; the distinction is not so clean, so this is not so clean;
// returns -EINVAL if type not recognized; can be thought of as a very
// simple custom hid_parse becauase of the quirks
static enum veikk_hid_type veikk_identify_device(struct hid_device *hid_dev)
{
	// dev_rdesc and dev_rsize are the device report descriptor and its
	// length, respectively
	u8 *rdesc = hid_dev->dev_rdesc;
	unsigned int rsize = hid_dev->dev_rsize, i;

	// TODO: remove; print out device report descriptor
	#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "DEV RDESC (len %d)", rsize);
	for (i = 0; i < rsize; i++)
		printk(KERN_CONT "%x ", rdesc[i]);
	printk(KERN_INFO "");
	#endif	// VEIKK_DEBUG_MODE

	// just to be safe
	if (rsize < 3)
		return VEIKK_UNKNOWN;
	
	// check if proprietary; always begins with the byte sequence
	// 0x06 0x0A 0xFF
	if (rdesc[0] == 0x06 && rdesc[1] == 0x0A && rdesc[2] == 0xFF)
		return VEIKK_PROPRIETARY;

	// check if keyboard; always has the byte sequence 0x05 0x01 0x09 0x06
	// TODO: possibly misidentifications on untested devices, am not sure
	for (i = 0; i < rsize-4; i++)
		if (rdesc[i] == 0x05 && rdesc[i+1] == 0x01
				&& rdesc[i+2] == 0x09 && rdesc[i+3] == 0x06)
			return VEIKK_KEYBOARD;

	// default case is digitizer
	// TODO: put some stricter check on this to protect against
	// possible misidentifications?
	return VEIKK_PEN;
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

// register input for a struct hid_device. This depends on the device type.
// returns -errno on failure
static int veikk_register_input(struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	struct input_dev *input;
	char *input_name;
	int i;

	// setup appropriate input capabilities
	if (veikk_dev->type == VEIKK_PEN) {
		input = veikk_dev->pen_input;

		// input name = model name + " Pen"
		if (!(input_name = devm_kzalloc(&hid_dev->dev,
				strlen(veikk_dev->model->name)+5, GFP_KERNEL)))
			return -ENOMEM;
		sprintf(input_name, "%s Pen", veikk_dev->model->name);
		input->name = input_name;

		__set_bit(INPUT_PROP_POINTER, input->propbit);

		__set_bit(EV_KEY, input->evbit);
		__set_bit(EV_ABS, input->evbit);

		__set_bit(BTN_TOUCH, input->keybit);
		__set_bit(BTN_STYLUS, input->keybit);
		__set_bit(BTN_STYLUS2, input->keybit);

		// TODO: not sure what resolution, fuzz values should be;
		// these ones seem to work fine for now
		input_set_abs_params(input, ABS_X, 0,
				veikk_dev->model->x_max, 0, 0);
		input_set_abs_params(input, ABS_Y, 0,
				veikk_dev->model->y_max, 0, 0);
		input_set_abs_params(input, ABS_PRESSURE, 0,
				veikk_dev->model->pressure_max, 0, 0);
		input_abs_set_res(input, ABS_X, 100);
		input_abs_set_res(input, ABS_Y, 100);
	} else if (veikk_dev->model->button_count
			&& veikk_dev->type == VEIKK_KEYBOARD) {
		input = veikk_dev->keyboard_input;

		// input name = model name + " Keyboard"
		if (!(input_name = devm_kzalloc(&hid_dev->dev,
				strlen(veikk_dev->model->name)+9, GFP_KERNEL)))
			return -ENOMEM;
		sprintf(input_name, "%s Keyboard", veikk_dev->model->name);
		input->name = input_name;

		__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);

		__set_bit(EV_KEY, input->evbit);
		__set_bit(EV_MSC, input->evbit);
		__set_bit(EV_REP, input->evbit);

		__set_bit(MSC_SCAN, input->mscbit);

		// possible keys sent out by button mappings
		__set_bit(BTN_0, input->keybit);
		__set_bit(BTN_1, input->keybit);
		__set_bit(BTN_2, input->keybit);
		__set_bit(BTN_3, input->keybit);
		__set_bit(BTN_4, input->keybit);
		__set_bit(BTN_5, input->keybit);
		__set_bit(BTN_6, input->keybit);
		__set_bit(BTN_7, input->keybit);
		__set_bit(KEY_DOWN, input->keybit);
		__set_bit(KEY_UP, input->keybit);
		__set_bit(KEY_LEFT, input->keybit);
		__set_bit(KEY_RIGHT, input->keybit);

		// possible keys sent out by default map
		__set_bit(KEY_LEFTCTRL, input->keybit);
		for (i = 0; i < 13; ++i)
			__set_bit(dfl_pseudousage_key_map[i], input->keybit);

		input_enable_softrepeat(input, 100, 33);

		// NASTY WORKAROUND FOR VEIKK A50/VK1560 (as tested, may be
		// others) device reports both keyboard and pen events from the
		// same struct hid_device, need this to trigger the correct
		// events; this makes the keyboard look like a mouse/pointer
		// to the X server, even though it doesn't emit any pointer
		// events
		// TODO: would like to get rid of these, but it seems they
		// need to be here to work, may be related to report desc
		// TODO: list any other devices that have this quirk
		/*__set_bit(BTN_TOUCH, input->keybit);
		__set_bit(BTN_STYLUS, input->keybit);
		__set_bit(BTN_STYLUS2, input->keybit);

		// set abs params so it's recognized by evdev, but fill with
		// bogus values to indicate that these events are invalid
		__set_bit(EV_ABS, input->evbit);
		input_set_abs_params(input, ABS_X, 0, 1, 0, 0);
		input_set_abs_params(input, ABS_Y, 0, 1, 0, 0);
		input_set_abs_params(input, ABS_PRESSURE, 0, 1, 0, 0);
		input_abs_set_res(input, ABS_X, 1);
		input_abs_set_res(input, ABS_Y, 1);*/
	} else {
		// if device type == VEIKK_KEYBOARD and no buttons, may
		// be needed in some rare possible edge case, given the
		// known existing quirks; in this case, just return
		// successfully without registering an input device
		return 0;
	}

	// common registration for pen and keyboard
	input->open = veikk_input_open;
	input->close = veikk_input_close;
	input->phys = hid_dev->phys;
	input->uniq = hid_dev->uniq;
	input->id.bustype = hid_dev->bus;
	input->id.vendor = hid_dev->vendor;
	input->id.product = hid_dev->product;
	input->id.version = hid_dev->version;

	// needed for veikk_input_open/veikk_input_close
	input_set_drvdata(input, hid_dev);

	return input_register_device(input);
}

// new device inserted
static int veikk_probe(struct hid_device *hid_dev,
		const struct hid_device_id *id)
{
	struct usb_interface *usb_intf = to_usb_interface(hid_dev->dev.parent);
	struct usb_device *usb_dev = interface_to_usbdev(usb_intf);
	struct veikk_device *veikk_dev, *veikk_dev_it;
	struct list_head *lh;
	enum veikk_hid_type dev_type;
	int err;

	if ((dev_type = veikk_identify_device(hid_dev)) == VEIKK_UNKNOWN) {
		hid_err(hid_dev, "could not identify veikk device hid type");
		return dev_type;
	}
	
	// proprietary device emits no events, ignore
	if (dev_type == VEIKK_PROPRIETARY)
		return 0;

	if (!id->driver_data) {
		hid_err(hid_dev, "id->driver_data missing");
		return -EINVAL;
	}

	// allocate veikk device
	if (!(veikk_dev = devm_kzalloc(&hid_dev->dev,
			sizeof(struct veikk_device), GFP_KERNEL))) {
		hid_info(hid_dev, "allocating struct veikk_device");
		return -ENOMEM;
	}
	
	// configure struct veikk_device and set as hid device descriptor
	veikk_dev->model = (struct veikk_model *) id->driver_data;
	veikk_dev->usb_dev = usb_dev;
	veikk_dev->type = dev_type;
	hid_set_drvdata(hid_dev, veikk_dev);

	if ((err = hid_parse(hid_dev))) {
		hid_info(hid_dev, "hid_parse");
		return err;
	}

	if (!(devres_open_group(&hid_dev->dev, veikk_dev, GFP_KERNEL))) {
		hid_info(hid_dev, "opening devres group");
		return -ENOMEM;
	}

	// allocate input
	if (dev_type == VEIKK_PEN && !(veikk_dev->pen_input =
			devm_input_allocate_device(&hid_dev->dev))) {
		hid_info(hid_dev, "allocating digitizer input");
		return -ENOMEM;
	} else if (dev_type == VEIKK_KEYBOARD && !(veikk_dev->keyboard_input =
			devm_input_allocate_device(&hid_dev->dev))) {
		hid_info(hid_dev, "allocating keyboard input");
		return -ENOMEM;
	}

	devres_close_group(&hid_dev->dev, veikk_dev);

	// all veikk devices have exactly one keyboard and one pen input
	// (see definitions from description of quirks); these will be
	// allocated on their respective devices, and the latter of the two
	// will set the allocated devices on both struct_devices through
	// this atomic linked list code
	// TODO: remove from mutex if some other failure later in probe
	mutex_lock(&veikk_devs_mutex);
	// walk the list, see if this is the first or the second struct
	// hid_device to be added to this list; if so, set the appropriate
	// pointers to the other device's inputs and add it to the linked list;
	// else just add to list
	list_for_each(lh, &veikk_devs) {
		veikk_dev_it = list_entry(lh, struct veikk_device, lh);

		if (veikk_dev_it->usb_dev != veikk_dev->usb_dev)
			continue;

		if (dev_type == VEIKK_PEN) {
			veikk_dev_it->pen_input = veikk_dev->pen_input;
			veikk_dev->keyboard_input =
					veikk_dev_it->keyboard_input;
		} else {
			veikk_dev->pen_input = veikk_dev_it->pen_input;
			veikk_dev_it->keyboard_input =
					veikk_dev->keyboard_input;
		}
		break;
	}
	list_add(&veikk_dev->lh, &veikk_devs);
	mutex_unlock(&veikk_devs_mutex);

	if ((err = veikk_register_input(hid_dev))) {
		hid_err(hid_dev, "error registering inputs");
		return err;
	}

	if ((err = hid_hw_start(hid_dev, HID_CONNECT_HIDRAW
			| HID_CONNECT_DRIVER))) {
		hid_err(hid_dev, "error signaling hardware start");
		return err;
	}

	// TODO: remove; for testing
	hid_info(hid_dev, "%s probed successfully.", veikk_dev->model->name);
	return 0;
}

// TODO: be more careful with resources if unallocated
static void veikk_remove(struct hid_device *hid_dev) {
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);

	if (veikk_dev) {
		mutex_lock(&veikk_devs_mutex);
		list_del(&veikk_dev->lh);
		mutex_unlock(&veikk_devs_mutex);

		hid_hw_stop(hid_dev);
	}

	#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "device removed successfully.");
	#endif	// VEIKK_DEBUG_MODE
}

// list of veikk models
static struct veikk_model veikk_model_0x0001 = {
	.name = "VEIKK S640", .prod_id = 0x0001,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.button_count = 0
};
static struct veikk_model veikk_model_0x0003 = {
	.name = "VEIKK A50", .prod_id = 0x0003,
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.button_count = 1
};
static struct veikk_model veikk_model_0x1001 = {
	.name = "VEIKK VK1560", .prod_id = 0x1001,
	// FIXME: not correct dims
	.x_max = 32768, .y_max = 32768, .pressure_max = 8192,
	.button_count = 1
};
// TODO: add more tablets

// register models for hotplugging
#define VEIKK_MODEL(prod)\
	HID_USB_DEVICE(VEIKK_VENDOR_ID, prod),\
	.driver_data = (u64) &veikk_model_##prod
static const struct hid_device_id veikk_model_ids[] = {
	{ VEIKK_MODEL(0x0001) },
	{ VEIKK_MODEL(0x0003) },
	{ VEIKK_MODEL(0x1001) },
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

	// the following are for debugging, .raw_event is the only one used
	// for real event reporting
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
