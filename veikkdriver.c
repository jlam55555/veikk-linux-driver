/*
 * based off of wacom driver
 * https://github.com/torvalds/linux/blob/master/drivers/hid/wacom.h
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>
#include <linux/kfifo.h>
#include <linux/leds.h>
#include <linux/usb/input.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h> 

struct veikk {
  struct usb_device *usbdev;
  struct usb_interface *intf;
  /* struct wacom_wac */
}

MODULE_LICENSE("Dual BSD/GPL");
