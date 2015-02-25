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

#include "gpio.h"
#include "peripherals.h"
#include "delay.h"

namespace GPIO {
    enum {
	// function selector
	GPIO_FSEL0   = 0x00, // 0x3F200000
	GPIO_FSEL1   = 0x04, // 0x3F200004
	GPIO_FSEL2   = 0x08, // 0x3F200008
	GPIO_FSEL3   = 0x0C, // 0x3F20000C
	GPIO_FSEL4   = 0x10, // 0x3F200010
	GPIO_FSEL5   = 0x14, // 0x3F200014

	// set and clear pin output
	GPIO_SET0    = 0x1C, // 0x3F20001C
	GPIO_SET1    = 0x20, // 0x3F200020
	GPIO_CLR0    = 0x28, // 0x3F200028
	GPIO_CLR1    = 0x2C, // 0x3F20002C

	// Controls actuation of pull up/down to ALL GPIO pins.
	GPIO_PUD     = 0x94, // 0x3F200094
	// Controls actuation of pull up/down for specific GPIO pin.
	GPIO_PUDCLK0 = 0x98, // 0x3F200098
	GPIO_PUDCLK1 = 0x9C, // 0x3F20009C
    };

    constexpr volatile uint32_t * reg(uint32_t offset) {
	return Peripherals::reg(GPIO::GPIO_BASE + offset);
    }

    void set_function(uint32_t pin, FSel fn) {
	volatile uint32_t *fsel = &reg(GPIO_FSEL0)[pin / 10];
	uint32_t shift = (pin % 10) * 3;
	uint32_t mask = ~(7 << shift);
	*fsel = (*fsel & mask) | (fn << shift);
    }

    void set(uint32_t pin, bool state) {
	// set or clear bit for pin
	reg(state ? GPIO_SET0 : GPIO_CLR0)[pin / 32] = 1 << (pin % 32);
    }

    void set_pull_up_down(uint32_t pin, PullUpDown action) {
        // set action & delay for 150 cycles.
	volatile uint32_t *pud = reg(GPIO_PUD);
	*pud = action;
	delay(150);

	// trigger action & delay for 150 cycles.
	volatile uint32_t *clock =
	    reg((pin < 32) ? GPIO_PUDCLK0 : GPIO_PUDCLK1);
	*clock = (1 << (pin % 32));
	delay(150);

	// clear action
	*pud = OFF;
	
        // remove clock
	*clock = 0;
    }
}

