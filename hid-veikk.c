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

// comment the following line to disable debugging output in kernel log (dmesg)
//#define VEIKK_DEBUG_MODE	1

#define VEIKK_VENDOR_ID 0x2FEB
#define VEIKK_DRIVER_VERSION "3.0.0"
#define VEIKK_DRIVER_DESC "VEIKK digitizer driver"
#define VEIKK_DRIVER_AUTHOR "Jonathan Lam <jlam55555@gmail.com>"
#define VEIKK_DRIVER_LICENSE "GPL"

#define VEIKK_PEN_REPORT 0x1
#define VEIKK_STYLUS_REPORT 0x2
#define VEIKK_KEYBOARD_REPORT 0x3

#define VEIKK_BTN_TOUCH 0x1
#define VEIKK_BTN_STYLUS 0x2
#define VEIKK_BTN_STYLUS2 0x4

#define VEIKK_BTN_COUNT 13

// TODO: turn this into a sysfs parameter; macro used just for simplicity
#define VEIKK_DFL_BTNS 0

// raw report structures
struct veikk_pen_report
{
	u8 report_id;
	u8 btns;
	u16 x, y, pressure;
};
struct veikk_keyboard_report
{
	u8 report_id, ctrl_modifier;
	u8 btns[6];
};

// types of veikk hid_devices; see quirks for more details because keyboard
// and digitizer inputs are not exact
enum veikk_hid_type
{
	VEIKK_UNKNOWN = -EINVAL, // erroroneous report format
	VEIKK_PROPRIETARY = 0,	 // this input to be ignored
	VEIKK_KEYBOARD,			 // keyboard input
	VEIKK_PEN				 // pen (drawing pad) input
};

// veikk model descriptor
struct veikk_model
{
	// basic parameters
	const char *name;
	const int prod_id;

	// drawing pad physical characteristics
	const int x_max, y_max, pressure_max;

	// button mapping; should be an array of length VEIKK_BTN_COUNT
	const int *btn_map;
};

// veikk hid device descriptor
struct veikk_device
{
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
#endif // VEIKK_DEBUG_MODE

/*
 * possible VEIKK buttons; these indices will be "pseudo-usages" ("pusage"s)
 * used to remap to specific keys, depending on the device
 *  0,  1,  2,  3,  4,        5,  6,  7,  8,  9, 10, 11, 12
 * 3e, 0c, 2c, 19, 06, 19+Ctrl*, 1d, 16, 28, 2d, 2e, 2f, e0
 * note that the usage 19 (KEY_V) can be used with or without a Ctrl modifier
 * on the A50, but this is resolved in the handler
 */
static const s8 usage_pusage_map[64] = {
	//	  0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
	-1, -1, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, 1, -1, -1, -1,  // 0
	-1, -1, -1, -1, -1, -1, 7, -1, -1, 3, -1, -1, -1, 6, -1, -1,   // 1
	-1, -1, -1, -1, -1, -1, -1, -1, 8, -1, -1, -1, 2, 9, 10, 11,   // 2
	12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, // 3
};

// default map (control modifier will be placed separately)
static const int dfl_pusage_key_map[VEIKK_BTN_COUNT] = {
	KEY_F5, KEY_I, KEY_SPACE, KEY_V, KEY_C, KEY_V, KEY_Z, KEY_S,
	KEY_ENTER, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE};

// use this map as a symbol for the device having no buttons (e.g., for S640)
static const int veikk_no_btns[VEIKK_BTN_COUNT];

/*
 * identify veikk type by report parsing; pen input, keyboard input, or
 * proprietary; the distinction is not so clean, so this is not so clean;
 * returns -EINVAL if type not recognized; can be thought of as a very
 * simple custom hid_parse becauase of the quirks
 */
static enum veikk_hid_type veikk_identify_device(struct hid_device *hid_dev)
{
	// dev_rdesc and dev_rsize are the device report descriptor and its
	// length, respectively
	u8 *rdesc = hid_dev->dev_rdesc;
	unsigned int rsize = hid_dev->dev_rsize, i;

#ifdef VEIKK_DEBUG_MODE
	// print out device report descriptor
	hid_info(hid_dev, "DEV RDESC (len %d)", rsize);
	for (i = 0; i < rsize; i++)
		printk(KERN_CONT "%x ", rdesc[i]);
	printk(KERN_INFO "");
#endif // VEIKK_DEBUG_MODE

	// just to be safe
	if (rsize < 3)
		return VEIKK_UNKNOWN;

	// check if proprietary; always begins with the byte sequence
	// 0x06 0x0A 0xFF
	if (rdesc[0] == 0x06 && rdesc[1] == 0x0A && rdesc[2] == 0xFF)
		return VEIKK_PROPRIETARY;

	// check if keyboard; always has the byte sequence 0x05 0x01 0x09 0x06
	// TODO: possibly misidentifications on untested devices, am not sure
	for (i = 0; i < rsize - 4; i++)
		if (rdesc[i] == 0x05 && rdesc[i + 1] == 0x01 && rdesc[i + 2] == 0x09 && rdesc[i + 3] == 0x06)
			return VEIKK_KEYBOARD;

	// default case is digitizer
	// TODO: put some stricter check on this to protect against
	// possible misidentifications?
	return VEIKK_PEN;
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

static int veikk_keyboard_event(struct veikk_keyboard_report *evt,
								struct input_dev *input, const int *btn_map)
{
	u8 pusages[VEIKK_BTN_COUNT] = {0}, i;
	s8 pusage;
	const int *pusage_key_map;

	// fill pseudo-usages map; this is independent of device
	for (i = 0; i < /*6*/ VEIKK_BTN_COUNT && evt->btns[i]; ++i)
	{
		if ((pusage = usage_pusage_map[evt->btns[i]]) == -1)
			return -EINVAL;
		++pusages[pusage];
	}

	// if both Ctrl+V and V are pressed (A50)
	if (pusages[3] && evt->ctrl_modifier)
	{
		--pusages[3];
		++pusages[5];
	}

	// if default mapping; also report ctrl
	if (VEIKK_DFL_BTNS || btn_map == veikk_no_btns)
	{
		input_report_key(input, KEY_LEFTCTRL, evt->ctrl_modifier);
		pusage_key_map = dfl_pusage_key_map;
	}
	else
	{
		pusage_key_map = btn_map;
	}

	for (i = 0; i < VEIKK_BTN_COUNT; i++)
	{
#ifdef VEIKK_DEBUG_MODE
		hid_info(hid_dev, "KEY %d VALUE %d", pusage_key_map[i],
				 pusages[i]);
#endif // VEIKK_DEBUG_MODE
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

	switch (report->id)
	{
	case VEIKK_PEN_REPORT:
	case VEIKK_STYLUS_REPORT:
		if (size != sizeof(struct veikk_pen_report))
			return -EINVAL;

		input = veikk_dev->pen_input;
		if ((err = veikk_pen_event((struct veikk_pen_report *)data,
								   input)))
			return err;
		break;
	case VEIKK_KEYBOARD_REPORT:
		if (size != sizeof(struct veikk_keyboard_report))
			return -EINVAL;

#ifdef VEIKK_DEBUG_MODE
		hid_info(hid_dev, "%2x %2x %2x %2x %2x %2x %2x %2x ",
				 data[0], data[1], data[2], data[3],
				 data[4], data[5], data[6], data[7]);
#endif // VEIKK_DEBUG_MODE

		input = veikk_dev->keyboard_input;
		if ((err = veikk_keyboard_event((struct veikk_keyboard_report *)
											data,
										input, veikk_dev->model->btn_map)))
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
	return hid_hw_open((struct hid_device *)input_get_drvdata(dev));
}
static void veikk_input_close(struct input_dev *dev)
{
	hid_hw_close((struct hid_device *)input_get_drvdata(dev));
}

static int veikk_register_pen_input(struct input_dev *input,
									const struct veikk_model *model)
{
	char *input_name;

	// input name = model name + " Pen"
	if (!(input_name = devm_kzalloc(&input->dev, strlen(model->name) + 5,
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

static int veikk_register_keyboard_input(struct input_dev *input,
										 const struct veikk_model *model)
{
	char *input_name;
	int i;

	// input name = model name + " Keyboard"
	if (!(input_name = devm_kzalloc(&input->dev, strlen(model->name) + 9,
									GFP_KERNEL)))
		return -ENOMEM;
	sprintf(input_name, "%s Keyboard", model->name);
	input->name = input_name;

	__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_MSC, input->evbit);
	__set_bit(EV_REP, input->evbit);
	__set_bit(MSC_SCAN, input->mscbit);

	// possible keys sent out by regular map
	for (i = 0; i < VEIKK_BTN_COUNT; ++i)
		__set_bit(model->btn_map[i], input->keybit);

	// possible keys sent out by default map
	__set_bit(KEY_LEFTCTRL, input->keybit);
	for (i = 0; i < VEIKK_BTN_COUNT; ++i)
		__set_bit(dfl_pusage_key_map[i], input->keybit);

	input_enable_softrepeat(input, 100, 33);
	return 0;
}

/*
 * register input for a struct hid_device. This depends on the device type.
 * returns -errno on failure
 */
static int veikk_register_input(struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);
	const struct veikk_model *model = veikk_dev->model;
	struct input_dev *input;
	int err;

	// setup appropriate input capabilities
	if (veikk_dev->type == VEIKK_PEN)
	{
		input = veikk_dev->pen_input;
		if ((err = veikk_register_pen_input(input, model)))
			return err;
	}
	else if (veikk_dev->model->btn_map != veikk_no_btns && veikk_dev->type == VEIKK_KEYBOARD)
	{
		input = veikk_dev->keyboard_input;
		if ((err = veikk_register_keyboard_input(input, model)))
			return err;
	}
	else
	{
		// for S640, since it has no keyboard input
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

	if ((dev_type = veikk_identify_device(hid_dev)) == VEIKK_UNKNOWN)
	{
		hid_err(hid_dev, "could not identify veikk device hid type");
		return dev_type;
	}

	// proprietary device emits no events, ignore
	if (dev_type == VEIKK_PROPRIETARY)
		return 0;

	if (!id->driver_data)
	{
		hid_err(hid_dev, "id->driver_data missing");
		return -EINVAL;
	}

	// allocate veikk device
	if (!(veikk_dev = devm_kzalloc(&hid_dev->dev,
								   sizeof(struct veikk_device), GFP_KERNEL)))
	{
		hid_info(hid_dev, "allocating struct veikk_device");
		return -ENOMEM;
	}

	// configure struct veikk_device and set as hid device descriptor
	veikk_dev->model = (struct veikk_model *)id->driver_data;
	veikk_dev->usb_dev = usb_dev;
	veikk_dev->type = dev_type;
	hid_set_drvdata(hid_dev, veikk_dev);

	if ((err = hid_parse(hid_dev)))
	{
		hid_info(hid_dev, "hid_parse");
		return err;
	}

	// allocate struct input_devs
	if (dev_type == VEIKK_PEN && !(veikk_dev->pen_input =
									   devm_input_allocate_device(&hid_dev->dev)))
	{
		hid_info(hid_dev, "allocating digitizer input");
		return -ENOMEM;
	}
	else if (dev_type == VEIKK_KEYBOARD && !(veikk_dev->keyboard_input =
												 devm_input_allocate_device(&hid_dev->dev)))
	{
		hid_info(hid_dev, "allocating keyboard input");
		return -ENOMEM;
	}

	/*
	 * veikk devices have multiple interfaces, which *may not correspond
	 * to their functions* (e.g., one interface may have both keyboard and
	 * pen functionalities, and the other none; or they may have one each);
	 * thus, assign one struct input_dev to each. The one that is allocated
	 * second will find the one that is allocated first and "share" their
	 * struct input_devs so that they can emit the event on the correct
	 * device, thus rectifying the interface mismatch

	 * TODO: remove from llist if some other failure later in probe
	 */
	mutex_lock(&veikk_devs_mutex);
	/*
	 * walk the list, see if this is the first or the second struct
	 * hid_device to be added to this list; if it is the second, (i.e.,
	 * there is a struct hid_device with a matching usb_dev), set the
	 * appropriate pointers to the other device's inputs and add it to the
	 * linked list; else it is the first device, just add to list
	 */
	list_for_each(lh, &veikk_devs)
	{
		veikk_dev_it = list_entry(lh, struct veikk_device, lh);

		if (veikk_dev_it->usb_dev != veikk_dev->usb_dev)
			continue;

		if (dev_type == VEIKK_PEN)
		{
			veikk_dev_it->pen_input = veikk_dev->pen_input;
			veikk_dev->keyboard_input =
				veikk_dev_it->keyboard_input;
		}
		else
		{
			veikk_dev->pen_input = veikk_dev_it->pen_input;
			veikk_dev_it->keyboard_input =
				veikk_dev->keyboard_input;
		}
		break;
	}
	list_add(&veikk_dev->lh, &veikk_devs);
	mutex_unlock(&veikk_devs_mutex);

	if ((err = veikk_register_input(hid_dev)))
	{
		hid_err(hid_dev, "error registering inputs");
		return err;
	}

	if ((err = hid_hw_start(hid_dev, HID_CONNECT_HIDRAW | HID_CONNECT_DRIVER)))
	{
		hid_err(hid_dev, "error signaling hardware start");
		return err;
	}

#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "%s probed successfully.", veikk_dev->model->name);
#endif // VEIKK_DEBUG_MODE
	return 0;
}

// TODO: be more careful with resources if unallocated
static void veikk_remove(struct hid_device *hid_dev)
{
	struct veikk_device *veikk_dev = hid_get_drvdata(hid_dev);

	if (veikk_dev)
	{
		mutex_lock(&veikk_devs_mutex);
		list_del(&veikk_dev->lh);
		mutex_unlock(&veikk_devs_mutex);

		hid_hw_stop(hid_dev);
	}

#ifdef VEIKK_DEBUG_MODE
	hid_info(hid_dev, "device removed successfully.");
#endif // VEIKK_DEBUG_MODE
}

/*
 * list of veikk models; see usage_pusage_map for more details on the
 * button_map field
 *
 * TODO: need to get button map for some devices
 */
static struct veikk_model veikk_model_0x0001 = {
	.name = "VEIKK S640",
	.prod_id = 0x0001,
	.x_max = 32768,
	.y_max = 32768,
	.pressure_max = 8192,
	.btn_map = veikk_no_btns};
static struct veikk_model veikk_model_0x0002 = {
	.name = "VEIKK A30",
	.prod_id = 0x0002,
	.x_max = 32768,
	.y_max = 32768,
	.pressure_max = 8192,
	.btn_map = veikk_no_btns};
static struct veikk_model veikk_model_0x0003 = {
	.name = "VEIKK A50",
	.prod_id = 0x0003,
	.x_max = 32768,
	.y_max = 32768,
	.pressure_max = 8192,
	.btn_map = (int[]){BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, 0, KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT}};
static struct veikk_model veikk_model_0x0004 = {
	.name = "VEIKK A15",
	.prod_id = 0x0004,
	.x_max = 32768,
	.y_max = 32768,
	.pressure_max = 8192,
	.btn_map = veikk_no_btns};
static struct veikk_model veikk_model_0x0006 = {
	.name = "VEIKK A15 Pro",
	.prod_id = 0x0006,
	.x_max = 32768,
	.y_max = 32768,
	.pressure_max = 8192,
	// .btn_map = dfl_pusage_key_map};
	.btn_map = (int[]){
		// BTN_WHEEL,
		KEY_0,	   // K1
		KEY_1,	   // K2
		KEY_SPACE, // wheel button
		KEY_2,	   // K3
		KEY_3,
		KEY_4,
		KEY_5, // K4
		KEY_6,
		KEY_7,
		KEY_DOWN, // wheel anti-clockwise
		KEY_UP,	  // wheel clockwise
		KEY_8,
		KEY_9}};
static struct veikk_model veikk_model_0x1001 = {
	.name = "VEIKK VK1560",
	.prod_id = 0x1001,
	.x_max = 27536,
	.y_max = 15488,
	.pressure_max = 8192,
	.btn_map = (int[]){BTN_0, BTN_1, BTN_2, 0, BTN_3, BTN_4, BTN_5, BTN_6, BTN_WHEEL, KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT}};
// TODO: add more tablets

// register models for hotplugging
#define VEIKK_MODEL(prod)                  \
	HID_USB_DEVICE(VEIKK_VENDOR_ID, prod), \
		.driver_data = (u64)&veikk_model_##prod
static const struct hid_device_id veikk_model_ids[] = {
	{VEIKK_MODEL(0x0001)}, // S640
	{VEIKK_MODEL(0x0002)}, // A30
	{VEIKK_MODEL(0x0003)}, // A50
	{VEIKK_MODEL(0x0004)}, // A15
	{VEIKK_MODEL(0x0006)}, // A15 Pro
	{VEIKK_MODEL(0x1001)}, // VK1560
	{}};
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
#endif // VEIKK_DEBUG_MODE
};
module_hid_driver(veikk_driver);

MODULE_VERSION(VEIKK_DRIVER_VERSION);
MODULE_AUTHOR(VEIKK_DRIVER_AUTHOR);
MODULE_DESCRIPTION(VEIKK_DRIVER_DESC);
MODULE_LICENSE(VEIKK_DRIVER_LICENSE);
