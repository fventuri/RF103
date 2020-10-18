/*
 * rf103_test - simple stream test program  for librf103
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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rf103.h"


static void count_bytes_callback(uint32_t data_size, uint8_t *data,
                                 void *context);
static unsigned long long total_bytes;
static int num_callbacks;
static int runtime = 3000;
static struct timespec clk_start, clk_end;
static int stop_reception = 0;

static double clk_diff() {
  return ((double)clk_end.tv_sec + 1.0e-9*clk_end.tv_nsec) - 
           ((double)clk_start.tv_sec + 1.0e-9*clk_start.tv_nsec);
}


int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s <image file> <sample rate> [<runtime_in_ms>]\n", argv[0]);
    return -1;
  }
  char *imagefile = argv[1];
  double sample_rate = 0.0;
  sscanf(argv[2], "%lf", &sample_rate);
  if (3 < argc)
    runtime = atoi(argv[3]);

  if (sample_rate <= 0) {
    fprintf(stderr, "ERROR - given samplerate '%f' should be > 0\n", sample_rate);
    return -1;
  }

  int ret_val = -1;

  rf103_t *rf103 = rf103_open(0, imagefile);
  if (rf103 == 0) {
    fprintf(stderr, "ERROR - rf103_open() failed\n");
    return -1;
  }

  if (rf103_set_sample_rate(rf103, sample_rate) < 0) {
    fprintf(stderr, "ERROR - rf103_set_sample_rate() failed\n");
    goto DONE;
  }

  if (rf103_set_async_params(rf103, 0, 0, count_bytes_callback, rf103) < 0) {
    fprintf(stderr, "ERROR - rf103_set_async_params() failed\n");
    goto DONE;
  }

  total_bytes = 0;
  num_callbacks = 0;
  if (rf103_start_streaming(rf103) < 0) {
    fprintf(stderr, "ERROR - rf103_start_streaming() failed\n");
    return -1;
  }

  fprintf(stderr, "started streaming .. for %d ms ..\n", runtime);

  /* todo: move this into a thread */
  stop_reception = 0;
  clock_gettime(CLOCK_REALTIME, &clk_start);
  while (!stop_reception)
    rf103_handle_events(rf103);

  fprintf(stderr, "finished. now stop streaming ..\n");
  if (rf103_stop_streaming(rf103) < 0) {
    fprintf(stderr, "ERROR - rf103_stop_streaming() failed\n");
    return -1;
  }

  double dur = clk_diff();
  fprintf(stderr, "total bytes received=%llu in %d callbacks\n", total_bytes, num_callbacks);
  fprintf(stderr, "run for %f sec\n", dur);
  fprintf(stderr, "approx. samplerate is %f kSamples/sec\n", total_bytes / (2000.0*dur) );

  /* done - all good */
  ret_val = 0;

DONE:
  rf103_close(rf103);

  return ret_val;
}

static void count_bytes_callback(uint32_t data_size,
                                 uint8_t *data __attribute__((unused)),
                                 void *context __attribute__((unused)) )
{
  if (stop_reception)
    return;
  ++num_callbacks;
  total_bytes += data_size;
  if ( num_callbacks >= runtime ) {
    clock_gettime(CLOCK_REALTIME, &clk_end);
    stop_reception = 1;
  }
}

