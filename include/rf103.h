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

#ifndef __RF103_H
#define __RF103_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rf103 {
  struct libusb_device_handle *dev_handle;
} rf103_t;

struct rf103_device_info {
  unsigned char *manufacturer;
  unsigned char *product;
  unsigned char *serial_number;
};


int rf103_get_device_count();

int rf103_get_device_info(struct rf103_device_info **rf103_device_infos);

int rf103_free_device_info(struct rf103_device_info *rf103_device_infos);

rf103_t *rf103_open(int index, const char* imagefile);

void rf103_close(rf103_t *this);

#ifdef __cplusplus
}
#endif

#endif /* __RF103_H */
