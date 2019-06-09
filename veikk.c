#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>
#include <linux/kfifo.h>
#include <linux/leds.h>
#include <linux/usb/input.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>

#define HID_GROUP_VEIKK       104
#define USB_VENDOR_ID_VEIKK   0x2feb

struct veikk_features {
  const char *name;
  int x_max;
  int y_max;
  int pressure_max;
  int distance_max;
  int type;
  int x_resolution;
  int y_resolution;
  /* more? */
};
const struct veikk_features veikk_features_0x0001 = {
  "Veikk S640",
  32767,
  32767,
  8191,
  63,
  0,  /* ??? */
  100,
  100
};
static const struct hid_device_id id_table[] = {
//  { HID_DEVICE(BUS_USB, 0, USB_VENDOR_ID_VEIKK, 0x0001), .driver_data = (kernel_ulong_t)&veikk_features_0x0001 },
  { HID_DEVICE(HID_BUS_ANY, HID_GROUP_ANY, HID_ANY_ID, HID_ANY_ID) },
  {  }
};
MODULE_DEVICE_TABLE(hid, id_table);

// module functions
static int veikk_probe(struct hid_device *hdev, const struct hid_device_id *id) {
  printk(KERN_INFO "Inside veikk_probe()");
  return 0;
}
static void veikk_remove(struct hid_device *hdev) {
  printk(KERN_INFO "Inside veikk_remove()");
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
