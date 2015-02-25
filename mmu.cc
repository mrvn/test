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
 * MMU support for the Raspberry Pi
 */

#include <stdint.h>
#include "mmu.h"
#include "barriers.h"

extern uint32_t panic_delay;

namespace MMU {
    static volatile __attribute__ ((aligned (0x4000))) uint32_t page_table[4096];
    static volatile __attribute__ ((aligned (0x400))) uint32_t leaf_table[256];

    struct page {
	uint8_t data[4096];
    };

    extern "C" {
	extern page _mem_start[];
	extern page _mem_end[];
    }

    void init_page_table(void) {
	uint32_t base;
	// initialize page_table
	// 1024MB - 16MB of kernel memory (some belongs to the VC)
	// default: 880 MB ARM ram, 128MB VC
	for (base = 0; base < 880; base++) {
	    // section descriptor (1 MB)
	    // outer and inner write back, write allocate, shareable (fast but
	    // unsafe)
	    page_table[base] = base << 20 | 0x1140E;
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
    
    void init(void) {
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

	// set TTBR0 (page table walk inner and outer write-back,
	// write-allocate, cacheable, shareable memory)
	asm volatile ("mcr p15, 0, %0, c2, c0, 0"
		      :: "r" (0b1001010 | (unsigned) &page_table));
	instruction_barrier();

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
	mode &= 0x73027827;
	mode |= 0x20001827;
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r" (mode) : "memory");

	// instruction cache makes delay way faster, slow panic down
	panic_delay = 0x2000000;
    }

    void map(uint32_t slot, uint32_t phys_addr) {
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
	data_memory_barrier();
    }

    void unmap(uint32_t slot) {
	// remove from leaf_table
	leaf_table[slot] = 0;
	data_memory_barrier();
	// invalidate page
	page *virt = &((page *)0x80000000)[slot];
	asm volatile("mcr p15, 0, %[ptr], c8, c7, 1"::[ptr]"r"(virt));
	instruction_barrier();
    }
}

