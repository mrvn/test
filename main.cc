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
#include "gpio.h"
#include "framebuffer.h"
#include "font.h"
#include "peripherals.h"
#include "delay.h"

#define UNUSED(x) (void)x

extern "C" {
    void kernel_main(uint32_t r0, uint32_t model_id, void *atags);
}

void putc(char c) {
    UART::put(c);
}

void puts(const char *str) {
    while(*str) putc(*str++);
}

void put_uint32(uint32_t x) {
    static const char HEX[] = "0123456789ABCDEF";
    putc('0');
    putc('x');
    for(int i = 28; i >= 0; i -= 4) {
	putc(HEX[(x >> i) % 16]);
    }
}

void blink(uint32_t time) {
	LED::set(true);
	delay(time);
	LED::set(false);
	delay(time);
}

static uint32_t panic_delay = 0x10000;

void panic() {
    while(true) {
	blink(panic_delay);
    }
}


/**********************************************************************
 * MMU                                                                *
 **********************************************************************/
namespace MMU {
#define CACHED_TLB
//#undef CACHED_TLB

    static volatile __attribute__ ((aligned (0x4000))) uint32_t page_table[4096];
    static volatile __attribute__ ((aligned (0x400))) uint32_t leaf_table[256];

    struct page {
	uint8_t data[4096];
    };

    extern "C" {
	extern page _mem_start[];
	extern page _mem_end[];
    }

    void init_page_table() {
	uint32_t base;
	// initialize page_table
	// 1024MB - 16MB of kernel memory (some belongs to the VC)
	// default: 880 MB ARM ram, 128MB VC
	for (base = 0; base < 880; base++) {
	    // section descriptor (1 MB)
#ifdef CACHED_TLB
	    // outer and inner write back, write allocate, not shareable (fast
	    // but unsafe)
	    // page_table[base] = base << 20 | 0x0140E;
	    // outer and inner write back, write allocate, shareable (fast but
	    // unsafe)
	    page_table[base] = base << 20 | 0x1140E;
#else
	    // outer and inner write through, no write allocate, shareable
	    // (safe but slower)
	    page_table[base] = base << 20 | 0x1040A;
#endif
	}

	// VC ram up to 0x3F000000
	for (base = 0; base < 1024 - 16; base++) {
	    // section descriptor (1 MB)
	    // outer and inner write through, no write allocate, shareable
	    // page_table[base] = base << 20 | 0x1040A;
	    // outer write back, write allocate
	    // inner write through, no write allocate, shareable
	    page_table[base] = base << 20 | 0x1540A;
	}

	// unused up to 0x3F000000
	for (; base < 1024 - 16; base++) {
	    page_table[base] = 0;
	}

	// 16 MB peripherals at 0x3F000000
	for (; base < 1024; base++) {
	    // shared device, never execute
	    page_table[base] = base << 20 | 0x10416;
	}

	// 1 MB mailboxes
	// shared device, never execute
	page_table[base] = base << 20 | 0x10416;
	++base;
	
	// unused up to 0x7FFFFFFF
	for (; base < 2048; base++) {
	    page_table[base] = 0;
	}

	// one second level page tabel (leaf table) at 0x80000000
	page_table[base++] = (intptr_t)leaf_table | 0x1;

	// 2047MB unused (rest of address space)
	for (; base < 4096; base++) {
	    page_table[base] = 0;
	}

	// initialize leaf_table
	for (base = 0; base < 256; base++) {
	    leaf_table[base] = 0;
	}
    }
    
    void init() {
	// set SMP bit in ACTLR
	uint32_t auxctrl;
	asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (auxctrl));
	auxctrl |= 1 << 6;
	asm volatile ("mcr p15, 0, %0, c1, c0,  1" :: "r" (auxctrl));

        // setup domains (CP15 c3)
	// Write Domain Access Control Register
        // use access permissions from TLB entry
	asm volatile ("mcr     p15, 0, %0, c3, c0, 0" :: "r" (0x55555555));

	// set domain 0 to client
	asm volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r" (1));

	// always use TTBR0
	asm volatile ("mcr p15, 0, %0, c2, c0, 2" :: "r" (0));

#ifdef CACHED_TLB
	// set TTBR0 (page table walk inner and outer write-back,
	// write-allocate, cacheable, shareable memory)
	asm volatile ("mcr p15, 0, %0, c2, c0, 0"
		      :: "r" (0b1001010 | (unsigned) &page_table));
	// set TTBR0 (page table walk inner and outer write-back,
	// write-allocate, cacheable, non-shareable memory)
	//asm volatile ("mcr p15, 0, %0, c2, c0, 0"
	//	      :: "r" (0b1101010 | (unsigned) &page_table));
#else
	// set TTBR0 (page table walk inner and outer non-cacheable,
	// non-shareable memory)
	asm volatile ("mcr p15, 0, %0, c2, c0, 0"
		      :: "r" (0 | (unsigned) &page_table));
#endif
	asm volatile ("isb" ::: "memory");

	/* SCTLR
	 * Bit 31: SBZ     reserved
	 * Bit 30: TE      Thumb Exception enable (0 - take in ARM state)
	 * Bit 29: AFE     Access flag enable (1 - simplified model)
	 * Bit 28: TRE     TEX remap enable (0 - no TEX remapping)
	 * Bit 27: NMFI    Non-Maskable FIQ (read-only)
	 * Bit 26: 0       reserved
	 * Bit 25: EE      Exception Endianness (0 - little-endian)
	 * Bit 24: VE      Interrupt Vectors Enable (0 - use vector table)
	 * Bit 23: 1       reserved
	 * Bit 22: 1/U     (alignment model)
	 * Bit 21: FI      Fast interrupts (probably read-only)
	 * Bit 20: UWXN    (Virtualization extension)
	 * Bit 19: WXN     (Virtualization extension)
	 * Bit 18: 1       reserved
	 * Bit 17: HA      Hardware access flag enable (0 - enable)
	 * Bit 16: 1       reserved
	 * Bit 15: 0       reserved
	 * Bit 14: RR      Round Robin select (0 - normal replacement strategy)
	 * Bit 13: V       Vectors bit (0 - remapped base address)
	 * Bit 12: I       Instruction cache enable (1 - enable)
	 * Bit 11: Z       Branch prediction enable (1 - enable)
	 * Bit 10: SW      SWP/SWPB enable (maybe RAZ/WI)
	 * Bit 09: 0       reserved
	 * Bit 08: 0       reserved
	 * Bit 07: 0       endian support / RAZ/SBZP
	 * Bit 06: 1       reserved
	 * Bit 05: CP15BEN DMB/DSB/ISB enable (1 - enable)
	 * Bit 04: 1       reserved
	 * Bit 03: 1       reserved
	 * Bit 02: C       Cache enable (1 - data and unified caches enabled)
	 * Bit 01: A       Alignment check enable (1 - fault when unaligned)
	 * Bit 00: M       MMU enable (1 - enable)
	 */
	
	// enable MMU, caches and branch prediction in SCTLR
	uint32_t mode;
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (mode));
	// mask: 0b0111 0011 0000 0010 0111 1000 0010 0111
	// bits: 0b0010 0000 0000 0000 0001 1000 0010 0111
#ifdef CACHED_TLB
	mode &= 0x73027827;
	mode |= 0x20001827;
#else
	// no caches
	mode &= 0x73027827;
	mode |= 0x20000023;
#endif
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r" (mode) : "memory");

	// instruction cache makes delay way faster, slow panic down
#ifdef CACHED_TLB
	panic_delay = 0x2000000;
#endif
    }

    void tripple_barrier() {
	asm volatile ("isb" ::: "memory");
	asm volatile ("dmb" ::: "memory");
	asm volatile ("dsb" ::: "memory");
    }
    
    void map(uint32_t slot, uint32_t phys_addr) {
	tripple_barrier();

	/* second-level descriptor format (small page)
	 * Bit 31: small page base address
	 * ...
	 * Bit 12: small page base address
	 * Bit 11: nG      not global (0 - global)
	 * Bit 10: S       shareable (1 - shareable)
	 * Bit 09: AP[2]   0 (read/write)
	 * Bit 08: TEX[2]  1
	 * Bit 07: TEX[1]  0
	 * Bit 06: TEX[0]  1
	 * Bit 05: AP[1]   0 (only kernel)
	 * Bit 04: AP[0]   0 - access flag
	 * Bit 03: C       0
	 * Bit 02: B       1
	 * Bit 01: 1       1 (small page)
	 * Bit 00: XN      execute never (1 - not executable)
	 *
	 * TEX C B | Description               | Memory type      | Shareable
	 * 000 0 0 | Strongly-ordered          | Strongly-ordered | Shareable
	 * 000 0 1 | Shareable Device          | Device           | Shareable
	 * 000 1 0 | Outer/Inner Write-Through | Normal           | S bit
	 *         | no Write-Allocate         |                  |
	 * 000 1 1 | Outer/inner Write-Back    | Normal           | S bit
	 *         | no Write-Allocate         |                  |
	 * 001 0 0 | Outer/Inner Non-cacheable | Normal           | S bit
	 * 001 0 1 | reserved                  | -                | -
	 * 001 1 0 | IMPL                      | IMPL             | IMPL
	 * 001 1 1 | Outer/inner Write-Back    | Normal           | S bit
	 *         | Write-Allocate            |                  |
	 * 010 0 0 | Non-shareable Device      | Device           | Non-share.
	 * 010 0 1 | reserved                  | -                | -
	 * 010 1 x | reserved                  | -                | -
	 * 011 x x | reserved                  | -                | -
	 * 1BB A A | Cacheable Memory          | Normal           | S bit
	 *         | AA inner / BB outer       |                  |
	 *
	 * Inner/Outer cache attribute encoding
	 * 00 non-cacheable
	 * 01 Write-Back, Write-Allocate
	 * 10 Write-Through, no Write Allocate
	 * 11 Write-Back, no Write Allocate
	 *
	 * AP[2:1] simplified access permission model
	 * 00 read/write, only kernel
	 * 01 read/write, all
	 * 10 read-only, only kernel
	 * 11 read-only, all
	 */

	// outer and inner write back, write allocate, shareable
	// set accessed bit or the first access gives a fault
	// RPi/RPi2 do not have hardware support for accessed
	// 0b0101 0101 0111
	leaf_table[slot] = phys_addr | 0x557;
	tripple_barrier();
    }

    void unmap(uint32_t slot) {
	tripple_barrier();
	// remove from leaf_table
	leaf_table[slot] = 0;
	tripple_barrier();
	// invalidate page
	page *virt = &((page *)0x80000000)[slot];
	asm volatile("mcr p15, 0, %[ptr], c8, c7, 1"::[ptr]"r"(virt));
	tripple_barrier();
    }
}

/**********************************************************************
 * FPU                                                                *
 **********************************************************************/
namespace FPU {
    // Enable Advanced SIMD & Vector Floating Point Calculations (NEON MPE)
    void init(void) {
	uint32_t reg;
	// read Access Control Register
	asm volatile("mrc p15,0,%0,c1,c0,2" : "=r" (reg));
	reg |= 0x300000 + 0xC00000; // Enable Single & Double Precision
	// write Access Control Register
	asm volatile("mcr p15,0,%0,c1,c0,2" :: "r" (reg));
	// enable VFP
	asm volatile("vmsr fpexc,%0" :: "r" (0x40000000));
    }
}

/**********************************************************************
 * SMP                                                                *
 **********************************************************************/
namespace SMP {
    // Setup SMP (Boot Offset = $4000008C + ($10 * Core), Core = 1..3)
    enum {
	CORE_BASE = 0x4000008C,

	Core1Boot = 0x10, // Core 1 Boot Offset
	Core2Boot = 0x20, // Core 2 Boot Offset
	Core3Boot = 0x30, // Core 3 Boot Offset
    };

    typedef void (*fn)(void);

#define CORE_REG(x) ((volatile fn *)(CORE_BASE + (x)))

    volatile uint32_t count[4] = { 0 };

    // Multiprocessor Affinity Register (MPIDR)
    uint32_t get_mpidr(void) {
	uint32_t mpidr;
	asm volatile ("mrc p15,0,%0,c0,c0,5" : "=r" (mpidr));
	return mpidr;
    }

    extern "C" {
	void core_wakeup(void);
	void core_main(void);
    }
    
    void core_main(void) {
	puts("core is up: MPIDR = ");
	put_uint32(get_mpidr());
	putc('\n');
	MMU::init();
	puts("core is virtual\n");
	FPU::init();
	puts("core is floating\n");

	int id = get_mpidr() & 3;
	while(true) { count[id]++; }
    }
    
    void init() {
	puts("starting core 1\n");
	blink(panic_delay * 0x10);
	*CORE_REG(Core1Boot) = core_wakeup;
	blink(panic_delay * 0x10);
	puts("started core 1\n");
	blink(panic_delay * 0x10);

    	puts("starting core 2\n");
	blink(panic_delay * 0x10);
	*CORE_REG(Core2Boot) = core_wakeup;
	blink(panic_delay * 0x10);
	puts("started core 2\n");
	blink(panic_delay * 0x10);

    	puts("starting core 3\n");
	blink(panic_delay * 0x10);
	*CORE_REG(Core3Boot) = core_wakeup;
	blink(panic_delay * 0x10);
	puts("started core 3\n");
	blink(panic_delay * 0x10);
    }

}

/*
 * Clean and invalidate entire cache
 * Flush pending writes to main memory
 * Remove all data in data cache
 */
static inline void flush_cache(void) {
    // RPi
    // asm volatile("mcr p15, #0, %[zero], c7, c14, #0"
    //              : : [zero]"r"(0));
    // RPi2
    // FIXME
}

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

/**********************************************************************
 * Framebuffer                                                        *
 **********************************************************************/
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

/**********************************************************************
 * Mandelbrot                                                         *
 **********************************************************************/
namespace Mandelbrot {
    struct Params {
	double xmin, xmax, ymin, ymax;
	int nmax;
    } params = {
	-2.5, 1.5, -1.25, 1.25,
	64
    };

    bool operator !=(const Framebuffer::Pixel& p, const Framebuffer::Pixel& q) {
	return (p.red != q.red) || (p.green != q.green)
	    || (p.blue != q.blue) || (p.alpha != q.alpha);
    }
    
    bool mandelbrot(uint32_t stepx, uint32_t stepy) {
	const double bailout = 16.0;
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
	// compute missing bits
	for(uint32_t v = 0; v < Framebuffer::fb.height; v += stepy) {
	    if (UART::poll()) return false;
	    double y0 = params.ymin + v * (params.ymax - params.ymin) / Framebuffer::fb.height;
	    for(uint32_t u = 0; u < Framebuffer::fb.width; u += stepx) {
		Framebuffer::Pixel *p = (Framebuffer::Pixel*)(Framebuffer::fb.base + u * sizeof(Framebuffer::Pixel) + v * Framebuffer::fb.pitch);
		if (p->alpha == 0xff) continue;
		double x0 = params.xmin + u * (params.xmax - params.xmin) / Framebuffer::fb.width;
		int n = 0;
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
	}
	return true;
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
    SMP::init();

    for (int i = 0; i < 10; ++i) {
	puts("counts = ");
	put_uint32(SMP::count[1]);
	putc(' ');
	put_uint32(SMP::count[2]);
	putc(' ');
	put_uint32(SMP::count[3]);
	putc('\n');
	blink(panic_delay);
    }

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

