// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 1999-2001 Vojtech Pavlik, USB HIDBP Mouse support
 * Copyright (c) Gnarus-G, maccel
 * This file has an input handler derived from the maccel project
 * https://github.com/Gnarus-G/maccel/blob/dedaa97/driver/input_handler.h
 */

#include "accel.h"
#include "config.h"
#include "util.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/version.h>

#define DRIVER_VERSION "v1.6"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@ucw.cz>"
#define DRIVER_DESC "USB HID mouse driver with acceleration (LEETMOUSE)"

#define NONE_EVENT_VALUE 0
static int usb_mouse_event_lock = 0;
static int rel_event_values[REL_CNT] = {
  /* [x, y, ..., wheel, ...] */ NONE_EVENT_VALUE
};

static void usb_mouse_report_rel(struct input_dev *dev, u32 code, int value) {
  usb_mouse_event_lock++;
  input_report_rel(dev, code, value);
}

static bool usb_mouse_filter(struct input_handle *handle, u32 type, u32 code, int value) {
  switch (type) {
    case EV_SYN: {
      int x = rel_event_values[REL_X];
      int y = rel_event_values[REL_Y];
      int wheel = rel_event_values[REL_WHEEL];

      if (!accelerate(&x, &y, &wheel)) {
        usb_mouse_report_rel(handle->dev, REL_X, x);
        usb_mouse_report_rel(handle->dev, REL_Y, y);
        usb_mouse_report_rel(handle->dev, REL_WHEEL, wheel);
      }

      rel_event_values[REL_X] = NONE_EVENT_VALUE;
      rel_event_values[REL_Y] = NONE_EVENT_VALUE;
      rel_event_values[REL_WHEEL] = NONE_EVENT_VALUE;

      for (u32 code = REL_Z; code < REL_CNT; code++) {
        if (rel_event_values[code] != NONE_EVENT_VALUE) {
          usb_mouse_report_rel(handle->dev, code, rel_event_values[code]);
          rel_event_values[code] = NONE_EVENT_VALUE;
        }
      }

      return false;
    }

    case EV_REL:
      if (code < REL_Z || code > REL_CNT) {
        return false;
      } else if (usb_mouse_event_lock > 0) {
        usb_mouse_event_lock--;
        return false;
      } else {
        rel_event_values[code] = value;
        return true;
      }

    default:
      return false;
  }
}

static bool usb_mouse_match(struct input_handler *handler, struct input_dev *dev) {
  if (!dev->dev.parent)
    return false;
  struct hid_device *hdev = to_hid_device(dev->dev.parent);
  printk("LEETMOUSE: found a possible mouse %s", hdev->name);
  return hdev->type == HID_TYPE_USBMOUSE;
}

static int usb_mouse_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) {
  struct input_handle *handle;
  int error;

  handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
  if (!handle)
    return -ENOMEM;

  handle->dev = input_get_device(dev);
  handle->handler = handler;
  handle->name = "leetmouse";

  error = input_register_handle(handle);
  if (error)
    goto err_free_mem;

  error = input_open_device(handle);
  if (error)
    goto err_unregister_handle;

  printk(pr_fmt("LEETMOUSE: connecting to device: %s (%s at %s)"), dev_name(&dev->dev), dev->name ?: "unknown", dev->phys ?: "unknown");

  return 0;

err_unregister_handle:
  input_unregister_handle(handle);

err_free_mem:
  kfree(handle);
  return error;
}

static void usb_mouse_disconnect(struct input_handle *handle) {
  input_close_device(handle);
  input_unregister_handle(handle);
  kfree(handle);
}

static const struct input_device_id usb_mouse_ids[] = {
  {
    .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
    .evbit = { BIT_MASK(EV_REL) }
  },
  {},
};

MODULE_DEVICE_TABLE(input, usb_mouse_ids);

struct input_handler usb_mouse_handler = {
  .name = "leetmouse",
  .id_table = usb_mouse_ids,
  .connect = usb_mouse_connect,
  .disconnect = usb_mouse_disconnect,
  .match = usb_mouse_match,
  .filter = usb_mouse_filter,
};

static int __init usb_mouse_init(void) {
  return input_register_handler(&usb_mouse_handler);
}

static void __exit usb_mouse_exit(void) {
  input_unregister_handler(&usb_mouse_handler);
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_init(usb_mouse_init);
module_exit(usb_mouse_exit);
