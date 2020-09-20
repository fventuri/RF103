/*
 * clock_source.c - Si5351 functions
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

/* References:
 *  - Si5351 datasheet (https://www.silabs.com/documents/public/data-sheets/Si5351-B.pdf)
 *  - SiLabs Application Note AN619 - Manually Generating an Si5351 Register Map (https://www.silabs.com/documents/public/application-notes/AN619.pdf)
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "clock_source.h"
#include "usb_device.h"
#include "logging.h"


typedef struct clock_source clock_source_t;

/* internal functions */
static int power_down_clocks(clock_source_t *this);
static void rational_approximation(double value, uint32_t max_denominator,
                                   uint32_t *a, uint32_t *b, uint32_t *c);
static int configure_clock_input_and_pll(clock_source_t *this, int index,
                                         uint32_t a, uint32_t b, uint32_t c);
static int configure_clock_output(clock_source_t *this, int index,
                                  uint32_t output_ms, uint8_t rdiv);
static int reset_pll_and_clock_power_up(clock_source_t *this, int index);


typedef struct clock_source {
  usb_device_t *usb_device;
  double crystal_frequency;
  double frequency_correction;
} clock_source_t;


static const uint8_t SI5351_ADDR = 0x60 << 1;
static const double SI5351_FREQ = 27e6;
static const double SI5351_FREQ_CORR = 0.9999314;
static const double SI5351_MAX_VCO_FREQ = 900e6;
static const uint32_t SI5351_MAX_DENOMINATOR = 1048575;

enum SI5351Registers {
  SI5351_REGISTER_PLL_SOURCE   = 15,
  SI5351_REGISTER_CLK_BASE     = 16,
  SI5351_REGISTER_MSNA_BASE    = 26,
  SI5351_REGISTER_MSNB_BASE    = 34,
  SI5351_REGISTER_MS0_BASE     = 42,
  SI5351_REGISTER_MS1_BASE     = 50,
  SI5351_REGISTER_PLL_RESET    = 177,
  SI5351_REGISTER_CRYSTAL_LOAD = 183
};

enum SI5351CrystalLoadValues {
  SI5351_VALUE_CLK_PDN          = 0x80,
  SI5351_VALUE_CRYSTAL_LOAD_6PF = 0x01 << 6 | 0x12,
  SI5351_VALUE_PLLA_RESET       = 0x20,
  SI5351_VALUE_PLLB_RESET       = 0x80,
  SI5351_VALUE_MS_INT           = 0x40,
  SI5351_VALUE_CLK_SRC_MS       = 0x0c,
  SI5351_VALUE_CLK_DRV_8MA      = 0x03,
  SI5351_VALUE_MS_SRC_PLLA      = 0x00,
  SI5351_VALUE_MS_SRC_PLLB      = 0x20
};


clock_source_t *clock_source_open(usb_device_t *usb_device)
{
  clock_source_t *ret_val = 0;

  /* set crystal load capacitance */
  int ret = usb_device_i2c_write_byte(usb_device, SI5351_ADDR,
                                      SI5351_REGISTER_CRYSTAL_LOAD,
                                      SI5351_VALUE_CRYSTAL_LOAD_6PF);
  if (ret < 0) {
    log_error("usb_device_i2c_write_byte() failed", __func__, __FILE__, __LINE__);
    return ret_val;
  }

  /* we are good here - create and initialize the clock_source */
  clock_source_t *this = (clock_source_t *) malloc(sizeof(clock_source_t));
  this->usb_device = usb_device;
  this->crystal_frequency = SI5351_FREQ;
  this->frequency_correction = SI5351_FREQ_CORR;

  /* power down all the clocks to save power */
  ret = power_down_clocks(this);
  if (ret < 0) {
    log_error("power_down_clocks() failed", __func__, __FILE__, __LINE__);
    free(this);
    return ret_val;
  }

  ret_val = this;
  return ret_val;
}


void clock_source_close(clock_source_t *this)
{
  int ret = power_down_clocks(this);
  if (ret < 0) {
    log_error("power_down_clocks() failed", __func__, __FILE__, __LINE__);
  }
  free(this);
  return;
}


void clock_source_set_crystal_frequency(clock_source_t *this,
                                        double crystal_frequency)
{
  this->crystal_frequency = crystal_frequency;
}


void clock_source_set_frequency_correction(clock_source_t *this,
                                           double frequency_correction)
{
  this->frequency_correction = frequency_correction;
}


int clock_source_set_clock(clock_source_t *this, int index, double frequency)
{
  if (!(index == 0 || index == 1)) {
    fprintf(stderr, "ERROR - invalid clock index: %d\n", index);
    return -1;
  }

  /* if the requested frequency is below 1MHz, use an R divider */
  double r_frequency = frequency;
  uint8_t rdiv = 0;
  while (r_frequency < 1e6 && rdiv <= 7) {
    r_frequency *= 2.0;
    rdiv += 1;
  }
  if (r_frequency < 1e6) {
    fprintf(stderr, "ERROR - requested frequency is too low: %lg\n", frequency);
    return -1;
  }

  /* choose an even integer for the output MS */
  uint32_t output_ms = ((uint32_t) (SI5351_MAX_VCO_FREQ / r_frequency));
  output_ms &= ~0x01;
  double vco_frequency = r_frequency * output_ms;
  if (output_ms < 4 || output_ms > 2048) {
    fprintf(stderr, "ERROR - invalid output MS: %d  (frequency=%lg)\n",
            output_ms, frequency);
    return -1;
  }

  /* feedback MS */
  double feedback_ms = vco_frequency / (this->crystal_frequency / this->frequency_correction);
  /* find a good rational approximation for feedback_ms */
  uint32_t a;
  uint32_t b;
  uint32_t c;
  rational_approximation(feedback_ms, SI5351_MAX_DENOMINATOR, &a, &b, &c);

  int ret = configure_clock_input_and_pll(this, index, a, b, c);
  if (ret < 0) {
    fprintf(stderr, "ERROR - configure_clock_input_and_pll() failed\n");
    return -1;
  }

  ret = configure_clock_output(this, index, output_ms, rdiv);
  if (ret < 0) {
    fprintf(stderr, "ERROR - configure_clock_output() failed\n");
    return -1;
  }

  /* finally reset the PLL and power up the clock */
  ret = reset_pll_and_clock_power_up(this, index);
  if (ret < 0) {
    fprintf(stderr, "ERROR - reset_pll_and_clock_power_up() failed\n");
    return -1;
  }

  return 0;
}


/* internal functions */

static int power_down_clocks(clock_source_t *this)
{
  uint8_t data[] = {
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN,
    SI5351_VALUE_CLK_PDN
  };
  int ret = usb_device_i2c_write(this->usb_device, SI5351_ADDR,
                                 SI5351_REGISTER_CLK_BASE,
                                 data, sizeof(data));
  if (ret < 0) {
    log_error("usb_device_i2c_write() failed", __func__, __FILE__, __LINE__);
    return -1;
  }
  return 0;
}


/* best rational approximation:
 *
 *     value ~= a + b/c     (where b <= max_denominator)
 *
 * References:
 * - https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations
 */
static void rational_approximation(double value, uint32_t max_denominator,
                                   uint32_t *a, uint32_t *b, uint32_t *c)
{
  const double epsilon = 1e-5;

  double af;
  double f0 = modf(value, &af);
  *a = (uint32_t) af;
  *b = 0;
  *c = 1;
  double f = f0;
  double delta = f0;
  /* we need to take into account that the fractional part has a_0 = 0 */
  uint32_t h[] = {1, 0};
  uint32_t k[] = {0, 1};
  for (int i = 0; i < 100; ++i) {
    if (f <= epsilon) {
      break;
    }
    double anf;
    f = modf(1.0 / f, &anf);
    uint32_t an = (uint32_t) anf;
    for (uint32_t m = (an + 1) / 2; m <= an; ++m) {
      uint32_t hm = m * h[1] + h[0];
      uint32_t km = m * k[1] + k[0];
      if (km > max_denominator) {
        break;
      }
      double d = fabs((double) hm / (double) km - f0);
      if (d < delta) {
        delta = d;
        *b = hm;
        *c = km;
      }
    }
    uint32_t hn = an * h[1] + h[0];
    uint32_t kn = an * k[1] + k[0];
    h[0] = h[1]; h[1] = hn;
    k[0] = k[1]; k[1] = kn;
  }
  return;
}


/* stage 1 - configuring input and PLL register parameters (AN619 Ch 3) */
static int configure_clock_input_and_pll(clock_source_t *this, int index,
                                         uint32_t a, uint32_t b, uint32_t c)
{
  uint32_t b_over_c = 128 * b / c;
  uint32_t msn_p1 = 128 * a + b_over_c - 512;
  uint32_t msn_p2 = 128 * b  - c * b_over_c;
  uint32_t msn_p3 = c;

  uint8_t data[] = {
    (msn_p3 & 0x0000ff00) >>  8,
    (msn_p3 & 0x000000ff) >>  0,
    (msn_p1 & 0x00030000) >> 16,
    (msn_p1 & 0x0000ff00) >>  8,
    (msn_p1 & 0x000000ff) >>  0,
    (msn_p3 & 0x000f0000) >> 12 | (msn_p2 & 0x000f0000) >> 16,
    (msn_p2 & 0x0000ff00) >>  8,
    (msn_p2 & 0x000000ff) >>  0
  };

  uint8_t msn_register;
  if (index == 0) {
    msn_register = SI5351_REGISTER_MSNA_BASE;
  } else if (index == 1) {
    msn_register = SI5351_REGISTER_MSNB_BASE;
  }
  int ret = usb_device_i2c_write(this->usb_device, SI5351_ADDR, msn_register,
                                 data, sizeof(data));
  if (ret < 0) {
    log_error("usb_device_i2c_write() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  return 0;
}


/* stage 2 - configuring the output register parameters (AN619 Ch 4) */
static int configure_clock_output(clock_source_t *this, int index,
                                  uint32_t output_ms, uint8_t rdiv)
{
  /* since the output divider is an even integer a = output_ms, b = 0, c = 1 */
  uint32_t ms_p1 = 128 * output_ms - 512;
  uint32_t ms_p2 = 0;
  uint32_t ms_p3 = 1;

  uint8_t data[] = {
    (ms_p3 & 0x0000ff00) >>  8,
    (ms_p3 & 0x000000ff) >>  0,
    rdiv << 5 | (ms_p1 & 0x00030000) >> 16,
    (ms_p1 & 0x0000ff00) >>  8,
    (ms_p1 & 0x000000ff) >>  0,
    (ms_p3 & 0x000f0000) >> 12 | (ms_p2 & 0x000f0000) >> 16,
    (ms_p2 & 0x0000ff00) >>  8,
    (ms_p2 & 0x000000ff) >>  0
  };

  uint8_t ms_register;
  if (index == 0) {
    ms_register = SI5351_REGISTER_MS0_BASE;
  } else if (index == 1) {
    ms_register = SI5351_REGISTER_MS1_BASE;
  }
  int ret = usb_device_i2c_write(this->usb_device, SI5351_ADDR, ms_register,
                                 data, sizeof(data));
  if (ret < 0) {
    log_error("usb_device_i2c_write() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  return 0;
}


static int reset_pll_and_clock_power_up(clock_source_t *this, int index)
{
  uint8_t pll_reset;
  if (index == 0) {
    pll_reset = SI5351_VALUE_PLLA_RESET;
  } else if (index == 1) {
    pll_reset = SI5351_VALUE_PLLB_RESET;
  }
  int ret = usb_device_i2c_write_byte(this->usb_device, SI5351_ADDR,
                                      SI5351_REGISTER_PLL_RESET, pll_reset);
  if (ret < 0) {
    log_error("usb_device_i2c_write_byte() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  uint8_t clock_control = SI5351_VALUE_MS_INT | SI5351_VALUE_CLK_SRC_MS | SI5351_VALUE_CLK_DRV_8MA;
  if (index == 0) {
    clock_control |= SI5351_VALUE_MS_SRC_PLLA;
  } else if (index == 1) {
    clock_control |= SI5351_VALUE_MS_SRC_PLLB;
  }
  ret = usb_device_i2c_write_byte(this->usb_device, SI5351_ADDR,
                                  SI5351_REGISTER_CLK_BASE + index,
                                  clock_control);
  if (ret < 0) {
    log_error("usb_device_i2c_write_byte() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  return 0;
}
