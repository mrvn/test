/* font.h - simple ascii font bitmap */
/* Copyright (C) 2013 Goswin von Brederlow <goswin-v-b@web.de> (functions)

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

#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include "framebuffer.h"

namespace Font {
    void putc(const Framebuffer::FB &fb, uint32_t x, uint32_t y, char c,
	      uint32_t color = ~0L, uint32_t border = 0, bool fill = false);
}

#endif // #ifndef FONT_H
