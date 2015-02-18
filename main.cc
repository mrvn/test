#include <stdint.h>

#define UNUSED(x) (void)x

extern "C" {
    void kernel_main(uint32_t r0, uint32_t model_id, void *atags);
    void delay(uint32_t);
}

// Raspberry Pi 2 Peripheral Base Address
#define PERIPHERAL_BASE 0x3F000000

#define REG(x) ((volatile uint32_t *)(PERIPHERAL_BASE + (x)))

/**********************************************************************
 * GPIO                                                               *
 **********************************************************************/
namespace GPIO {
    enum {
	GPIO_BASE = 0x200000, // 0x3F200000

	// function selector
	GPIO_FSEL0   = 0x00, // 0x3F200000
	GPIO_FSEL1   = 0x04, // 0x3F200004

	// set and clear pin output
	GPIO_SET1    = 0x20, // 0x3F200020
	GPIO_CLR1    = 0x2C, // 0x3F20002C

	// Controls actuation of pull up/down to ALL GPIO pins.
	GPIO_PUD     = 0x94, // 0x3F200094
	// Controls actuation of pull up/down for specific GPIO pin.
	GPIO_PUDCLK0 = 0x98, // 0x3F200098
    };
#define GPIO_REG(x) (REG(GPIO::GPIO_BASE + (x)))

    void set_function(uint32_t pin, uint32_t fn) {
	volatile uint32_t *fsel =
	    GPIO_REG((pin < 10) ? GPIO::GPIO_FSEL0 : GPIO::GPIO_FSEL1);
	uint32_t shift = (pin < 10) ? pin : (pin - 10);
	shift *= 3;
	uint32_t mask = ~(7 << shift);
	fn = fn << shift;
	*fsel = (*fsel & mask) | fn;
    }
}

/**********************************************************************
 * LED                                                                *
 **********************************************************************/
namespace LED {
    // configure GPIO pins for the LED
    void init(void) {
	// configure GPIO pin 15 for function 1
	GPIO::set_function(15, 1);
    }

    // turn LED on or off
    void set(bool state) {
	// set or clear bit 15
	*GPIO_REG(state ? GPIO::GPIO_SET1 : GPIO::GPIO_CLR1) = 1 << 15;
    }
}

/**********************************************************************
 * UART                                                               *
 **********************************************************************/
namespace UART {
    enum {
	UART0_BASE = GPIO::GPIO_BASE + 0x1000, // 0x3F201000

	UART0_DR     = 0x00, // 0x3F201000
	UART0_RSRECR = 0x04, // 0x3F201004
	UART0_FR     = 0x18, // 0x3F201018
	UART0_ILPR   = 0x20, // 0x3F201020
	UART0_IBRD   = 0x24, // 0x3F201024
	UART0_FBRD   = 0x28, // 0x3F201028
	UART0_LCRH   = 0x2C, // 0x3F20102C
	UART0_CR     = 0x30, // 0x3F201030
	UART0_IFLS   = 0x34, // 0x3F201034
	UART0_IMSC   = 0x38, // 0x3F201038
	UART0_RIS    = 0x3C, // 0x3F20103C
	UART0_MIS    = 0x40, // 0x3F201040
	UART0_ICR    = 0x44, // 0x3F201044
	UART0_DMACR  = 0x48, // 0x3F201048
	UART0_ITCR   = 0x80, // 0x3F201080
	UART0_ITIP   = 0x84, // 0x3F201084
	UART0_ITOP   = 0x88, // 0x3F201088
	UART0_TDR    = 0x8C, // 0x3F20108C
    };

#define UART_REG(x) (REG(UART::UART0_BASE + x))

    // initialize uart
    void init(void) {
        // Disable UART0.
	*UART_REG(UART0_CR) = 0;

        // Disable pull up/down for all GPIO pins & delay for 150 cycles.
	*GPIO_REG(GPIO::GPIO_PUD) = 0;
	delay(150);

	// Disable pull up/down for pin 14,15 & delay for 150 cycles.
	*GPIO_REG(GPIO::GPIO_PUDCLK0) = (1 << 14) | (1 << 15);
	delay(150);

        // Write 0 to GPPUDCLK0 to make it take effect.
	*GPIO_REG(GPIO::GPIO_PUDCLK0) = 0;

	// select function 4 for pin 14, 15
	GPIO::set_function(14, 4);
	GPIO::set_function(15, 4);

        // Clear pending interrupts.
	*UART_REG(UART0_ICR) = 0x7FF;

        // Set integer & fractional part of baud rate.
        // Divider = UART_CLOCK/(16 * Baud)
        // Fraction part register = (Fractional part * 64) + 0.5
        // UART_CLOCK = 3000000; Baud = 115200.

        // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
        // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
	*UART_REG(UART0_IBRD) = 1;
	*UART_REG(UART0_FBRD) = 40;

        // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
	*UART_REG(UART0_LCRH) = (1 << 4) | (1 << 5) | (1 << 6);

        // Mask all interrupts.
	*UART_REG(UART0_IMSC) = (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6)
	                      | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10);

        // Enable UART0, receive & transfer part of UART.
	*UART_REG(UART0_CR) = (1 << 0) | (1 << 8) | (1 << 9);
    }

    void put(uint8_t x) {
	// wait for space in the send buffer
	while(*UART_REG(UART0_FR) & (1 << 5)) {	}
	*UART_REG(UART0_DR) = x;
    }

    uint8_t get(void) {
	// wait for something in the receive buffer
	while(*UART_REG(UART0_FR) & (1 << 4)) { }
	return *UART_REG(UART0_DR);
    }
}

void putc(char c) {
    UART::put(c);
}

void puts(const char *str) {
    while(*str) putc(*str++);
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

    void init(void) {
	uint32_t base;
	// initialize page_table
	// 4MB of kernel memory
	for (base = 0; base < 4; base++) {
	    // section descriptor (1 MB)
#ifdef CACHED_TLB
	    // outer and inner write back, write allocate, not shareable (fast
	    // but unsafe)
	    page_table[base] = base << 20 | 0x0140E;
	    // outer and inner write back, write allocate, shareable (fast but
	    // unsafe)
	    //page_table[base] = base << 20 | 0x1140E;
#else
	    // outer and inner write through, no write allocate, shareable
	    // (safe but slower)
	    page_table[base] = base << 20 | 0x1040A;
#endif
	}

	// one second level page tabel (leaf table) at 0x00400000
	page_table[base++] = (intptr_t)leaf_table | 0x1;

	// unused up to 0x3F000000
	for (; base < 1024 - 16; base++) {
	    page_table[base] = 0;
	}

	// 16 MB peripherals at 0x3F000000
	for (; base < 1024; base++) {
	    // shared device, never execute
	    page_table[base] = base << 20 | 0x10416;
	}

	// 3G unused
	for (; base < 4096; base++) {
	    page_table[base] = 0;
	}

	// initialize leaf_table
	for (base = 0; base < 256; base++) {
	    leaf_table[base] = 0;
	}

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
	panic_delay *= 0x200;
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
	// 0b0101 0100 0111
	//leaf_table[slot] = phys_addr | 0x547;

	// 0b0101 0101 0111 need AP[0] despite AFE==1
	leaf_table[slot] = phys_addr | 0x557;
	tripple_barrier();
    }

    void unmap(uint32_t slot) {
	tripple_barrier();
	// remove from leaf_table
	leaf_table[slot] = 0;
	tripple_barrier();
	// invalidate page
	page *virt = &((page *)0x400000)[slot];
	asm volatile("mcr p15, 0, %[ptr], c8, c7, 1"::[ptr]"r"(virt));
	tripple_barrier();
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

    MMU::init();

    puts("mapping and cleaning memory");
    MMU::page *p = (MMU::page*)MMU::_mem_start;
    for(uint32_t slot = 0; slot < 256; ++slot) {
	if (slot % 32 == 0) putc('\n');
	putc('.');
	blink(panic_delay);
	MMU::map(slot, (intptr_t)p);
	MMU::page *virt = &((MMU::page *)0x400000)[slot];
	// writing to page faults
	virt->data[0] = 0;
	MMU::unmap(slot);
	++p;
    }

    puts("\ndone\n");
    panic();
}

