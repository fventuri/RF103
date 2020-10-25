/*
 * tuner.h - R820T2 functions
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

#ifndef __TUNER_H
#define __TUNER_H

#include "usb_device.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct tuner tuner_t;


int has_tuner(usb_device_t *usb_device);

tuner_t *tuner_open(usb_device_t *usb_device);

void tuner_close(tuner_t *this);

uint32_t tuner_get_xtal_frequency(tuner_t *this);

int tuner_set_xtal_frequency(tuner_t *this, uint32_t xtal_frequency);

int tuner_start(tuner_t *this);

#ifdef __cplusplus
}
#endif

#endif /* __TUNER_H */
