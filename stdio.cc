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
 *      ISO C99 Standard: 7.19 Input/output     <stdio.h>
 */

#include "stdio.h"
#include "uart.h"
#include "string.h"

void putc(char c) {
    UART::put(c);
}

void puts(const char *str) {
    size_t len = strlen(str);
    UART::write(str, len);
}

void put_uint32(uint32_t x) {
    static const char HEX[] = "0123456789ABCDEF";
    char buf[] = "0x00000000";
    char *p = &buf[2];
    for(int i = 28; i >= 0; i -= 4) {
	*p++ = HEX[(x >> i) % 16];
    }
    UART::write(buf, 10);
}
