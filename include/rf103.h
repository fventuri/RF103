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

#include <stdint.h>

typedef struct rf103 rf103_t;

struct rf103_device_info {
  unsigned char *manufacturer;
  unsigned char *product;
  unsigned char *serial_number;
};

enum RF103Status {
  STATUS_OFF,
  STATUS_READY,
  STATUS_STREAMING,
  STATUS_FAILED = 0xff
};

enum LEDColors {
  LED_RED    = 0x01,
  LED_YELLOW = 0x02,
  LED_BLUE   = 0x04
};


/* basic functions */
int rf103_get_device_count();

int rf103_get_device_info(struct rf103_device_info **rf103_device_infos);

int rf103_free_device_info(struct rf103_device_info *rf103_device_infos);

rf103_t *rf103_open(int index, const char* imagefile);

void rf103_close(rf103_t *this);

enum RF103Status rf103_status(rf103_t *this);


/* GPIO related functions */
int rf103_led_on(rf103_t *this, uint8_t led_pattern);

int rf103_led_off(rf103_t *this, uint8_t led_pattern);

int rf103_led_toggle(rf103_t *this, uint8_t led_pattern);

int rf103_adc_dither(rf103_t *this, int dither);

int rf103_adc_random(rf103_t *this, int dither);


/* ADC/bulk transfer related functions */
typedef void (*rf103_read_async_cb_t)(uint32_t data_size, uint8_t *data,
                                      void *context);


#ifdef __cplusplus
}
#endif

#endif /* __RF103_H */
