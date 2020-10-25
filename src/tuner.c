/*
 * tuner.c - R820T2 functions
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
 *  - Rafael Micro datasheets:
 *    - R820T High Performance Low Power Advanced Digital TV Silicon Tuner Datasheet (https://www.rtl-sdr.com/wp-content/uploads/2013/04/R820T_datasheet-Non_R-20111130_unlocked1.pdf)
 *    - R820T2 Register Description (https://www.rtl-sdr.com/wp-content/uploads/2016/12/R820T2_Register_Description.pdf)
 *
 *  - Detailed register description:
 *    - https://github.com/old-dab/rtlsdr/blob/master/src/tuner_r82xx.c
 *    - https://github.com/erlendse/R820T2-description.git
 *
 *  - Other useful resources:
 *    - https://osmocom.org/projects/rtl-sdr/wiki
 *    - https://github.com/librtlsdr/librtlsdr/blob/master/src/tuner_r82xx.c
 *    - https://github.com/torvalds/linux/blob/master/drivers/media/tuners/r820t.c
 *    - https://github.com/ik1xpv/ExtIO_sddc-Ver0.96/blob/master/Source/R820T2.cpp
 */

#define TUNER_PARAMS_BBRF103 0
#define TUNER_PARAMS_LIBRTLSDR 1
#define TUNER_PARAMS TUNER_PARAMS_BBRF103
/* #define TUNER_PARAMS TUNER_PARAMS_LIBRTLSDR */
#define TUNER_BOUNDARY_SPUR_PREVENTION


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

#include "tuner.h"
#include "logging.h"


typedef struct tuner tuner_t;

/* internal functions */
struct tuner_pll_parameters;
struct tuner_mux_parameters;

static int tuner_init_registers(tuner_t *this);
static int tuner_set_pll(tuner_t *this, double frequency) __attribute__((unused));
static int tuner_compute_pll_parameters(tuner_t *this, double frequency,
                                        struct tuner_pll_parameters *pll_params);
static int tuner_apply_pll_parameters(tuner_t *this,
                                      const struct tuner_pll_parameters *pll_params);
static int tuner_set_mux(tuner_t *this, double frequency) __attribute__((unused));
static int tuner_compute_mux_parameters(tuner_t *this,
                                        double frequency,
                                        struct tuner_mux_parameters *mux_params);
static int tuner_apply_mux_parameters(tuner_t *this,
                                      const struct tuner_mux_parameters *mux_params);

static int tuner_read_value(tuner_t *this, const uint8_t where[3],
                            uint8_t *value);
static int tuner_read_registers(tuner_t *this, uint32_t register_mask);
static int tuner_write_value(tuner_t *this, const uint8_t where[3],
                             uint8_t value);
static int tuner_write_registers(tuner_t *this, uint32_t register_mask);
static uint8_t tuner_get_value(tuner_t *this, const uint8_t where[3]) __attribute__((unused));
static void tuner_set_value(tuner_t *this, const uint8_t where[3],
                            uint8_t value);


enum { R820T2_REGISTERS = 32 };

typedef struct tuner {
  usb_device_t *usb_device;
  uint32_t xtal_frequency;
  uint8_t registers[R820T2_REGISTERS];
  uint32_t registers_dirty_mask;
} tuner_t;


static const uint32_t DEFAULT_TUNER_XTAL_FREQUENCY = 32000000;

static const uint8_t R820T2_ADDR = 0x1a;
static const uint8_t R820T2_ADDR_READ  = R820T2_ADDR << 1 | 0x00;
static const uint8_t R820T2_ADDR_WRITE = R820T2_ADDR << 1 | 0x01;
static const uint32_t R820T2_REGISTERS_READ_MASK  = 0xffffffff;
static const uint32_t R820T2_REGISTERS_WRITE_MASK = 0xfffffff0;

enum R820T2Registers {
  R820T2_REGISTER_SOMETHING    = 0
};

/* R820T2 register matrix */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
static const uint8_t R820T2_VCO_INDICATOR[] = { 0x02, 0x7f, 0 };
static const uint8_t R820T2_RF_INDICATOR[]  = { 0x03, 0xff, 0 };
static const uint8_t R820T2_PWD_LT[]        = { 0x05, 0x80, 7 };
static const uint8_t R820T2_PWD_LNA1[]      = { 0x05, 0x20, 5 };
static const uint8_t R820T2_LNA_GAIN_MODE[] = { 0x05, 0x10, 4 };
static const uint8_t R820T2_LNA_GAIN[]      = { 0x05, 0x0f, 0 };
static const uint8_t R820T2_PWD_PDET1[]     = { 0x06, 0x80, 7 };
static const uint8_t R820T2_PWD_PDET3[]     = { 0x06, 0x40, 6 };
static const uint8_t R820T2_FILT_3DB[]      = { 0x06, 0x20, 5 };
static const uint8_t R820T2_PW_LNA[]        = { 0x06, 0x07, 0 };
static const uint8_t R820T2_PWD_MIX[]       = { 0x07, 0x40, 6 };
static const uint8_t R820T2_PW0_MIX[]       = { 0x07, 0x20, 5 };
static const uint8_t R820T2_MIXGAIN_MODE[]  = { 0x07, 0x10, 4 };
static const uint8_t R820T2_MIX_GAIN[]      = { 0x07, 0x0f, 0 };
static const uint8_t R820T2_PWD_AMP[]       = { 0x08, 0x80, 7 };
static const uint8_t R820T2_PW0_AMP[]       = { 0x08, 0x40, 6 };
static const uint8_t R820T2_IMR_G[]         = { 0x08, 0x3f, 0 };
static const uint8_t R820T2_PWD_IFFILT[]    = { 0x09, 0x80, 7 };
static const uint8_t R820T2_PW1_IFFILT[]    = { 0x09, 0x40, 6 };
static const uint8_t R820T2_IMR_P[]         = { 0x09, 0x3f, 0 };
static const uint8_t R820T2_PWD_FILT[]      = { 0x0a, 0x80, 7 };
static const uint8_t R820T2_PW_FILT[]       = { 0x0a, 0x60, 5 };
static const uint8_t R820T2_FILT_CODE[]     = { 0x0a, 0x0f, 0 };
static const uint8_t R820T2_FILT_BW[]       = { 0x0b, 0x60, 5 };
static const uint8_t R820T2_HPF[]           = { 0x0b, 0x0f, 0 };
static const uint8_t R820T2_PWD_VGA[]       = { 0x0c, 0x40, 6 };
static const uint8_t R820T2_VGA_MODE[]      = { 0x0c, 0x10, 4 };
static const uint8_t R820T2_VGA_CODE[]      = { 0x0c, 0x0f, 0 };
static const uint8_t R820T2_LNA_VTHH[]      = { 0x0d, 0xf0, 4 };
static const uint8_t R820T2_LNA_VTHL[]      = { 0x0d, 0x0f, 0 };
static const uint8_t R820T2_MIX_VTH_H[]     = { 0x0e, 0xf0, 4 };
static const uint8_t R820T2_MIX_VTH_L[]     = { 0x0e, 0x0f, 0 };
static const uint8_t R820T2_CLK_OUT_ENB[]   = { 0x0f, 0x10, 4 };
static const uint8_t R820T2_CLK_AGC_ENB[]   = { 0x0f, 0x02, 1 };
static const uint8_t R820T2_SEL_DIV[]       = { 0x10, 0xe0, 5 };
static const uint8_t R820T2_REFDIV[]        = { 0x10, 0x10, 4 };
static const uint8_t R820T2_XTAL_DRIVE[]    = { 0x10, 0x08, 3 };
static const uint8_t R820T2_CAPX[]          = { 0x10, 0x03, 0 };
static const uint8_t R820T2_PW_LDO_A[]      = { 0x11, 0xc0, 6 };
static const uint8_t R820T2_VCO_CURRENT[]   = { 0x12, 0xe0, 5 };
static const uint8_t R820T2_PW_SDM[]        = { 0x12, 0x08, 3 };
static const uint8_t R820T2_SI2C[]          = { 0x14, 0xc0, 6 };
static const uint8_t R820T2_NI2C[]          = { 0x14, 0x3f, 0 };
static const uint8_t R820T2_SDM_INL[]       = { 0x15, 0xff, 0 };
static const uint8_t R820T2_SDM_INH[]       = { 0x16, 0xff, 0 };
static const uint8_t R820T2_PW_LDO_D[]      = { 0x17, 0xc0, 6 };
static const uint8_t R820T2_OPEN_D[]        = { 0x17, 0x08, 3 };
static const uint8_t R820T2_PWD_RFFILT[]    = { 0x19, 0x80, 7 };
static const uint8_t R820T2_SW_AGC[]        = { 0x19, 0x10, 4 };
static const uint8_t R820T2_RFMUX[]         = { 0x1a, 0xc0, 6 };
static const uint8_t R820T2_PLL_AUTO_CLK[]  = { 0x1a, 0x0c, 2 };
static const uint8_t R820T2_RFFILT[]        = { 0x1a, 0x03, 0 };
static const uint8_t R820T2_TF_NCH[]        = { 0x1b, 0xf0, 4 };
static const uint8_t R820T2_TF_LP[]         = { 0x1b, 0x0f, 0 };
static const uint8_t R820T2_PDET3_GAIN[]    = { 0x1c, 0xf0, 4 };
static const uint8_t R820T2_PDET1_GAIN[]    = { 0x1d, 0x38, 3 };
static const uint8_t R820T2_PDET2_GAIN[]    = { 0x1d, 0x07, 0 };
static const uint8_t R820T2_PDET_CLK[]      = { 0x1e, 0x3f, 0 };
static const uint8_t R820T2_FILT_EXT[]      = { 0x1e, 0x40, 6 };
#pragma GCC diagnostic pop


int has_tuner(usb_device_t *usb_device)
{
  uint8_t data[4];
  int ret = usb_device_control(usb_device, TESTFX3, 0, 0, data, sizeof(data));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(TESTFX3) failed\n");
    /* return 0 (instead of -1) since an error probably means no tuner */
    return 0;
  }
  return data[0] == 0;
}


tuner_t *tuner_open(usb_device_t *usb_device)
{
  tuner_t *ret_val = 0;

  /* create the tuner and initialize the registers */
  tuner_t *this = (tuner_t *) malloc(sizeof(tuner_t));
  this->usb_device = usb_device;
  this->xtal_frequency = DEFAULT_TUNER_XTAL_FREQUENCY;
  memset(this->registers, 0, sizeof(this->registers));
  this->registers_dirty_mask = 0;

  int ret = tuner_init_registers(this);
  if (ret < 0) {
    log_error("tuner_init_registers() failed", __func__, __FILE__, __LINE__);
    free(this);
    return ret_val;
  }
  ret = tuner_read_registers(this, 0xffffffff);
  if (ret < 0) {
    log_error("tuner_read_registers() failed", __func__, __FILE__, __LINE__);
    free(this);
    return ret_val;
  }


  ret_val = this;
  return ret_val;
}


void tuner_close(tuner_t *this)
{
  free(this);
  return;
}


uint32_t tuner_get_xtal_frequency(tuner_t *this)
{
  return this->xtal_frequency;
}


int tuner_set_xtal_frequency(tuner_t *this, uint32_t xtal_frequency)
{
  /* no checks yet */
  this->xtal_frequency = xtal_frequency;
  return 0;
}


int tuner_start(tuner_t *this)
{
  /* TODO */
printf("this: %p\n", (void *) this);
  return 0;
}


/* internal functions */

static int tuner_init_registers(tuner_t *this)
{
  static const uint8_t init_registers[R820T2_REGISTERS] = {
    0x00, 0x00, 0x00, 0x00, 0x00,            /* 00 to 04 */
#if TUNER_PARAMS == TUNER_PARAMS_BBRF103
    /* 05 */ 0x90, /* 0x90 LNA manual gain mode, init to 0 */
    /* 06 */ 0x80,
    /* 07 */ 0x60,
    /* 08 */ 0x80, /* Image Gain Adjustment */
    /* 09 */ 0x40, /*  40 Image Phase Adjustment */
    /* 0a */ 0xa0, /*  A8 Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch! */
    /* 0b */ 0x6f, /*  0F High pass filter - Optimal. Don't touch! */
    /* 0c */ 0x40, /* 0x480x40 VGA control by code, init at 0 */
    /* 0d */ 0x63, /* LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold */
    /* 0e */ 0x75,
    /* 0f */ 0xf8, /* f8 Filter Widest, LDO_5V OFF, clk out OFF */
    /* 10 */ 0x7c,
    /* 11 */ 0x83,
    /* 12 */ 0x80,
    /* 13 */ 0x00,
    /* 14 */ 0x0f,
    /* 15 */ 0x00,
    /* 16 */ 0xc0,
    /* 17 */ 0x30,
    /* 18 */ 0x48,
    /* 19 */ 0xcc, /* 0xcc */
    /* 1a */ 0x62, /* 0x60 */
    /* 1b */ 0x00,
    /* 1c */ 0x54,
    /* 1d */ 0xae,
    /* 1e */ 0x0a,
    /* 1f */ 0xc0
#elif TUNER_PARAMS == TUNER_PARAMS_LIBRTLSDR
    /* 05 */ 0x80,
    /* 06 */ 0x13,
    /* 07 */ 0x70,
    /* 08 */ 0xc0,
    /* 09 */ 0x40,
    /* 0a */ 0xdb,
    /* 0b */ 0x6b,
    /* Reg 0x0c:
     * for manual gain was: set fixed VGA gain for now (16.3 dB): 0x08
     * with active agc was: set fixed VGA gain for now (26.5 dB): 0x0b */
    /* 0c */ 0xeb,
    /* 0d */ 0x53,
    /* 0e */ 0x75,
    /* 0f */ 0x68,
    /* 10 */ 0x6c,
    /* 11 */ 0xbb,
    /* 12 */ 0x80,
    /* 13 */ 0x31,
    /* 14 */ 0x0f,
    /* 15 */ 0x00,
    /* 16 */ 0xc0,
    /* 17 */ 0x30,
    /* 18 */ 0x48,
    /* 19 */ 0xec,
    /* 1a */ 0x60,
    /* 1b */ 0x00,
    /* 1c */ 0x24,
    /* 1d */ 0xdd,
    /* 1e */ 0x0e,
    /* 1f */ 0x40
#endif /* TUNER_PARAMS */
  };

  memcpy(this->registers, init_registers, sizeof(this->registers));
  int ret = tuner_write_registers(this, R820T2_REGISTERS_WRITE_MASK);
  if (ret < 0) {
    log_error("tuner_write_registers() failed", __func__, __FILE__, __LINE__);
    return -1;
  }
  return 0;
}


struct tuner_pll_parameters {
  uint8_t refdiv;       /* PLL Reference frequency Divider (always 0) */
  uint8_t sel_div;      /* PLL to Mixer divider number control (0-5 for 2-64) */
  uint8_t ni2c;         /* PLL integer divider number input Ni2c (0-31) */
  uint8_t si2c;         /* PLL integer divider number input Si2c (0-3) */
  /* Sigma-Delta Modulator */
  uint8_t pw_sdm;       /* 0: Enable frac pll, 1: Disable frac pll */
  uint16_t sdm;         /* PLL fractional divider number input (0-65535) */
};


static int tuner_set_pll(tuner_t *this, double frequency)
{
  struct tuner_pll_parameters pll_params;
  int ret = tuner_compute_pll_parameters(this, frequency, &pll_params);
  if (ret < 0) {
    fprintf(stderr, "ERROR - tuner_compute_pll_parameters() failed\n");
    return -1;
  }

  ret = tuner_apply_pll_parameters(this, &pll_params);
  if (ret < 0) {
    fprintf(stderr, "ERROR - tuner_apply_pll_parameters() failed\n");
    return -1;
  }
  return 0;
}


static int tuner_compute_pll_parameters(tuner_t *this, double frequency,
                                        struct tuner_pll_parameters *pll_params)
{
  /* useful constants */
  const double MIN_VCO_FREQUENCY = 1.77e9;   /* min VCO frequency 1.77GHz */
  const uint8_t MAX_SEL_DIV = 5;             /* max value for SEL_DIV */
  const double MIN_MULTIPLIER = 13;
  const double MAX_MULTIPLIER = MIN_MULTIPLIER + 128;
  const uint32_t SDM_FRAC_PRECISION = 65536;


  memset(pll_params, 0, sizeof(*pll_params));

#if TUNER_PARAMS == TUNER_PARAMS_BBRF103
  /* BBRF103 sets refdiv=1 */
  pll_params->refdiv = 1;
#elif TUNER_PARAMS == TUNER_PARAMS_LIBRTLSDR
  /* RTL-SDR always sets refdiv=0 */
  pll_params->refdiv = 0;
#endif /* TUNER_PARAMS */

  /* compute VCO ouput divider (sel_div) */
  pll_params->sel_div = 0;
  double vco_frequency = frequency * 2.0;
  while (pll_params->sel_div <= MAX_SEL_DIV && vco_frequency < MIN_VCO_FREQUENCY) {
     pll_params->sel_div++;
     vco_frequency *= 2.0;
  }
  if (pll_params->sel_div > MAX_SEL_DIV) {
    fprintf(stderr, "requested PLL frequency is too low: %lg\n", frequency);
    return -1;
  }

  /* compute PLL feedback divider */
  double multiplier;
  if (pll_params->refdiv == 0) {
    multiplier = vco_frequency / (2 * this->xtal_frequency);
  } else if (pll_params->refdiv == 1) {
    multiplier = vco_frequency / this->xtal_frequency;
  }
  if (multiplier < MIN_MULTIPLIER) {
    fprintf(stderr, "requested PLL frequency is too low: %lg\n", frequency);
    return -1;
  }
  if (multiplier >= MAX_MULTIPLIER) {
    fprintf(stderr, "requested PLL frequency is too high: %lg\n", frequency);
    return -1;
  }
  uint32_t mult_scaled = (uint32_t) round(multiplier * SDM_FRAC_PRECISION);
  uint32_t mult_int = mult_scaled / SDM_FRAC_PRECISION;
  uint32_t mult_frac = mult_scaled % SDM_FRAC_PRECISION;

#ifdef TUNER_BOUNDARY_SPUR_PREVENTION
  /* boundary spur prevention */
  const uint32_t BOUNDARY_MARGIN = SDM_FRAC_PRECISION / 128;
  const uint32_t LOWER_HALF_MARGIN = SDM_FRAC_PRECISION / 2 - BOUNDARY_MARGIN / 2;
  const uint32_t UPPER_HALF_MARGIN = SDM_FRAC_PRECISION / 2 + BOUNDARY_MARGIN / 2;
  if (mult_frac < BOUNDARY_MARGIN) {
    mult_frac = 0;
  } else if (mult_frac > SDM_FRAC_PRECISION - BOUNDARY_MARGIN) {
    mult_int++;
    mult_frac = 0;
  } else if (mult_frac < SDM_FRAC_PRECISION / 2 && mult_frac > LOWER_HALF_MARGIN) {
    mult_frac = LOWER_HALF_MARGIN;
  } else if (mult_frac > SDM_FRAC_PRECISION / 2 && mult_frac < UPPER_HALF_MARGIN) {
    mult_frac = UPPER_HALF_MARGIN;
  }
#endif /* TUNER_BOUNDARY_SPUR_PREVENTION */

  pll_params->ni2c = (mult_int - 13) / 4;
  pll_params->si2c = (mult_int - 13) % 4;
  pll_params->pw_sdm = mult_frac == 0;
  pll_params->sdm = (uint16_t) mult_frac;

  /* all good */
  return 0;
}


static int tuner_apply_pll_parameters(tuner_t *this,
                                      const struct tuner_pll_parameters *pll_params)
{
  /* set PLL autotune = 128kHz */
  int ret = tuner_write_value(this, R820T2_PLL_AUTO_CLK, 0);
  if (ret < 0) {
    log_error("tuner_write_value() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  /* set VCO current = 100 */
  ret = tuner_write_value(this, R820T2_VCO_CURRENT, 4);
  if (ret < 0) {
    log_error("tuner_write_value() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  /* set PLL parameters */
  tuner_set_value(this, R820T2_REFDIV, pll_params->refdiv);
  tuner_set_value(this, R820T2_SEL_DIV, pll_params->sel_div);
  tuner_set_value(this, R820T2_PW_SDM, pll_params->pw_sdm);
  tuner_set_value(this, R820T2_SI2C, pll_params->si2c);
  tuner_set_value(this, R820T2_NI2C, pll_params->ni2c);
  tuner_set_value(this, R820T2_SDM_INL, pll_params->sdm & 0xff);
  tuner_set_value(this, R820T2_SDM_INH, (pll_params->sdm >> 8) & 0xff);

  ret = tuner_write_registers(this, this->registers_dirty_mask);
  if (ret < 0) {
    log_error("tuner_write_registers() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  /* is PLL locked? */
  sleep(1);
  uint8_t vco_indicator = 0;
  ret = tuner_read_value(this, R820T2_VCO_INDICATOR, &vco_indicator);
  if (ret < 0) {
    log_error("tuner_read_value() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  /* if the PLL is not  locked, try increasing the current */
  if (!(vco_indicator & 0x40)) {
    ret = tuner_write_value(this, R820T2_VCO_CURRENT, 3);
    if (ret < 0) {
      log_error("tuner_write_value() failed", __func__, __FILE__, __LINE__);
      return -1;
    }
    sleep(1);
    uint8_t vco_indicator = 0;
    ret = tuner_read_value(this, R820T2_VCO_INDICATOR, &vco_indicator);
    if (ret < 0) {
      log_error("tuner_read_value() failed", __func__, __FILE__, __LINE__);
      return -1;
    }
  }
  if (!(vco_indicator & 0x40)) {
    fprintf(stderr, "ERROR - unable to get the PLL to lock\n");
    return -1;
  }

  /* set PLL autotune = 8kHz */
  ret = tuner_write_value(this, R820T2_PLL_AUTO_CLK, 2);
  if (ret < 0) {
    log_error("tuner_write_value() failed", __func__, __FILE__, __LINE__);
    return -1;
  }

  return 0;
}


struct tuner_mux_parameters {
  uint8_t open_d;       /* Open Drain */
  uint8_t rf_mux_ploy;  /* RF_MUX, Polymux */
  uint8_t tf_c;         /* Tracking Filter Band */
};


static int tuner_set_mux(tuner_t *this, double frequency)
{
  struct tuner_mux_parameters mux_params;
  int ret = tuner_compute_mux_parameters(this, frequency, &mux_params);
  if (ret < 0) {
    fprintf(stderr, "ERROR - tuner_compute_mux_parameters() failed\n");
    return -1;
  }

  ret = tuner_apply_mux_parameters(this, &mux_params);
  if (ret < 0) {
    fprintf(stderr, "ERROR - tuner_apply_mux_parameters() failed\n");
    return -1;
  }
  return 0;
}


static int tuner_compute_mux_parameters(tuner_t *this __attribute__((unused)),
                                        double frequency,
                                        struct tuner_mux_parameters *mux_params)
{
  /* based on Mauro Carvalho Chehab freq_ranges table
   * https://github.com/torvalds/linux/blob/master/drivers/media/tuners/r820t.c
   * Copyright (C) 2013 Mauro Carvalho Chehab
   */
  const struct {
    double lower_frequency;
    struct tuner_mux_parameters mux_parameters;
  } mux_params_table[] = {
  /*
   * freq                            open_d  rf_mux_ploy     tf_c
   */
  {   0.0, { 0x08, 0x02, 0xdf } },  /*  low,  (LPF, low),  (band2, band0) */
  {  50e6, { 0x08, 0x02, 0xbe } },  /*  low,  (LPF, low),  (band4, band1) */
  {  55e6, { 0x08, 0x02, 0x8b } },  /*  low,  (LPF, low),  (band7, band4) */
  {  60e6, { 0x08, 0x02, 0x7b } },  /*  low,  (LPF, low),  (band8, band4) */
  {  65e6, { 0x08, 0x02, 0x69 } },  /*  low,  (LPF, low),  (band9, band6) */
  {  70e6, { 0x08, 0x02, 0x58 } },  /*  low,  (LPF, low),  (band10, band7) */
  {  75e6, { 0x00, 0x02, 0x44 } },  /* high,  (LPF, low),  (band11, band11) */
  {  80e6, { 0x00, 0x02, 0x44 } },  /* high,  (LPF, low),  (band11, band11) */
  {  90e6, { 0x00, 0x02, 0x34 } },  /* high,  (LPF, low),  (band12, band11) */
  { 100e6, { 0x00, 0x02, 0x34 } },  /* high,  (LPF, low),  (band12, band11) */
  { 110e6, { 0x00, 0x02, 0x24 } },  /* high,  (LPF, low),  (band13, band11) */
  { 120e6, { 0x00, 0x02, 0x24 } },  /* high,  (LPF, low),  (band13, band11) */
  { 140e6, { 0x00, 0x02, 0x14 } },  /* high,  (LPF, low),  (band14, band11) */
  { 180e6, { 0x00, 0x02, 0x13 } },  /* high,  (LPF, low),  (band14, band12) */
  { 220e6, { 0x00, 0x02, 0x13 } },  /* high,  (LPF, low),  (band14, band12) */
  { 250e6, { 0x00, 0x02, 0x11 } },  /* high,  (LPF, low),  (highest, highest) */
  { 280e6, { 0x00, 0x02, 0x00 } },  /* high,  (LPF, low),  (highest, highest) */
  { 310e6, { 0x00, 0x41, 0x00 } },  /* high,  (bypass, mid),  (highest, highest) */
  { 450e6, { 0x00, 0x41, 0x00 } },  /* high,  (bypass, mid),  (highest, highest) */
  { 588e6, { 0x00, 0x40, 0x00 } },  /* high,  (bypass, highest),  (highest, highest) */
  { 650e6, { 0x00, 0x40, 0x00 } }   /* high,  (bypass, highest),  (highest, highest) */
  };

  memset(mux_params, 0, sizeof(*mux_params));

  int mux_params_table_size = sizeof(mux_params_table) / sizeof(mux_params_table);
  int idx;
  for (idx = 0; idx < mux_params_table_size - 1; ++idx) {
    if (frequency < mux_params_table[idx+1].lower_frequency) {
      break;
    }
  }
  mux_params->open_d = mux_params_table[idx].mux_parameters.open_d;
  mux_params->rf_mux_ploy = mux_params_table[idx].mux_parameters.rf_mux_ploy;
  mux_params->tf_c = mux_params_table[idx].mux_parameters.tf_c;

  /* all good */
  return 0;
}


static int tuner_apply_mux_parameters(tuner_t *this,
                                      const struct tuner_mux_parameters *mux_params)
{
  /* set MUX parameters */
  tuner_set_value(this, R820T2_OPEN_D, mux_params->open_d >> 3);
  tuner_set_value(this, R820T2_RFMUX, (mux_params->rf_mux_ploy & 0xc0) >> 6);
  tuner_set_value(this, R820T2_RFFILT, (mux_params->rf_mux_ploy & 0x03) >> 0);
  tuner_set_value(this, R820T2_TF_NCH, (mux_params->tf_c & 0xf0) >> 4);
  tuner_set_value(this, R820T2_TF_LP, (mux_params->tf_c & 0x0f) >> 0);

  /* XTAL CAP & Drive */
  /* Internal xtal no cap,  bit3 = 0 ? */
  tuner_set_value(this, R820T2_XTAL_DRIVE, 0);
  tuner_set_value(this, R820T2_CAPX, 0);
  /* Mixer buffer power on, high current, Image Gain Adjustment min */
  tuner_set_value(this, R820T2_PWD_AMP, 1);
  tuner_set_value(this, R820T2_PW0_AMP, 0);
  tuner_set_value(this, R820T2_IMR_G, 0);
  /* IF Filter power on, high current, Image Gain Adjustment min */
  tuner_set_value(this, R820T2_PWD_IFFILT, 0);
  tuner_set_value(this, R820T2_PW1_IFFILT, 0);
  tuner_set_value(this, R820T2_IMR_P, 0);

  int ret = tuner_write_registers(this, this->registers_dirty_mask);
  if (ret < 0) {
    log_error("tuner_write_registers() failed", __func__, __FILE__, __LINE__);
    return -1;
  }
  return 0;
}


static int tuner_read_value(tuner_t *this, const uint8_t where[3],
                            uint8_t *value) {
  uint8_t reg = where[0];
  int ret = usb_device_i2c_read(this->usb_device, R820T2_ADDR_READ,
                                reg, this->registers + reg, 1);
  if (ret < 0) {
    log_error("usb_device_i2c_read() failed", __func__, __FILE__, __LINE__);
    return -1;
  }
  this->registers_dirty_mask &= ~(1 << reg);
  *value = (this->registers[reg] & where[1]) >> where[2];
  return 0;
}


static int tuner_read_registers(tuner_t *this, uint32_t register_mask)
{
  uint32_t mask = register_mask & R820T2_REGISTERS_READ_MASK;
  int from = -1;
  for (int i = 0; i <= R820T2_REGISTERS; i++) {
    if (i == R820T2_REGISTERS || ((1 << i) & mask) == 0) {
      if (from >= 0) {
        /* read from 'from' to i-1 */
        int ret = usb_device_i2c_read(this->usb_device, R820T2_ADDR_READ,
                                      from, this->registers + from, i - from);
        if (ret < 0) {
          log_error("usb_device_i2c_read() failed", __func__, __FILE__, __LINE__);
          return -1;
        }
        from = -1;
      }
    } else {
      if (from < 0) {
        from = i;
      }
    }
  }
  this->registers_dirty_mask &= ~mask;
  return 0;
}


static int tuner_write_value(tuner_t *this, const uint8_t where[3],
                             uint8_t value) {
  uint8_t reg = where[0];
  this->registers[reg] &= ~where[1];
  this->registers[reg] |= value << where[2];
  this->registers_dirty_mask |= 1 << reg;
  int ret = usb_device_i2c_write(this->usb_device, R820T2_ADDR_WRITE,
                                 reg, this->registers + reg, 1);
  if (ret < 0) {
    log_error("usb_device_i2c_write() failed", __func__, __FILE__, __LINE__);
    return -1;
  }
  this->registers_dirty_mask &= ~(1 << reg);
}


static int tuner_write_registers(tuner_t *this, uint32_t register_mask)
{
  uint32_t mask = register_mask & R820T2_REGISTERS_WRITE_MASK;
  int from = -1;
  for (int i = 0; i <= R820T2_REGISTERS; i++) {
    if (i == R820T2_REGISTERS || ((1 << i) & mask) == 0) {
      if (from >= 0) {
        /* write from 'from' to i-1 */
        int ret = usb_device_i2c_write(this->usb_device, R820T2_ADDR_WRITE,
                                       from, this->registers + from, i - from);
        if (ret < 0) {
          log_error("usb_device_i2c_write() failed", __func__, __FILE__, __LINE__);
          return -1;
        }
        from = -1;
      }
    } else {
      if (from < 0) {
        from = i;
      }
    }
  }
  this->registers_dirty_mask &= ~mask;
  return 0;
}


static uint8_t tuner_get_value(tuner_t *this, const uint8_t where[3]) {
  uint8_t reg = where[0];
  return (this->registers[reg] & where[1]) >> where[2];
}


static void tuner_set_value(tuner_t *this, const uint8_t where[3],
                            uint8_t value) {
  uint8_t reg = where[0];
  this->registers[reg] &= ~where[1];
  this->registers[reg] |= value << where[2];
  this->registers_dirty_mask |= 1 << reg;
}
