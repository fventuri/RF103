/*
 * usb_device.h - FX3 functions
 *
 * Copyright (C) 2020 by Franco Venturi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include <libusb.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_device {
  struct libusb_device_handle *dev_handle;
} usb_device_t;

struct usb_device_info {
  unsigned char *manufacturer;
  unsigned char *product;
  unsigned char *serial_number;
};

int usb_device_count_devices();

int usb_device_get_device_list(struct usb_device_info **usb_device_infos);

int usb_device_free_device_list(struct usb_device_info *usb_device_infos);

usb_device_t *usb_device_open(int index, const char* imagefile);

void usb_device_close(usb_device_t *this);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_H */
