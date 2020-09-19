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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "clock_source.h"
#include "logging.h"


typedef struct clock_source clock_source_t;


typedef struct clock_source {
  uint8_t place_holder;
} clock_source_t;


clock_source_t *clock_source_open()
{
  clock_source_t *ret_val = 0;

  // TODO

  /* we are good here - create and initialize the clock_source */
  clock_source_t *this = (clock_source_t *) malloc(sizeof(clock_source_t));
  this->place_holder = 0;

  ret_val = this;
  return ret_val;
}


void clock_source_close(clock_source_t *this)
{
  free(this);
  return;
}
