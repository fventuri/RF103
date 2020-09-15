/*
 * rf103_test - simple test tool for librf103
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

#include <stdio.h>

#include "rf103.h"


int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <image file>\n", argv[0]);
    return -1;
  }
  char *imagefile = argv[1];

  /* count devices */
  int count = rf103_get_device_count();
  if (count < 0) {
    fprintf(stderr, "ERROR - rf103_get_device_count() failed\n");
    return -1;
  }
  printf("device count=%d\n", count);

  /* get device info */
  struct rf103_device_info *rf103_device_infos;
  count = rf103_get_device_info(&rf103_device_infos);
  if (count < 0) {
    fprintf(stderr, "ERROR - rf103_get_device_info() failed\n");
    return -1;
  }
  for (int i = 0; i < count; ++i) {
    printf("%d - manufacturer=\"%s\" product=\"%s\" serial number=\"%s\"\n",
           i, rf103_device_infos[i].manufacturer, rf103_device_infos[i].product,
           rf103_device_infos[i].serial_number);
  }
  rf103_free_device_info(rf103_device_infos);

  /* open and close device */
  rf103_t *rf103 = rf103_open(0, imagefile);
  if (rf103 == 0) {
    fprintf(stderr, "ERROR - rf103_open() failed\n");
    return -1;
  }
  rf103_close(rf103);

  return 0;
}
