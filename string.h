/* Copyright (C) 2015 Goswin von Brederlow <goswin-v-b@web.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 *      ISO C99 Standard: 7.21 String handling  <string.h>
 */

#ifndef _STRING_H
#define _STRING_H       1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void * memcpy(void * __restrict__ dest, const void * __restrict__ src, size_t n) __attribute__((used));

size_t strlen(const char *str);
    
#ifdef __cplusplus
}
#endif

#endif // #ifndef _STRING_H
