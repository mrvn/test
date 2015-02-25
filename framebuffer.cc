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
 * Framebuffer support for the Raspberry Pi
 */

#include <stdint.h>
#include "framebuffer.h"
#include "peripherals.h"
#include "barriers.h"
#include "stdio.h"
#include "font.h"

namespace Framebuffer {
    enum {
	// Mailbox registers
	MAILBOX0READ   = 0xb880,
	MAILBOX0STATUS = 0xb898,
	MAILBOX0WRITE  = 0xb8a0,

	// Channel for framebuffer
	FBCHAN = 8,
    };

#define MAILBOX(x) (Peripherals::reg(x))

    FB fb;

    // Status register
    enum { SHIFT_EMPTY = 30, SHIFT_FULL };
    enum Flags {
	EMPTY = 1 << SHIFT_EMPTY,
	FULL = 1 << SHIFT_FULL
    };

    void write(void *buf, uint32_t chan) {
	volatile uint32_t *status = MAILBOX(MAILBOX0STATUS);
	volatile uint32_t *write = MAILBOX(MAILBOX0WRITE);
	data_memory_barrier();
	// don't use cached values
	flush_cache();
	puts("status at ");
	put_uint32((intptr_t)status);
	putc('\n');
	// wait for mailbox to be not full
	while(*status & FULL) {
	    puts("full\n");
	    flush_cache();
	}
	*write = (intptr_t)buf | chan;
    }

    void * read(uint32_t chan) {
	volatile uint32_t *status = MAILBOX(MAILBOX0STATUS);
	volatile uint32_t *read = MAILBOX(MAILBOX0READ);
	// possibly switching peripheral
	data_memory_barrier();
	while(true) {
	    // don't use cached values
	    flush_cache();
	    // wait for mailbox to contain something
	    while(*status & EMPTY) {
		flush_cache();
	    }
	    uint32_t res = *read;
	    if ((res & 0xF) == chan) return (void *)(res & ~0xF);
	}
    }
  
    // initialize framebuffer
    Error init(void) {
	puts("Framebuffer::init()\n");

	// some spare memory for mail content
	uint32_t mailbuffer[1024] __attribute__((aligned(16)));
	
	/* Get the display size */
	mailbuffer[0] = 8 * 4; // Total size
	mailbuffer[1] = 0; // Request
	mailbuffer[2] = 0x40003; // Display size
	mailbuffer[3] = 8; // Buffer size
	mailbuffer[4] = 0; // Request size
	mailbuffer[5] = 0; // Space for horizontal resolution
	mailbuffer[6] = 0; // Space for vertical resolution
	mailbuffer[7] = 0; // End tag

	write(mailbuffer, FBCHAN);
	uint32_t *buf = (uint32_t *)read(FBCHAN);
	
	/* Valid response in data structure? */
	if (buf[1] != 0x80000000) return FAIL_GET_RESOLUTION;

	fb.width = buf[5];
	fb.height = buf[6];

	if (fb.width == 0 || fb.height == 0) return FAIL_GOT_INVALID_RESOLUTION;

	/* Set up screen */
	unsigned int c = 1;
	mailbuffer[c++] = 0; // Request

	mailbuffer[c++] = 0x00048003; // Tag id (set physical size)
	mailbuffer[c++] = 8; // Value buffer size (bytes)
	mailbuffer[c++] = 8; // Req. + value length (bytes)
	mailbuffer[c++] = fb.width; // Horizontal resolution
	mailbuffer[c++] = fb.height; // Vertical resolution

	mailbuffer[c++] = 0x00048004; // Tag id (set virtual size)
	mailbuffer[c++] = 8; // Value buffer size (bytes)
	mailbuffer[c++] = 8; // Req. + value length (bytes)
	mailbuffer[c++] = fb.width; // Horizontal resolution
	mailbuffer[c++] = fb.height; // Vertical resolution

	mailbuffer[c++] = 0x00048005; // Tag id (set depth)
	mailbuffer[c++] = 4; // Value buffer size (bytes)
	mailbuffer[c++] = 4; // Req. + value length (bytes)
	mailbuffer[c++] = 32; // 32 bpp

	mailbuffer[c++] = 0x00040001; // Tag id (allocate framebuffer)
	mailbuffer[c++] = 8; // Value buffer size (bytes)
	mailbuffer[c++] = 4; // Req. + value length (bytes)
	mailbuffer[c++] = 16; // Alignment = 16
	mailbuffer[c++] = 0; // Space for response

	mailbuffer[c++] = 0; // Terminating tag

	mailbuffer[0] = c*4; // Buffer size

	write(mailbuffer, FBCHAN);
	buf = (uint32_t *)read(FBCHAN);

	/* Valid response in data structure */
	if(buf[1] != 0x80000000) return FAIL_SETUP_FRAMEBUFFER;

	// Scan replies for allocate response
	unsigned int i = 2; /* First tag */
	uint32_t data;
	while ((data = buf[i])) {
	    // allocate response?
	    if (data == 0x40001) break;

	    /* Skip to next tag
	     * Advance count by 1 (tag) + 2 (buffer size/value size)
	     * + specified buffer size
	     */
	    i += 3 + (buf[i + 1] >> 2);

	    if (i > c) return FAIL_INVALID_TAGS;
	}

	/* 8 bytes, plus MSB set to indicate a response */
	if (buf[i + 2] != 0x80000008) return FAIL_INVALID_TAG_RESPONSE;

	/* Framebuffer address/size in response */
	fb.base = buf[i + 3];
	fb.size = buf[i + 4];

	if (fb.base == 0 || fb.size == 0) return FAIL_INVALID_TAG_DATA;

	// fb.base += 0xC0000000; // physical to virtual
	
	/* Get the framebuffer pitch (bytes per line) */
	mailbuffer[0] = 7 * 4; // Total size
	mailbuffer[1] = 0; // Request
	mailbuffer[2] = 0x40008; // Display size
	mailbuffer[3] = 4; // Buffer size
	mailbuffer[4] = 0; // Request size
	mailbuffer[5] = 0; // Space for pitch
	mailbuffer[6] = 0; // End tag

	write(mailbuffer, FBCHAN);
	buf = (uint32_t *)read(FBCHAN);

	/* 4 bytes, plus MSB set to indicate a response */
	if (buf[4] != 0x80000004) return FAIL_INVALID_PITCH_RESPONSE;

	fb.pitch = buf[5];
	if (fb.pitch == 0) return FAIL_INVALID_PITCH_DATA;

	// set alpha to 0xff everywhere
	for(uint32_t n = 3; n < fb.size; n += 4) {
	    *(uint8_t*)(fb.base + n) = 0xff;
	}

	// draw chessboard pattern
	for(uint32_t y = 0; y < fb.height; ++y) {
	    for(uint32_t x = 0; x < fb.width; ++x) {
		Pixel *p = (Pixel*)(fb.base + x * sizeof(Pixel) + y * fb.pitch);
		uint8_t col = ((x & 16) ^ (y & 16)) ? 0x00 : 0xff;
		p->red = col;
		p->green = col;
		p->blue = col;
	    }
	}

	// draw back->red fade left to right at the top
	// draw back->blue fade left to right at the bottom
	for(int y = 0; y < 16; ++y) {
	    for(int x = 16; x < 256 + 16; ++x) {
		Pixel *p = (Pixel*)(fb.base + x * sizeof(Pixel) + y * fb.pitch);
		p->red = x - 16;
		p->green = 0;
		p->blue = 0;
		p = (Pixel*)(fb.base + x * sizeof(Pixel) + (fb.height - y - 1) * fb.pitch);
		p->red = 0;
		p->green = 0;
		p->blue = x - 16;
	    }
	}
	// draw back->green fade top to bottom at the left
	// draw back->green fade top to bottom at the right
	for(int y = 16; y < 256 + 16; ++y) {
	    for(int x = 0; x < 16; ++x) {
		Pixel *p = (Pixel*)(fb.base + x * sizeof(Pixel) + y * fb.pitch);
		p->red = 0;
		p->green = y - 16;
		p->blue = 0;
		p = (Pixel*)(fb.base + (fb.width - x- 1) * sizeof(Pixel) + y * fb.pitch);
		p->red = y - 16;
		p->green = y - 16;
		p->blue = y - 16;		
	    }
	}

	const char text[] = "MOOSE V0.0";
	struct Arg {
	    uint32_t color;
	    uint32_t border;
	    bool fill;
	} args[] = {
	    {~0LU,  0, false},
	    { 0, ~0LU, false},
	    {~0LU,  0, true},
	    { 0, ~0LU, true},
	    {0xff0000ff,  0,   false},
	    {0xff0000ff, ~0LU, false},
	    {0xff0000ff,  0,   true},
	    {0xff0000ff, ~0LU, true},
	    {0xff00ff00,  0,   false},
	    {0xff00ff00, ~0LU, false},
	    {0xff00ff00,  0,   true},
	    {0xff00ff00, ~0LU, true},
	    {0xffff0000,  0,   false},
	    {0xffff0000, ~0LU, false},
	    {0xffff0000,  0,   true},
	    {0xffff0000, ~0LU, true},
	};
	int y = 152;
	Arg *arg = args;
	for (i = 0; i < 16; ++i) {
	    int x = 152;
	    for (const char *p = text; *p; ++p) {
		Font::putc(fb, x, y, *p, arg->color, arg->border, arg->fill);
		x += 8;
	    }
	    y += 16;
	    ++arg;
	}
	
	return SUCCESS;
    }
}

