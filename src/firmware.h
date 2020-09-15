/*
 * firmware.h - FX3 firmware functions
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

#ifndef __FIRMWARE_H
#define __FIRMWARE_H

#include <libusb.h>


#ifdef __cplusplus
extern "C" {
#endif

int load_image(struct libusb_device_handle *dev_handle, const char *imagefile);

#ifdef __cplusplus
}
#endif

#endif /* __FIRMWARE_H */
