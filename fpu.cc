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
 * FPU support for the Raspberry Pi
 */

#include <stdint.h>
#include "fpu.h"

namespace FPU {
    // Enable Advanced SIMD & Vector Floating Point Calculations (NEON MPE)
    void init(void) {
	uint32_t reg;
	// read Access Control Register
	asm volatile("mrc p15,0,%0,c1,c0,2" : "=r" (reg));
	// Enable Single & Double Precision
	reg |= 0x300000 + 0xC00000;
	// write Access Control Register
	asm volatile("mcr p15,0,%0,c1,c0,2" :: "r" (reg));
	// enable VFP
	asm volatile("vmsr fpexc,%0" :: "r" (0x40000000));
    }
}

