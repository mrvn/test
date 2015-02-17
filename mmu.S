#define PERIPHERAL_BASE 0x3F000000 // Raspberry Pi 2 Peripheral Base Address

#define GPBASE   0x200000 // 0x3F200000
#define GPFSEL1       0x4 // 0x3F200004
#define GPSET1       0x20 // 0x3F200020
#define GPCLR1       0x2C // 0x3F20002C

// Controls actuation of pull up/down to ALL GPIO pins.
#define GPPUD        0x94 // 0x3F200094
// Controls actuation of pull up/down for specific GPIO pin.
#define GPPUDCLK0    0x98 // 0x3F200098


#define UART0_BASE   (PERIPHERAL_BASE + GPBASE + 0x1000) // 0x3F201000
#define UART0_DR     0x00 // 0x3F201000
#define UART0_RSRECR 0x04 // 0x3F201004
#define UART0_FR     0x18 // 0x3F201018
#define UART0_ILPR   0x20 // 0x3F201020
#define UART0_IBRD   0x24 // 0x3F201024
#define UART0_FBRD   0x28 // 0x3F201028
#define UART0_LCRH   0x2C // 0x3F20102C
#define UART0_CR     0x30 // 0x3F201030
#define UART0_IFLS   0x34 // 0x3F201034
#define UART0_IMSC   0x38 // 0x3F201038
#define UART0_RIS    0x3C // 0x3F20103C
#define UART0_MIS    0x40 // 0x3F201040
#define UART0_ICR    0x44 // 0x3F201044
#define UART0_DMACR  0x48 // 0x3F201048
#define UART0_ITCR   0x80 // 0x3F201080
#define UART0_ITIP   0x84 // 0x3F201084
#define UART0_ITOP   0x88 // 0x3F201088
#define UART0_TDR    0x8C // 0x3F20108C

.section ".text.boot"
.global Start

Start:
        // Setup the stack.
        mov     sp, #0x8000
	
	// Initialize LED
	bl	led_init

	// blink 5 times, 1s interval
	mov	r0, #5
	mov	r1, #0x100000
	bl	blink

	// initialize uart
	bl	uart_init

	// say hello
	mov	r0, #'\n'
	bl	putc
	ldr	r0, =hello
	bl	puts
	
	// give up and panic
	b	panic

.section ".text"

// initialize uart
.global uart_init
uart_init:
	push	{r4, r5, r6, r7, lr}
	mov	r6,#PERIPHERAL_BASE
	orr	r6,#GPBASE		// 0x3F200000 GPBASE
	ldr	r4, =UART0_BASE		// 0x3F201000 UART0 Base

        // Disable UART0.
	mov	r5, #0
	str	r5, [r4, #UART0_CR]

        // Disable pull up/down for all GPIO pins & delay for 150 cycles.
	str	r5, [r6, #GPPUD]
	mov	r0, #150
	bl	delay

	// Disable pull up/down for pin 14,15 & delay for 150 cycles.
	mov	r7, #((1 << 14) | (1 << 15))
	str	r7, [r6, #GPPUDCLK0]
	mov	r0, #150
	bl	delay

        // Write 0 to GPPUDCLK0 to make it take effect.
	str	r5, [r6, #GPPUDCLK0]

	// select function 4 for pin 14, 15
	mov	r0,#PERIPHERAL_BASE
	orr	r0,#GPBASE		// R0 = GPBASE
	ldr	r1,[r0,#GPFSEL1]	// R1 = GPFSEL1
	mov	r2,#~(63 << 12)
	and	r1,r2			// &= ~(63 << 12)
	mov	r2,#(0x24 << 12)
	orr	r1,r2			// |= 24 << 12
	str	r1,[r0,#GPFSEL1]

        // Clear pending interrupts.
	ldr	r7, =0x7FF
	str	r7, [r4, #UART0_ICR]

        // Set integer & fractional part of baud rate.
        // Divider = UART_CLOCK/(16 * Baud)
        // Fraction part register = (Fractional part * 64) + 0.5
        // UART_CLOCK = 3000000; Baud = 115200.

        // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
        // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
	mov	r7, #1
	str	r7, [r4, #UART0_IBRD]
	mov	r7, #40
	str	r7, [r4, #UART0_FBRD]

        // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
	mov	r7, #((1 << 4) | (1 << 5) | (1 << 6))
	str	r7, [r4, #UART0_LCRH]

        // Mask all interrupts.
	ldr	r7, =((1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) \
	             | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10))
	str	r7, [r4, #UART0_IMSC]

        // Enable UART0, receive & transfer part of UART.
	ldr	r7, =((1 << 0) | (1 << 8) | (1 << 9))
	str	r7, [r4, #UART0_CR]
	
	pop	{r4, r5, r6, r7, lr}
	bx	lr
	
// store constants
constants:
.ltorg

// put char to uart
// r0 - char to put
.global putc
putc:
	ldr	r1, =UART0_BASE		// 0x3F201000 UART0 Base

        // wait for UART to become ready to transmit	
1:	
	ldr	r2, [r1, #UART0_FR]
	tst	r2, #(1 << 5)
	bne	1b

	// send char
	str	r0, [r1, #UART0_DR]

	bx	lr

// put string ti uart
// r0 - string to put
.global puts
puts:
	push	{r4, lr}
	mov	r4, r0
1:
	ldrb	r0, [r4]
	cmp	r0, #0
	beq	2f
	bl	putc
	add	r4, #1
	b	1b
2:
	pop	{r4, lr}
	bx	lr
	
// blink LED rapidly in panic
.global panic
panic:
	bl	led_on
	mov	r0,#0x10000
	bl	delay
	bl	led_off
	mov	r0,#0x10000
	bl	delay
	b	panic
	
// blink LED a few times
// r0 - number of blinks
// r1 - delay between blinks (#0x100000 = ~1s)
.global blink
blink:
	// save registers and move arguments
	push	{r4, r5, lr}
	mov	r4, r0
	mov	r5, r1
1:	
	bl	led_on
	mov	r0,r5
	bl	delay
	bl	led_off
	mov	r0,r5
	bl	delay
	subs	r4,#1
	bne	1b
	pop	{r4, r5, lr}
	bx	lr

//0b0000 0010 0100 0000 0000 0000
//0b0000 0100 0000 0000 0000 0000
// configure GPIO for LED
.global	led_init
led_init:
	mov	r0,#PERIPHERAL_BASE
	orr	r0,#GPBASE		// R0 = GPBASE
	ldr	r1,[r0,#GPFSEL1]	// R1 = GPFSEL1
	mov	r2,#~(7 << 18)
	and	r1,r2			// &= ~(7 << 18)
	mov	r2,#1
	orr	r1,r2,lsl #18		// |= 1 << 18
	str	r1,[r0,#GPFSEL1]
	bx	lr

// turn on LED
.global led_on
led_on:
	mov	r0,#PERIPHERAL_BASE
	orr	r0,#GPBASE		// R0 = GPBASE
	mov	r2,#1
	mov	r2,r2,lsl #15		// 1 << 15
	str	r2,[r0,#GPSET1]
	bx	lr

// turn off LED
.global led_off
led_off:
	mov	r0,#PERIPHERAL_BASE
	orr	r0,#GPBASE		// R0 = GPBASE
	mov	r2,#1
	mov	r2,r2,lsl #15		// 1 << 15
	str	r2,[r0,#GPCLR1]
	bx	lr

// sleep a bit
// r0 - number of loops to delay (#0x100000 = ~1s)
.global delay
delay:
	subs	r0, #1
	bne	delay
	bx	lr

.section ".data"

	.global	hello
hello:	.asciz "Hello\n"
	
	/****************************************************************
	 * Memory map
	 ****************************************************************
	 * [000] 0x00000000 - 0x000FFFFF kernel
	 * [001] 0x00100000 - 0x001FFFFF leaf_table
	 * [3F0] 0x3F000000 - 0x3F0FFFFF peripherals
	 * [3F1] 0x3F100000 - 0x3F1FFFFF peripherals
	 * [3F2] 0x3F200000 - 0x3F2FFFFF peripherals
	*/

	.balign 16384
	.globl page_table
page_table:
	.fill	16384, 1, 0

	.balign 4096
	.globl leaf_table
leaf_table:
	.fill	4096, 1, 0
