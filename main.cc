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

#include <stdint.h>
#include "string.h"
#include "led.h"
#include "uart.h"
#include "mmu.h"
#include "fpu.h"
#include "smp.h"
#include "stdio.h"

#include "gpio.h"
#include "framebuffer.h"
#include "font.h"
#include "peripherals.h"
#include "delay.h"
#include "barriers.h"

#define UNUSED(x) (void)x

extern "C" {
    void kernel_main(uint32_t r0, uint32_t model_id, void *atags);
}

void blink(uint32_t time) {
	LED::set(true);
	delay(time);
	LED::set(false);
	delay(time);
}

uint32_t panic_delay = 0x10000;

void panic() {
    while(true) {
	blink(panic_delay);
    }
}

/**********************************************************************
 * Mandelbrot                                                         *
 **********************************************************************/
namespace Mandelbrot {
    struct Params {
	volatile double xmin, xmax, ymin, ymax;
	volatile uint32_t nmax;
	volatile uint32_t stepx;
	volatile uint32_t stepy;
	volatile uint32_t line;
	volatile uint32_t running;
    };
    Params params = {
	-2.5, 1.5, -1.25, 1.25,
	64,
	64, 64,
	0,
	0,
    };
    
    bool operator !=(const Framebuffer::Pixel& p, const Framebuffer::Pixel& q) {
	return (p.red != q.red) || (p.green != q.green)
	    || (p.blue != q.blue) || (p.alpha != q.alpha);
    }

    void set_color(Framebuffer::Pixel *p, uint32_t n) {
	if (n == params.nmax) {
	    p->red = 0;
	    p->green = 0;
	    p->blue = 0;
	} else if (n >= params.nmax / 2) { // white
	    p->red = 0xff;
	    p->green = 0xff;
	    p->blue = 0xff;
	} else if (n >= params.nmax / 4) { // yellow -> white
	    int t = (n - params.nmax / 4) * 255 * 4 / params.nmax;
	    p->red = 0xff;
	    p->green = 0xff;
	    p->blue = t;
	} else if (n >= params.nmax / 8) { // red -> yellow
	    int t = (n - params.nmax / 8) * 255 * 8 / params.nmax;
	    p->red = 0xff;
	    p->green = t;
	    p->blue = 0;
	} else if (n >= params.nmax / 16) { // magenta -> red
	    int t = (n - params.nmax / 16) * 255 * 16 / params.nmax;
	    p->red = 0xff;
	    p->green = 0;
	    p->blue = 0xff - t;
	} else if (n >= params.nmax / 32) { // blue -> magenta
	    int t = (n - params.nmax / 32) * 255 * 32 / params.nmax;
	    p->red = t;
	    p->green = 0;
	    p->blue = 0xff;
	} else if (n >= params.nmax / 64) { // cyan -> blue
	    int t = (n - params.nmax / 64) * 255 * 64 / params.nmax;
	    p->red = 0;
	    p->green = 0xff - t;
	    p->blue = 0xff;
	} else { // green -> cyan
	    int t = n * 255 * 64 / params.nmax;
	    p->red = 0;
	    p->green = 0xff;
	    p->blue = t;
	}	    
    }

    void guess(uint32_t stepx, uint32_t stepy) {
	// guess gaps
	for(uint32_t v = 4 * stepy; v + 4 * stepy < Framebuffer::fb.height; v += 2 * stepy) {
	    for(uint32_t u = 4 * stepx; u + 4 * stepx < Framebuffer::fb.width; u += 2 * stepx) {
		Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + u * sizeof(Framebuffer::Pixel) + v * Framebuffer::fb.pitch);
		if (p->alpha != 0xFF) continue;
		if ((p->red == 0 && p->green == 0 && p->blue == 0)
		    || (p->red == 0xff && p->green == 0xff && p->blue == 0xff)) {
		    for(uint32_t y = v - 4 * stepy; y <= v + 4 * stepy; y += 2 * stepy) {
			for(uint32_t x = u - 4 * stepx; x <= u + 4 * stepx; x += 2 * stepx) {
			    if (x == u && y == v) continue;
			    Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
			    if (*p != *q) {
				goto not_same;
			    }
			}
		    }
		} else {
		    for(uint32_t y = v - 2 * stepy; y <= v + 2 * stepy; y += 2 * stepy) {
			for(uint32_t x = u - 2 * stepx; x <= u + 2 * stepx; x += 2 * stepx) {
			    if (x == u && y == v) continue;
			    Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
			    if (*p != *q) {
				goto not_same;
			    }
			}
		    }
		}
		for(uint32_t y = v - stepy; y <= v + stepy; y += stepy) {
		    for(uint32_t x = u - stepx; x <= u + stepx; x += stepx) {
			if (x == u && y == v) continue;
			Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
			*q = *p;
		    }
		}
	    not_same:
		{}
	    }
	}
    }

    void mandel_line(uint32_t v) {
	const double bailout = 16.0;
	double y0 = params.ymin + v * (params.ymax - params.ymin) / Framebuffer::fb.height;
	for(uint32_t u = 0; u < Framebuffer::fb.width; u += params.stepx) {
	    Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + u * sizeof(Framebuffer::Pixel) + v * Framebuffer::fb.pitch);
	    if (p->alpha == 0xff) continue;
	    double x0 = params.xmin + u * (params.xmax - params.xmin) / Framebuffer::fb.width;
	    uint32_t n = 0;
	    double x = x0, x2 = x0 * x0;
	    double y = y0, y2 = y0 * y0;
	    while (n < params.nmax && x2 + y2 < bailout) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		y2 = y * y;
		x2 = x * x;
		++n;
	    }
	    p->alpha = 0xff;
	    set_color(p, n);
	}
    }

    void show_lines(int core, uint32_t lines) {
	char buf[] = "Core 0 computed 0000 lines\n";
	buf[5] = '0' + core;
	buf[16] = '0' + ((lines / 1000) % 10);
	buf[17] = '0' + ((lines /  100) % 10);
	buf[18] = '0' + ((lines /   10) % 10);
	buf[19] = '0' + ((lines /    1) % 10);
	puts(buf);
    }
    
    bool mandelbrot(uint32_t stepx, uint32_t stepy) {
	params.stepx = stepx;
	params.stepy = stepy;
	params.line = 0;
	// compute missing bits
	uint32_t v;
	uint32_t lines = 0;
	while((v = __sync_fetch_and_add(&params.line, params.stepy)) < Framebuffer::fb.height) {
	    if (UART::poll()) {
		// abort computaions
		params.line = Framebuffer::fb.height;
		show_lines(0, lines);
		while(params.running > 0) { }
		return false;
	    }
	    mandel_line(v);
	    ++lines;
	}
	show_lines(0, lines);
	while(params.running > 0) { }
	return true;
    }

    void mandeld(int core) {
	while(true) {
	    // wait for something to do
	    while(params.line >= Framebuffer::fb.height) { }
	    // increase running count
	    __sync_fetch_and_add(&params.running, 1);
	    // loop as long as there is work to do
	    uint32_t v;
	    uint32_t lines = 0;
	    while((v = __sync_fetch_and_add(&params.line, params.stepy)) < Framebuffer::fb.height) {
		++lines;
		mandel_line(v);
	    }
	    show_lines(core, lines);
	    // decrement running count
	    __sync_fetch_and_sub(&params.running, 1);
	}
    }

    int zoom(int step) {
	int zx, zy;
	double xmin, ymin, xmax, ymax;
	char c;
    again:
	puts("Select [1-9no]: ");
	c = UART::get();
	putc(c);
	putc('\n');	
	switch(c) {
	case '1': zx = 0; zy = 2; break;
	case '2': zx = 1; zy = 2; break;
	case '3': zx = 2; zy = 2; break;
	case '4': zx = 0; zy = 1; break;
	case '5': zx = 1; zy = 1; break;
	case '6': zx = 2; zy = 1; break;
	case '7': zx = 0; zy = 0; break;
	case '8': zx = 1; zy = 0; break;
	case '9': zx = 2; zy = 0; break;
	case 'n': goto new_nmax;
	case 'o': goto zoom_out;
	default:
	    if (step > 0) {
		return step;
	    } else {
		goto again;
	    }
	}
	xmin = params.xmin + zx * (params.xmax - params.xmin) / 4;
	ymin = params.ymin + zy * (params.ymax - params.ymin) / 4;
	xmax = params.xmin + (zx + 2) * (params.xmax - params.xmin) / 4;
	ymax = params.ymin + (zy + 2) * (params.ymax - params.ymin) / 4;
	params.xmin = xmin;
	params.ymin = ymin;
	params.xmax = xmax;
	params.ymax = ymax;

	if (zx != 2 || zy != 2) {   
	    for(int y = Framebuffer::fb.height / 2 - 1; y >= 0; --y) {
		int v = y + zy * Framebuffer::fb.height / 4;
		for(int x = Framebuffer::fb.width / 2 - 1; x >= 0; --x) {
		    int u = x + zx * Framebuffer::fb.width / 4;
		    Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + (x + Framebuffer::fb.width / 2) * sizeof(Framebuffer::Pixel) + (y + Framebuffer::fb.height / 2) * Framebuffer::fb.pitch);
		    Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + u * sizeof(Framebuffer::Pixel) + v * Framebuffer::fb.pitch);
		    *p = *q;
		}
	    }
	}
	for(uint32_t y = 0; y < Framebuffer::fb.height; ++y) {
	    for(uint32_t x = 0; x < Framebuffer::fb.width; ++x) {
		if ((x % 2) == 0 && (y % 2) == 0) {
		    Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + (x / 2 + Framebuffer::fb.width / 2) * sizeof(Framebuffer::Pixel) + (y / 2 + Framebuffer::fb.height / 2) * Framebuffer::fb.pitch);
		    Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
		    *q = *p;
		} else {
		    Framebuffer::Pixel *q = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
		    q->red = 0x80;
		    q->green = 0x80;
		    q->blue = 0x80;
		    q->alpha = 0x80;
		}
	    }
	}
	if (step == 0) {
	    step = 1;
	} else {
	    step *= 2;
	    if (step > 64) {
		step = 64;
	    }
	}
	return step;

    new_nmax:
	params.nmax *= 2;
	for(uint32_t y = 0; y < Framebuffer::fb.height; ++y) {
	    for(uint32_t x = 0; x < Framebuffer::fb.width; ++x) {
		Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
		if (p->red == 0 && p->green == 0 && p->blue == 0) {
		    p->red = 0x80;
		    p->green = 0x80;
		    p->blue = 0x80;
		}
		p->alpha = 0x80;
	    }
	}
	return 64;
    zoom_out:
	xmin = params.xmin - (params.xmax - params.xmin) / 2;
	ymin = params.ymin - (params.ymax - params.ymin) / 2;
	xmax = params.xmax + (params.xmax - params.xmin) / 2;
	ymax = params.ymax + (params.ymax - params.ymin) / 2;
	params.xmin = xmin;
	params.ymin = ymin;
	params.xmax = xmax;
	params.ymax = ymax;
	for(uint32_t y = 0; y < Framebuffer::fb.height; ++y) {
	    for(uint32_t x = 0; x < Framebuffer::fb.width; ++x) {
		Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + x * sizeof(Framebuffer::Pixel) + y * Framebuffer::fb.pitch);
		if (p->red == 0 && p->green == 0 && p->blue == 0) {
		    p->red = 0x80;
		    p->green = 0x80;
		    p->blue = 0x80;
		}
		p->alpha = 0x80;
	    }
	}
	return 64;
    }    

    void init(void) {
	// set alpha to 0x0 everywhere
	for(uint32_t n = 3; n < Framebuffer::fb.size; n += 4) {
	    *(uint8_t*)(Framebuffer::fb.base + n) = 0x0;
	}
	int step = 64;
	while(true) {
	    while(step > 0) {
		puts("Nmax = ");
		put_uint32(params.nmax);
		puts(" Step = ");
		put_uint32(step);
		putc('\n');
		guess(step, step);
		if (!mandelbrot(step, step)) break;
		step /= 2;
	    }
	    step = zoom(step);
	}
    }
}

void kernel_main(uint32_t r0, uint32_t model_id, void *atags) {
    UNUSED(r0);
    UNUSED(model_id);
    UNUSED(atags);
    
    LED::init();
    for(int i = 0; i < 3; ++i) {
	blink(0x100000);
    }

    UART::init();
    puts("\nHello\n");
    delay(0x100000);

    Framebuffer::Error error = Framebuffer::init();
    puts("error = ");
    put_uint32(error);
    putc('\n');
    
    MMU::init_page_table();
    MMU::init();
    FPU::init();
    SMP::start_core(1, (SMP::start_fn_t)Mandelbrot::mandeld, (void *)1);
    SMP::start_core(2, (SMP::start_fn_t)Mandelbrot::mandeld, (void *)2);
    SMP::start_core(3, (SMP::start_fn_t)Mandelbrot::mandeld, (void *)3);

    // All cores have caches active, locking now works
    UART::set_with_locks();

    /*
    puts("mapping and cleaning memory");
    MMU::page *p = (MMU::page*)MMU::_mem_start;
    for(uint32_t slot = 0; slot < 256; ++slot) {
	if (slot % 32 == 0) putc('\n');
	putc('.');
	blink(panic_delay);
	MMU::map(slot, (intptr_t)p);
	MMU::page *virt = &((MMU::page *)0x80000000)[slot];
	// writing to page faults
	virt->data[0] = 0;
	MMU::unmap(slot);
	++p;
    }
    */

    Mandelbrot::init();
    
    puts("\ndone\n");
    panic();
}

