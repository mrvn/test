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
 * Barrieris for the Raspberry Pi
 */

#ifndef KERNEL_BARRIERS_H
#define KERNEL_BARRIERS_H 1

/*
 * Data memory barrier
 * No memory access after the DMB can run until all memory accesses
 * before it have completed.
 */
static inline void data_memory_barrier(void) {
    // RPi
    // asm volatile("mcr p15, #0, %[zero], c7, c10, #5"
    //	     : : [zero]"r"(0));
    // RPi2
    asm volatile ("dmb" ::: "memory");
}

static inline void instruction_barrier(void) {
    asm volatile ("isb" ::: "memory");
}

static inline void data_sync_barrier(void) {
    asm volatile ("dsb" ::: "memory");
}

#endif // #ifndef KERNEL_BARRIERS_H

