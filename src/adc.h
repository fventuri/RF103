/*
 * adc.h - ADC related functions (including bulk data transfer)
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

#ifndef __ADC_H
#define __ADC_H

#include "usb_device.h"
#include "rf103.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct adc adc_t;

adc_t *adc_open_sync(usb_device_t *usb_device);

adc_t *adc_open_async(usb_device_t *usb_device, uint32_t frame_size,
                      uint32_t num_frames, rf103_read_async_cb_t callback,
                      void *callback_context);

void adc_close(adc_t *this);

int adc_set_sample_rate(adc_t *this, uint32_t sample_rate);

int adc_start(adc_t *this);

int adc_stop(adc_t *this);

int adc_reset_status(adc_t *this);

int adc_read_sync(adc_t *this, uint8_t *data, int length, int *transferred);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H */
