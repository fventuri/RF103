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

#include "rf103.h"
#include "logging.h"
#include "usb_device.h"
#include "clock_source.h"
#include "adc.h"
#include "tuner.h"

typedef struct rf103 rf103_t;

/* internal functions */
static uint8_t initial_gpio_register();


typedef struct rf103 {
  enum RF103Status status;
  enum RFMode rf_mode;
  usb_device_t *usb_device;
  clock_source_t *clock_source;
  adc_t *adc;
  int has_tuner;
  tuner_t *tuner;
  double sample_rate;
} rf103_t;


/******************************
 * basic functions
 ******************************/

int rf103_get_device_count()
{
  return usb_device_count_devices();
}


int rf103_get_device_info(struct rf103_device_info **rf103_device_infos)
{
  int ret_val = -1; 

  /* no more info to add from usb_device_get_device_list() for now */
  struct usb_device_info *list;
  int ret = usb_device_get_device_list(&list);
  if (ret < 0) {
    goto FAIL0;
  }

  int count = ret;
  struct rf103_device_info *device_infos = (struct rf103_device_info *) malloc((count + 1) * sizeof(struct rf103_device_info));
  /* use the first element to save the pointer to the underlying list,
     so we can use it to free it later on */
  *((void **) device_infos) = list;
  device_infos++;
  for (int i = 0; i < count; ++i) {
    device_infos[i].manufacturer = list[i].manufacturer;
    device_infos[i].product = list[i].product;
    device_infos[i].serial_number = list[i].serial_number;
  }

  *rf103_device_infos = device_infos;
  ret_val = count;

FAIL0:
  return ret_val;
}


int rf103_free_device_info(struct rf103_device_info *rf103_device_infos)
{
  /* just free our structure and call usb_device_free_device_list() to free
     underlying data structure */
  /* retrieve the underlying usb_device list pointer first */
  rf103_device_infos--;
  struct usb_device_info *list = (struct usb_device_info *) *((void **) rf103_device_infos);
  free(rf103_device_infos);
  int ret = usb_device_free_device_list(list);
  return ret;
}


rf103_t *rf103_open(int index, const char* imagefile)
{
  rf103_t *ret_val = 0;

  usb_device_t *usb_device = usb_device_open(index, imagefile,
                                             initial_gpio_register());
  if (usb_device == 0) {
    fprintf(stderr, "ERROR - usb_device_open() failed\n");
    goto FAIL0;
  }

  clock_source_t *clock_source = clock_source_open(usb_device);
  if (clock_source == 0) {
    fprintf(stderr, "ERROR - clock_source_open() failed\n");
    goto FAIL1;
  }

  rf103_t *this = (rf103_t *) malloc(sizeof(rf103_t));
  this->status = STATUS_READY;
  this->rf_mode = HF_MODE;
  this->usb_device = usb_device;
  this->clock_source = clock_source;
  this->adc = 0;
  this->has_tuner = has_tuner(usb_device);
  this->tuner = 0;
  this->sample_rate = 0;    /* default sample rate */

  ret_val = this;
  return ret_val;

FAIL1:
  usb_device_close(usb_device);
FAIL0:
  return ret_val;
}


void rf103_close(rf103_t *this)
{
  if (this->adc)
    adc_close(this->adc);
  if (this->tuner)
    tuner_close(this->tuner);
  clock_source_close(this->clock_source);
  usb_device_close(this->usb_device);
  free(this);
  return;
}


enum RF103Status rf103_status(rf103_t *this)
{
  return this->status;
}


int rf103_set_rf_mode(rf103_t *this, enum RFMode rf_mode)
{
  switch (rf_mode) {
    case HF_MODE:
      if (this->tuner)
        tuner_close(this->tuner);
      this->tuner = 0;
      break;
    case VHF_MODE:
      if (!this->has_tuner) {
        fprintf(stderr, "WARNING - no VHF/UHF tuner found\n");
        return -1;
      }
      this->tuner = tuner_open(this->usb_device);
      if (this->tuner == 0) {
        fprintf(stderr, "ERROR - tuner_open() failed\n");
        return -1;
      }
      break;
    default:
      fprintf(stderr, "WARNING - invalid RF mode: %d\n", rf_mode);
      return -1;
  }
  return 0;
}


/******************************
 * GPIO related functions
 ******************************/

enum GPIOBits {
  GPIO_LED_RED    = 0x01,    /* GPIO21 */
  GPIO_LED_YELLOW = 0x02,    /* GPIO22 */
  GPIO_LED_BLUE   = 0x04,    /* GPIO23 */
  GPIO_SEL0       = 0x08,    /* GPIO26 */
  GPIO_SEL1       = 0x10,    /* GPIO27 */
  GPIO_SHDWN      = 0x20,    /* GPIO28 */
  GPIO_DITHER     = 0x40,    /* GPIO29 */
  GPIO_RANDOM     = 0x80     /* GPIO20 */
};


static uint8_t initial_gpio_register()
{
  return GPIO_SEL1 | GPIO_LED_BLUE | GPIO_LED_YELLOW | GPIO_LED_RED;
}


int rf103_led_on(rf103_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(GPIO_LED_RED | GPIO_LED_YELLOW | GPIO_LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_on(this->usb_device, led_pattern);
}


int rf103_led_off(rf103_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(GPIO_LED_RED | GPIO_LED_YELLOW | GPIO_LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_off(this->usb_device, led_pattern);
}


int rf103_led_toggle(rf103_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(GPIO_LED_RED | GPIO_LED_YELLOW | GPIO_LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_toggle(this->usb_device, led_pattern);
}


int rf103_adc_dither(rf103_t *this, int dither)
{
  if (dither) {
    return usb_device_gpio_on(this->usb_device, GPIO_DITHER);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_DITHER);
  }
}


int rf103_adc_random(rf103_t *this, int random)
{
  if (random) {
    return usb_device_gpio_on(this->usb_device, GPIO_RANDOM);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_RANDOM);
  }
  adc_set_random(this->adc, random);
}


int rf103_hf_attenuation(rf103_t *this, double attenuation)
{
  uint8_t bit_pattern = 0;
  switch ((int) attenuation) {
    case 0:
      bit_pattern = GPIO_SEL1;
      break;
    case 10:
      bit_pattern = GPIO_SEL0 | GPIO_SEL1;
      break;
    case 20:
      bit_pattern = GPIO_SEL0;
      break;
    default:
      fprintf(stderr, "ERROR - invalid HF attenuation: %lf\n", attenuation);
      return -1;
  }
  return usb_device_gpio_set(this->usb_device, bit_pattern,
                             GPIO_SEL0 | GPIO_SEL1);
}


/******************************
 * streaming related functions
 ******************************/

int rf103_set_sample_rate(rf103_t *this, double sample_rate)
{
  /* no checks yet */
  this->sample_rate = sample_rate;
  return 0;
}


int rf103_set_async_params(rf103_t *this, uint32_t frame_size,
                           uint32_t num_frames, rf103_read_async_cb_t callback,
                           void *callback_context)
{
  if (this->adc) {
    fprintf(stderr, "ERROR - adc_open_async() failed: already opened\n");
    return -1;
  }

  this->adc = adc_open_async(this->usb_device, frame_size, num_frames,
                              callback, callback_context);
  if (this->adc == 0) {
    fprintf(stderr, "ERROR - adc_open_async() failed\n");
    return -1;
  }

  return 0;
}


int rf103_start_streaming(rf103_t *this)
{
  int ret = clock_source_set_clock(this->clock_source, ADC_CLOCK, this->sample_rate);
  if (ret < 0) {
    fprintf(stderr, "ERROR - clock_source_set_clock() failed\n");
    return -1;
  }
  ret = clock_source_start_clock(this->clock_source, ADC_CLOCK);
  if (ret < 0) {
    fprintf(stderr, "ERROR - clock_source_start_clock() failed\n");
    return -1;
  }
  if (this->rf_mode == VHF_MODE && this->tuner) {
    ret = clock_source_set_clock(this->clock_source, TUNER_CLOCK,
                                 tuner_get_xtal_frequency(this->tuner));
    if (ret < 0) {
      fprintf(stderr, "ERROR - clock_source_set_clock() failed\n");
      return -1;
    }
    ret = clock_source_start_clock(this->clock_source, TUNER_CLOCK);
    if (ret < 0) {
      fprintf(stderr, "ERROR - clock_source_start_clock() failed\n");
      return -1;
    }
    ret = tuner_start(this->tuner);
    if (ret < 0) {
      fprintf(stderr, "ERROR - tuner_start() failed\n");
      return -1;
    }
  }
  adc_set_sample_rate(this->adc, (uint32_t) this->sample_rate);
  ret = adc_start(this->adc);
  if (ret < 0) {
    fprintf(stderr, "ERROR - adc_start() failed\n");
    return -1;
  }
  ret = usb_device_control(this->usb_device, STARTFX3, 0, 0, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(STARTFX3) failed\n");
    return -1;
  }

  /* all good */
  return 0;
}

int rf103_handle_events(rf103_t *this)
{
  return usb_device_handle_events(this->usb_device);
}

int rf103_stop_streaming(rf103_t *this)
{
  int ret = usb_device_control(this->usb_device, STOPFX3, 0, 0, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(STOPFX3) failed\n");
    return -1;
  }
  ret = adc_stop(this->adc);
  if (ret < 0) {
    fprintf(stderr, "ERROR - adc_stop() failed\n");
    return -1;
  }
  ret = clock_source_stop_clock(this->clock_source, ADC_CLOCK);
  if (ret < 0) {
    fprintf(stderr, "ERROR - clock_source_stop_clock() failed\n");
    return -1;
  }

  return 0;
}


int rf103_reset_status(rf103_t *this)
{
  int ret = adc_reset_status(this->adc);
  if (ret < 0) {
    fprintf(stderr, "ERROR - adc_reset_status() failed\n");
    return -1;
  }
  return 0;
}


int rf103_read_sync(rf103_t *this, uint8_t *data, int length, int *transferred)
{
  return adc_read_sync(this->adc, data, length, transferred);
}
