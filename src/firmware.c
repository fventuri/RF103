/*
 * firmware.c - FX3 firmware functions
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

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libusb.h>

#include "firmware.h"
#include "error_handling.h"


static int validate_image(const uint8_t *image, const size_t size);
static int transfer_image(const uint8_t *image,
                          libusb_device_handle *dev_handle);

int load_image(struct libusb_device_handle *dev_handle, const char *imagefile)
{
  int ret_val = -1;

  int fd = open(imagefile, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "ERROR - open(%s) failed: %s\n", imagefile, strerror(errno));
    goto FAIL0;
  }

  /* slurp the whole fle into memory */
  struct stat statbuf;
  int ret = fstat(fd, &statbuf);
  if (ret < 0) {
    fprintf(stderr, "ERROR - fstat(%s) failed: %s\n", imagefile, strerror(errno));
    goto FAIL1;
  }
  size_t image_size = statbuf.st_size;
  uint8_t *image = (uint8_t *) malloc(image_size);
  if (image == 0) {
    fprintf(stderr, "ERROR - malloc() failed: %s\n", strerror(errno));
    goto FAIL1;
  }
  for (size_t nleft = image_size; nleft != 0; ) {
    ssize_t nr = read(fd, image, nleft);
    if (nr < 0) {
      fprintf(stderr, "ERROR - read(%s) failed: %s\n", imagefile, strerror(errno));
      goto FAIL1;
    }
    nleft -= nr;
  }

  close(fd);

  if (validate_image(image, image_size) < 0) {
    fprintf(stderr, "ERROR - validate_image() failed\n");
    goto FAILA;
  }

  if (transfer_image(image, dev_handle) < 0) {
    fprintf(stderr, "ERROR - transfer_image() failed\n");
    goto FAILA;
  }

  ret_val = 0;

FAILA:
  free(image);
  return ret_val;

FAIL1:
  close(fd);
FAIL0:
  return ret_val;
}

static int validate_image(const uint8_t *image, const size_t size)
{
  if (size < 10240) {
    fprintf(stderr, "ERROR - image file is too small\n");
    return -1;
  }
  if (!(image[0] == 'C' && image[1] == 'Y')) {
    fprintf(stderr, "ERROR - image header does not start with 'CY'\n");
    return -1;
  }
  if (!(image[2] == 0x1c)) {
    fprintf(stderr, "ERROR - I2C config is not set to 0x1C\n");
    return -1;
  }
  if (!(image[3] == 0xb0)) {
    fprintf(stderr, "ERROR - image type is not binary (0x01)\n");
    return -1;
  }

  uint32_t checksum = 0;
  uint32_t *current = (uint32_t *) image + 1;
  uint32_t *end = (uint32_t *) (image + size);

  while (1) {
    uint32_t loadSz = *current++;
    //printf("\tloadSz: %u\n", loadSz);
    if (loadSz == 0) {
      break;
    }
    uint32_t secStart __attribute__((unused)) = *current++;
    //printf("\tsecStart: 0x%08x\n", secStart);
    if (current + loadSz >= end - 2) {
      fprintf(stderr, "ERROR - loadSz is too big - loadSz=%u\n", loadSz);
      return -1;
    }
    while (loadSz--) {
      checksum += *current++;
    }
  }
  uint32_t entryAddr __attribute__((unused)) = *current++;
  //printf("entryAddr: 0x%08x\n", entryAddr);
  uint32_t expected_checksum = *current++;
  if (!(current == end)) {
    fprintf(stderr, "WARNING - image file longer than expected\n");
  }
  if (!(checksum == expected_checksum)) {
      fprintf(stderr, "ERROR - checksum does not match - actual=0x%08x expected=0x%08x\n",
              checksum, expected_checksum);
      return -1;
  }
  return 0;
}

static int transfer_image(const uint8_t *image,
                          libusb_device_handle *dev_handle)
{
  const uint8_t bmRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
  const uint8_t bRequest = 0xa0;            // vendor command
  const unsigned int timeout = 5000;        // timeout (in ms) for each command
  const size_t max_write_size = 2 * 1024;   // max write size in bytes
 
  // skip first word with 'CY' magic
  uint32_t *current = (uint32_t *) image + 1;

  while (1) {
    uint32_t loadSz = *current++;
    if (loadSz == 0) {
      break;
    }
    uint32_t address = *current++;

    unsigned char *data = (unsigned char *) current;
    for (size_t nleft = loadSz * 4; nleft > 0; ) {
      uint16_t wLength = nleft > max_write_size ? max_write_size : nleft;
      int ret = libusb_control_transfer(dev_handle, bmRequestType, bRequest,
                                        address & 0xffff, address >> 16,
                                        data, wLength, timeout);
      if (ret < 0) {
        usb_error(ret, __func__, __FILE__, __LINE__);
        return -1;
      }
      if (!(ret == wLength)) {
        fprintf(stderr, "ERROR - libusb_control_transfer() returned less bytes than expected - actual=%hu expected=%hu\n", ret, wLength);
        return -1;
      }
      data += wLength;
      nleft -= wLength;
    }
    current += loadSz;
  }

  uint32_t entryAddr = *current++;
  uint32_t checksum __attribute__((unused)) = *current++;

  sleep(1);

  int ret = libusb_control_transfer(dev_handle, bmRequestType, bRequest,
                                    entryAddr & 0xffff, entryAddr >> 16,
                                    0, 0, timeout);
  if (ret < 0) {
    usb_warning(ret, __func__, __FILE__, __LINE__);
  }

  return 0;
}
