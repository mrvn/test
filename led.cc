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
 * Activity LED on the Raspberry Pi
 */

#include "led.h"
#include "gpio.h"

namespace LED {
    // configure GPIO pins for the LED
    void init(void) {
	// Disable pull up/down for pin 47
	GPIO::set_pull_up_down(47, GPIO::OFF);
	// configure GPIO pin 47 for output
	GPIO::set_function(47, GPIO::OUTPUT);
    }

    // turn LED on or off
    void set(bool state) {
	// set or clear pin 47
	GPIO::set(47, state);
    }
}

