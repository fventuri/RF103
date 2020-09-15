/*
 * rf103 - low level functions for wideband SDR receivers like
 *         BBRF103, RX-666, RX888, HF103, etc
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>

#include "rf103.h"
#include "error_handling.h"
#include "firmware.h"


struct rf103_usb_id {
  uint16_t vid;
  uint16_t pid;
  int needs_firmware;
};

static struct rf103_usb_id rf103_usb_ids[] = {
  { 0x04b4, 0x00f3, 1 },     /* Cypress / FX3 Boot-loader */
  { 0x04b4, 0x00f1, 0 }      /* Cypress / FX3 Streamer Example */
};
static int n_rf103_usb_ids = sizeof(rf103_usb_ids) / sizeof(rf103_usb_ids[0]);


int rf103_get_device_count()
{
  int ret_val = -1;

  int ret = libusb_init(0);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }
  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(0, &list);
  if (nusbdevices < 0) {
    usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }
  int count = 0;
  for (ssize_t i = 0; i < nusbdevices; ++i) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(dev, &desc);
    for (int i = 0; i < n_rf103_usb_ids; ++i) {
      if (desc.idVendor == rf103_usb_ids[i].vid &&
          desc.idProduct == rf103_usb_ids[i].pid) {
        count++;
      }
    }
  }
  libusb_free_device_list(list, 1);

  ret_val = count;

FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


int rf103_get_device_info(struct rf103_device_info **rf103_device_infos)
{
  const int MAX_STRING_BYTES = 256;

  int ret_val = -1;

  if (rf103_device_infos == 0) {
    error("argument rf103_device_infos is a null pointer", __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  int ret = libusb_init(0);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }
  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(0, &list);
  if (nusbdevices < 0) {
    usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }

  struct rf103_device_info *device_infos = (struct rf103_device_info *) malloc((nusbdevices + 1) * sizeof(struct rf103_device_info));
  int count = 0;
  for (ssize_t i = 0; i < nusbdevices; ++i) {
    libusb_device *device = list[i];
    struct libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(device, &desc);
    for (int i = 0; i < n_rf103_usb_ids; ++i) {
      if (!(desc.idVendor == rf103_usb_ids[i].vid &&
            desc.idProduct == rf103_usb_ids[i].pid)) {
        continue;
      }

      libusb_device_handle *dev_handle = 0;
      ret = libusb_open(device, &dev_handle);
      if (ret < 0) {
        usb_error(ret, __func__, __FILE__, __LINE__);
        goto FAIL2;
      }

      device_infos[count].manufacturer = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].manufacturer[0] = '\0';
      if (desc.iManufacturer) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iManufacturer,
                      device_infos[count].manufacturer, MAX_STRING_BYTES);
        if (ret < 0) {
          usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
      }

      device_infos[count].product = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].product[0] = '\0';
      if (desc.iProduct) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iProduct,
                      device_infos[count].product, MAX_STRING_BYTES);
        if (ret < 0) {
          usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
      }

      device_infos[count].serial_number = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].serial_number[0] = '\0';
      if (desc.iSerialNumber) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber,
                      device_infos[count].serial_number, MAX_STRING_BYTES);
        if (ret < 0) {
          usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
      }

      ret = 0;
FAIL3:
      libusb_close(dev_handle);
      if (ret < 0) {
        goto FAIL2;
      }
      count++;
    }
  }

  device_infos[count].manufacturer = 0;
  device_infos[count].product = 0;
  device_infos[count].serial_number = 0;

  *rf103_device_infos = device_infos;
  ret_val = count;

FAIL2:
  libusb_free_device_list(list, 1);
FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


int rf103_free_device_info(struct rf103_device_info *rf103_device_infos)
{
  for (struct rf103_device_info *rdi = rf103_device_infos;
       rdi->manufacturer || rdi->product || rdi->serial_number;
       ++rdi) {
    if (rdi->manufacturer) {
      free(rdi->manufacturer);
    }
    if (rdi->product) {
      free(rdi->product);
    }
    if (rdi->serial_number) {
      free(rdi->serial_number);
    }
  }
  free(rf103_device_infos);
  return 0;
}


static struct libusb_device_handle *find_rf103(int index, int *needs_firmware);

rf103_t *rf103_open(int index, const char* imagefile)
{
  rf103_t *ret_val = 0;

  int ret = libusb_init(0);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  int needs_firmware = 0;
  struct libusb_device_handle *dev_handle = find_rf103(index, &needs_firmware);
  if (dev_handle == 0) {
    goto FAIL1;
  }

  if (needs_firmware) {
    ret = load_image(dev_handle, imagefile);
    if (ret < 0) {
      error("load_image() failed", __func__, __FILE__, __LINE__);
      goto FAIL2;
    }

    /* rescan USB to get a new device handle */
    libusb_close(dev_handle);
    needs_firmware = 0;
    dev_handle = find_rf103(index, &needs_firmware);
    if (dev_handle == 0) {
      goto FAIL1;
    }
    if (needs_firmware) {
      error("device is still in boot loader mode", __func__, __FILE__, __LINE__);
      goto FAIL2;
    }
  }

  rf103_t *this = (rf103_t *) malloc(sizeof(rf103_t));
  this->dev_handle = dev_handle;

  ret_val = this;

FAIL2:
  libusb_close(dev_handle);
FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


static struct libusb_device_handle *find_rf103(int index, int *needs_firmware)
{
  struct libusb_device_handle *ret_val = 0;

  *needs_firmware = 0;

  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(0, &list);
  if (nusbdevices < 0) {
    usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  int count = 0;
  libusb_device *device = 0;
  for (ssize_t i = 0; i < nusbdevices; ++i) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);
    for (int i = 0; i < n_rf103_usb_ids; ++i) {
      if (desc.idVendor == rf103_usb_ids[i].vid &&
          desc.idProduct == rf103_usb_ids[i].pid) {
        if (count == index) {
          device = dev;
          *needs_firmware = rf103_usb_ids[i].needs_firmware;
        }
        count++;
      }
    }
  }

  if (device == 0) {
    fprintf(stderr, "ERROR - rf103@%d not found\n", index);
    goto FAIL1;
  }

  libusb_device_handle *dev_handle = 0;
  int ret = libusb_open(device, &dev_handle);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }
  libusb_free_device_list(list, 1);

  ret = libusb_kernel_driver_active(dev_handle, 0);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAILA;
  }
  if (ret == 1) {
    fprintf(stderr, "ERROR - device busy\n");
    goto FAILA;
  }

  ret = libusb_claim_interface(dev_handle, 0);
  if (ret < 0) {
    usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAILA;
  }

  ret_val = dev_handle;

FAIL1:
  libusb_free_device_list(list, 1);
FAIL0:
  return ret_val;

FAILA:
  libusb_close(dev_handle);
  return ret_val;
}


void rf103_close(rf103_t *this)
{
  libusb_close(this->dev_handle);
  free(this);
  libusb_exit(0);
  return;
}
