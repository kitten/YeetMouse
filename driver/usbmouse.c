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

static unsigned int usb_mouse_events(struct input_handle *handle, struct input_value *vals, unsigned int count) {
  struct input_handler *handler = handle->handler;
  struct input_value *end = vals;
  struct input_value *v;

  struct input_value *v_x = NULL;
  struct input_value *v_y = NULL;
  struct input_value *v_wheel = NULL;

  /* Find input_value for EV_REL events we're interested in and store pointers */
  for (v = vals; v != vals + count; v++) {
    if (v->type != ENV_REL)
      continue;
    switch (v->code) {
    case REL_X:
      v_x = v;
      break;
    case REL_Y:
      v_y = v;
      break;
    case REL_WHEEL:
      v_wheel = v;
      break;
    } /* TODO: What if we get duplicate events before a SYN? */
  }

  if (v_x != NULL || v_y != NULL || v_wheel != NULL) {
    /* Retrieve values if any EV_REL events were found */
    int x = NONE_EVENT_VALUE;
    int y = NONE_EVENT_VALUE;
    int wheel = NONE_EVENT_VALUE;
    if (v_x != NULL)
      x = v_x->value;
    if (v_y != NULL)
      y = v_y->value;
    if (v_wheel != NULL)
      wheel = v_wheel->value;
    /* Attempt to apply acceleration */
    if (!accelerate(&x, &y, &wheel)) {
      /* If successful, apply new values to events, filtering out zeroed values */
      for (v = vals; v != vals + count; v++) {
        switch (v) {
        case v_x:
          if (x == NONE_EVENT_VALUE)
            continue;
          v->value = x;
          break;
        case v_y:
          if (y == NONE_EVENT_VALUE)
            continue;
          v->value = y;
          break;
        case v_wheel:
          if (wheel == NONE_EVENT_VALUE)
            continue;
          v->value = wheel;
          break;
        }
        if (end != v)
          *end = *v;
        end++;
      }
      return end - vals;
    }
  }

  /* Otherwise return events unchanged */
  return count;
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
  .events = usb_mouse_events,
  .connect = usb_mouse_connect,
  .disconnect = usb_mouse_disconnect,
  .match = usb_mouse_match
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
