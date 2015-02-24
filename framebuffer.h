/* framebuffer.h - Framebuffer initialization & communication */
/* Copyright (C) 2013 Goswin von Brederlow <goswin-v-b@web.de>

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

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

namespace Framebuffer {
    enum Error {
	SUCCESS,
	// Error codes
	FAIL_GET_RESOLUTION,
	FAIL_GOT_INVALID_RESOLUTION,
	FAIL_SETUP_FRAMEBUFFER,
	FAIL_INVALID_TAGS,
	FAIL_INVALID_TAG_RESPONSE,
	FAIL_INVALID_TAG_DATA,
	FAIL_INVALID_PITCH_RESPONSE,
	FAIL_INVALID_PITCH_DATA
    };

    struct Pixel {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;
    };
    
    struct FB {
	uint32_t base;
	uint32_t size; // in bytes
	uint32_t pitch; // in bytes
	uint32_t width;
	uint32_t height;
    };

    extern FB fb;

    Error init(void);
}

#endif // #ifndef FRAMEBUFFER_H
