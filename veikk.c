#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>
#include <linux/kfifo.h>
#include <linux/leds.h>
#include <linux/usb/input.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>

#define VEIKK_PKGLEN_MAX  361

// struct for something else?
struct veikk_vei {
  struct input_dev *pen_input;
  struct input_dev *touch_input;
  struct input_dev *pad_input;
  struct kfifo_rec_ptr_2 pen_fifo;
}

// struct to hold driver data
struct veikk {
  struct usb_device *usbdev;
  struct usb_interface *intf;
  struct veikk_vei veikk_vei; 
  struct hid_device *hdev;
};

static const struct hid_device_id id_table[] = {
  { HID_USB_DEVICE(0x2feb, 0x0001) },
  { }
};
MODULE_DEVICE_TABLE(hid, id_table);

// other functions
int veikk_setup_pen_input_capabilities(struct input_dev *input_dev, struct veikk_vei *veikk_vei) {
  input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
  // or __set_bit(INPUT_PROP_POINTER, input_dev->propbit);

  __set_bit(BTN_TOUCH, input_dev->keybit);
  __set_bit(ABS_MISC, input_dev->absbit);

  input_set_abs_params(input_dev, ABS_X, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_Y, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_PRESSURE, 0, 8191, 0, 0);
  input_abs_set_res(input_dev, ABS_X, 100);
  input_abs_set_res(input_dev, ABS_Y, 100);

  return 0;
}
int veikk_setup_touch_input_capabilities(struct input_dev *input_dev, struct veikk_vei *veikk_vei) {

  input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
  // or __set_bit(INPUT_PROP_POINTER, input_dev->propbit);

  __set_bit(BTN_TOUCH, input_dev->keybit);

  input_set_abs_params(input_dev, ABS_X, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_Y, 0, 32767, 0, 0);
  input_abs_set_res(input_dev, ABS_X, 100);   // TODO: get actual resolution (this is just a guess for now, not sure of actual value)
  input_abs_set_res(input_dev, ABS_Y, 100);

  return 0;
}
static struct input_dev *veikk_allocate_input(struct veikk *veikk) {
  struct input_dev *input_dev;
  struct hid_device *hdev = veikk->hdev;
  struct veikk_vei *veikk_vei = &(veikk->veikk_vei);

  input_dev = devm_input_allocate_device(&hdev->dev);
  if(!input_dev)
    return NULL;

  input_dev->name = "will be overwritten so not important"; // TODO: fix this
  input_dev->phys = hdev->phys;
  input_dev->dev.parent = &hdev->dev;
  input_dev->open = veikk_open; // TODO: implement this
  input_dev->close = veikk_close;
  input_dev->uniq = hdev->uniq;
  input_dev->id.bustype = hdev->bus;
  input_dev->id.vendor = hdev->vendor;
  input_dev->id.product = hdev->product;
  input_dev->id.version = hdev->version;
  input_set_drvdata(input_dev, veikk);

  return input_dev;
}
static int veikk_allocate_inputs(struct veikk *veikk) {
  struct veikk_vei *veikk_vei = &(veikk->veikk_vei);

  veikk_vei->pen_input = veikk_allocate_input(veikk);
  veikk_vei->touch_input = veikk_allocate_input(veikk);
  veikk_vei->pad_input = veikk_allocate_input(veikk);

  if(!veikk_vei->pen_input || !veikk_vei->touch_input || !veikk_vei->pad_input)
    return -ENOMEM;

  veikk_vei->pen_input->name = "Testing pen 1";
  veikk_vei->touch_input->name = "Testing touch 1";
  veikk_vei->pad_input->name = "Testing pad 1";

  return 0;
}
static int veikk_register_inputs(struct veikk *veikk) {
  struct input_dev *pen_input_dev, *touch_input_dev, *pad_input_dev;
  struct veikk_vei *veikk_vei = &(veikk->veikk_vei);
  int error = 0;

  pen_input_dev = veikk_vei->pen_input;
  touch_input_dev = veikk_vei->touch_input;
  pad_input_dev = veikk_vei->pad_input;

  // TODO: is there a pen on this thing?
  error = veikk_setup_pen_input_capabilities(pen_input_dev, veikk_vei);
  if(error) {
    input_free_device(pen_input_dev);
    veikk_vei->pen_input = NULL;
    pen_input_dev = NULL;
  } else {
    error = input_register_device(pen_input_dev);
    if(error)
      goto fail;
  }
  
  // TODO: is there a touch on this thing?
  error = veikk_setup_touch_input_capabilities(touch_input_dev, veikk_vei);
  if(error) {
    input_free_device(touch_input_dev);
    veikk_vei->touch_input = NULL;
    touch_input_dev = NULL;
  } else {
    error = input_register_device(touch_input_dev);
    if(error)
      goto fail;
  }

  // TODO: is there a pad on this thing?
  // I think not? (i.e., no pad buttons)
  input_free_device(pad_input_dev);
  veikk_vei->pad_input = NULL;
  pad_input_dev = NULL;
  
  return 0;

fail:
  printk(KERN_WARN "Error from veikk_register_inputs(): %i", error);
  veikk_vei->pen_input = NULL;
  veikk_vei->touch_input = NULL;
  veikk_vei->pad_input = NULL;
  return error;
}
static int veikk_parse_and_register(struct veikk *veikk) {
  struct veikk_vei *veikk_vei = &veikk->veikk_vei;
  struct hid_device *hdev = veikk->hdev;
  int error;
  unsigned int connect_mask = HID_CONNECT_HIDRAW;

  if(!devres_open_group(&hdev->dev, veikk, GFP_KERNEL))
    return -ENOMEM;

  error = veikk_allocate_inputs(veikk);
  if(error)
    goto fail;
  
  // wacom_set_default_phy(features)
  
  // wacom_retrieve_hid_descriptor(hdev, features)
  // wacom_setup_device_quirks

  // wacom_calculate_res

  // error = wacom_add_shared_data(hdev)

  error = veikk_register_inputs(veikk);
  if(error)
    goto fail;
  
  // connect_mask |= HID_CONNECT_DRIVER;
  
  error = hid_hw_start(hdev, connect_mask);
  if(error) {
    hid_err(hdev, "hw start failed\n");
    goto fail;
  }

  // wacom_query_tablet_data

  // wacom_set_shared_values
  devres_close_group(&hdev->dev, veikk);

fail:
  // wacom_release_resources
  printk(KERN_WARN "Error from veikk_parse_and_register(): %i", error);
  return error;
}

// module functions
static int veikk_probe(struct hid_device *hdev, const struct hid_device_id *id) {
  printk(KERN_INFO "Inside veikk_probe()");
  struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
  struct usb_device *dev = interface_to_usbdev(intf);
  struct veikk *veikk;
  struct veikk_vei *veikk_vei;
  int error;
  
  if(!id->driver_data)
    return -EINVAL;

  veikk = devm_kzalloc(&hdev->dev, sizeof(struct veikk), GFP_KERNEL);
  if(!veikk) {
    error = -ENODEV;
    goto fail;
  }

  hid_set_drvdata(hdev, veikk);
  veikk->hdev = hdev;
  veikk_vei = &veikk->veikk_vei;
  
  error = kfifo_alloc(&veikk_vei->pen_fifo, VEIKK_PKGLEN_MAX, GFP_KERNEL);
  if(error)
    goto fail;

  veikk->usbdev = dev;
  veikk->intf = intf;

  error = hid_parse(hdev);
  if(error) {
    hid_err(hdev, "parse failed\n");
    goto fail;
  }

  error = veikk_parse_and_register(veikk);
  if(error)
    goto fail;

  return 0;

fail:
  hid_set_drvdata(hdev, NULL);
  printk(KERN_WARN "Error in veikk_probe(): %i");
  return error;

}
static void veikk_remove(struct hid_device *hdev) {
  printk(KERN_INFO "Inside veikk_remove()");

  struct veikk *veikk = hid_get_drvdata(hdev);
	struct veikk_vei *veikk_vei = &veikk->veikk_vei;

	hid_hw_stop(hdev);

	kfifo_free(&veikk_vei->pen_fifo);

	hid_set_drvdata(hdev, NULL);
}
static void veikk_report(struct hid_device *hdev, struct hid_report *report) {
  printk(KERN_INFO "Inside veikk_report()");
}
static int veikk_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *raw_data, int size) {
  printk(KERN_INFO "Inside veikk_raw_event()");
  return 0;
}

static struct hid_driver veikk_driver = {
  .name       = "veikk",
  .id_table   = id_table,
  .probe      = veikk_probe,
  .remove     = veikk_remove,
  .report     = veikk_report,
  .raw_event  = veikk_raw_event
};
module_hid_driver(veikk_driver);

MODULE_LICENSE("GPL");
