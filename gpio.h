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
 * GPIO helper for the Raspberry Pi
 */

#ifndef KERNEL_GPIO_H
#define KERNEL_GPIO_H 1

#include <stdint.h>

namespace GPIO {
    enum {
	GPIO_BASE = 0x200000, // 0x3F200000
    };

    enum FSel {
	INPUT, OUTPUT, FN5, FN4, FN0, FN1, FN2, FN3,
    };
    
    void set_function(uint32_t pin, FSel fn);
    void set(uint32_t pin, bool state);

    enum PullUpDown {
	OFF, UP, DOWN,
    };

    void set_pull_up_down(uint32_t pin, PullUpDown action);
}

#endif // #ifndef KERNEL_GPIO_H

