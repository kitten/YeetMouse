// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 *
 *  USB HIDBP Mouse support
 */

/*
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include "accel.h"
#include "config.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/version.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.6"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@ucw.cz>"
#define DRIVER_DESC "USB HID mouse driver with acceleration (LEETMOUSE)"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

struct usb_mouse {
    char name[128];
    char phys[64];
    struct usb_device *usbdev;
    struct input_dev *dev;
    struct urb *irq;

    signed char *data;
    u8 poll_interval;
};

/* See: https://github.com/torvalds/linux/blob/c964ced7726294d40913f2127c3f185a92cb4a41/drivers/hid/usbhid/usbmouse.c#L49-L86 */
static void usb_mouse_irq(struct urb *urb)
{
    struct usb_mouse *mouse = urb->context;
    signed char *data = mouse->data;
    struct input_dev *dev = mouse->dev;
    int status;

    switch (urb->status) {
    case 0:            /* success */
        break;
    case -ECONNRESET:    /* unlink */
    case -ENOENT:
    case -ESHUTDOWN:
        return;
    /* NOTE: -EOVERFLOW; Leetmouse addition */
    case -EOVERFLOW:
        printk("LEETMOUSE: EOVERFLOW. Try to increase BUFFER_SIZE from %d to %d in 'config.h'", BUFFER_SIZE, 2*BUFFER_SIZE);
        goto resubmit;
    /* -EPIPE:  should clear the halt */
    default:        /* error */
        goto resubmit;
    }

    input_report_key(dev, BTN_LEFT,   data[0] & 0x01);
    input_report_key(dev, BTN_RIGHT,  data[0] & 0x02);
    input_report_key(dev, BTN_MIDDLE, data[0] & 0x04);
    input_report_key(dev, BTN_SIDE,   data[0] & 0x08);
    input_report_key(dev, BTN_EXTRA,  data[0] & 0x10);
    /* NOTE: Leetmouse applying acceleration */
    signed int x = data[1];
    signed int y = data[2];
    signed int wheel = data[3];
    if (!accelerate(&x, &y, &wheel)) {
        input_report_rel(dev, REL_X,     x);
        input_report_rel(dev, REL_Y,     y);
        input_report_rel(dev, REL_WHEEL, wheel);
    }

    input_sync(dev);
resubmit:
    status = usb_submit_urb (urb, GFP_ATOMIC);
    if (status)
        dev_err(&mouse->usbdev->dev,
            "can't resubmit intr, %s-%s/input0, status %d\n",
            mouse->usbdev->bus->bus_name,
            mouse->usbdev->devpath, status);
}

static int usb_mouse_open(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);

    mouse->irq->dev = mouse->usbdev;
    if (usb_submit_urb(mouse->irq, GFP_KERNEL))
        return -EIO;

    return 0;
}

static void usb_mouse_close(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);

    usb_kill_urb(mouse->irq);
}

/* See: https://github.com/torvalds/linux/blob/c964ced7726294d40913f2127c3f185a92cb4a41/drivers/hid/usbhid/usbmouse.c#L106-L201 */
static int usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_device *dev = interface_to_usbdev(intf);
    struct usb_host_interface *interface;
    struct usb_endpoint_descriptor *endpoint;
    struct usb_mouse *mouse;
    struct input_dev *input_dev;
    struct urb *irq; // NOTE: This is now allocated separately and early
    int pipe, maxp;
    int error = -ENOMEM;

    // NOTE: Added check to prevent Keyboard issues
    if(strstr(dev->product, "Keyboard")) { // This caused issues with keyboard media buttons / volume roller
        printk("Probed product is a kb, not a mouse! (%s)\n", dev->product);
        return 1;
    }

    interface = intf->cur_altsetting;

    if (interface->desc.bNumEndpoints != 1)
        return -ENODEV;

    endpoint = &interface->endpoint[0].desc;
    if (!usb_endpoint_is_int_in(endpoint))
        return -ENODEV;

    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
    #if LINUX_VERSION_CODE < KERNEL_VERSION(5,19,0)
        maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
    #else
        maxp = usb_maxpacket(dev, pipe);
    #endif

    irq = usb_alloc_urb(0, GFP_KERNEL); // NOTE: The irq is now allocated earlier for `irq->transfer_dma`
    mouse = kzalloc(sizeof(struct usb_mouse), GFP_KERNEL);
    if (!mouse || !irq)
        goto fail1;

    input_dev = input_allocate_device();
    if (!input_dev)
        goto fail2;

    mouse->poll_interval = endpoint->bInterval;
    mouse->irq = irq;
    mouse->usbdev = dev;
    mouse->dev = input_dev;
    irq->dev = dev; // NOTE: We now set this early instead of just in `usb_mouse_open`

    if (dev->manufacturer)
        strscpy(mouse->name, dev->manufacturer, sizeof(mouse->name));

    if (dev->product) {
        if (dev->manufacturer)
            strlcat(mouse->name, " ", sizeof(mouse->name));
        strlcat(mouse->name, dev->product, sizeof(mouse->name));
    }

    if (!strlen(mouse->name))
        snprintf(mouse->name, sizeof(mouse->name),
             "USB HIDBP Mouse %04x:%04x",
             le16_to_cpu(dev->descriptor.idVendor),
             le16_to_cpu(dev->descriptor.idProduct));

    usb_make_path(dev, mouse->phys, sizeof(mouse->phys));
    strlcat(mouse->phys, "/input0", sizeof(mouse->phys));

    input_dev->name = mouse->name;
    input_dev->phys = mouse->phys;
    usb_to_input_id(dev, &input_dev->id);
    input_dev->dev.parent = &intf->dev;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
        BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
    input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] |= BIT_MASK(BTN_SIDE) |
        BIT_MASK(BTN_EXTRA);
    input_dev->relbit[0] |= BIT_MASK(REL_WHEEL);

    input_set_drvdata(input_dev, mouse);

    input_dev->open = usb_mouse_open;
    input_dev->close = usb_mouse_close;

    mouse->data = usb_alloc_coherent(dev, BUFFER_SIZE, GFP_KERNEL, &irq->transfer_dma);
    irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    usb_fill_int_urb(mouse->irq, dev, pipe, mouse->data,
             (maxp > BUFFER_SIZE ? BUFFER_SIZE : maxp),
             usb_mouse_irq, mouse, endpoint->bInterval);

    error = input_register_device(mouse->dev);
    if (!mouse->data || error)
        goto fail3;

    usb_set_intfdata(intf, mouse);
    return 0;

fail3:    
    usb_free_coherent(dev, BUFFER_SIZE, mouse->data, irq->transfer_dma);
fail2:
    input_free_device(input_dev);
fail1:
    usb_free_urb(mouse->irq);
    kfree(mouse);
    return error;
}

static void usb_mouse_disconnect(struct usb_interface *intf)
{
    struct usb_mouse *mouse = usb_get_intfdata (intf);

    usb_set_intfdata(intf, NULL);
    if (mouse) {
        usb_kill_urb(mouse->irq);
        input_unregister_device(mouse->dev);
        usb_free_urb(mouse->irq);
        usb_free_coherent(interface_to_usbdev(intf), BUFFER_SIZE, mouse->data, mouse->irq->transfer_dma);
        kfree(mouse);
    }
}

static const struct usb_device_id usb_mouse_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
        USB_INTERFACE_PROTOCOL_MOUSE) },
    { }    /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, usb_mouse_id_table);

static struct usb_driver usb_mouse_driver = {
    .name        = "leetmouse",                                 //Leetmouse Mod
    .probe        = usb_mouse_probe,
    .disconnect    = usb_mouse_disconnect,
    .id_table    = usb_mouse_id_table,
};

module_usb_driver(usb_mouse_driver);
