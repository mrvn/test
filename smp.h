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
 * SMP support for the Raspberry Pi
 */

#ifndef KERNEL_SMP_H
#define KERNEL_SMP_H 1

namespace SMP {
    typedef void (*start_fn_t)(void *);
    // wake up the additional cores
    void start_core(int core, start_fn_t start, void *arg);
}

#endif // #ifndef KERNEL_SMP_H

