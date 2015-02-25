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

#include <stdint.h>
#include "smp.h"
#include "stdio.h"
#include "mmu.h"
#include "fpu.h"

namespace SMP {
    // Setup SMP (Boot Offset = $4000008C + ($10 * Core), Core = 1..3)
    enum {
	CORE_BASE = 0x4000008C,

	Core0Boot = 0x00, // Core 0 Boot Offset
	Core1Boot = 0x10, // Core 1 Boot Offset
	Core2Boot = 0x20, // Core 2 Boot Offset
	Core3Boot = 0x30, // Core 3 Boot Offset
    };

    typedef void (*fn)(void);

    constexpr volatile fn * mailbox(int core) {
	return (volatile fn *)(CORE_BASE + core * 0x10);
    }

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

    static start_fn_t start_fn;
    void *start_arg;
    volatile bool started;

    void core_main(void) {
	puts("core is up: MPIDR = ");
	put_uint32(get_mpidr());
	putc('\n');
	MMU::init();
	puts("core is virtual\n");
	FPU::init();
	puts("core is floating\n");
	
	start_fn_t temp_fn = start_fn;
	void *temp_arg = start_arg;
	started = true;
	temp_fn(temp_arg);
	// FIXME: make restartable?
	while(true) { }
    }

    void start_core(int core, start_fn_t start, void *arg) {
	start_fn = start;
	start_arg = arg;
	puts("starting core ");
	putc("0123"[core]);
	putc('\n');
	started = false;
	*mailbox(core) = core_wakeup;
	while(!started) { }
	puts("started core ");
	putc("0123"[core]);
	putc('\n');
    }
}

