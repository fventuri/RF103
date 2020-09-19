/*
 * clock_source.h - Si5351 functions
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

#ifndef __CLOCK_SOURCE_H
#define __CLOCK_SOURCE_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct clock_source clock_source_t;

clock_source_t *clock_source_open();

void clock_source_close(clock_source_t *this);

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_SOURCE_H */
