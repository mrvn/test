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

void panic() {
    while(true) {
	blink(0x10000);
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
    
    panic();
}
