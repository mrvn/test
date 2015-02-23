.section ".text.boot"
.global Start

Start:
	// r0 = 0
	// r1 = model ID (0xC42)
	// r2 = ATAGs (0x100)
	
        // Setup the stack
        mov     sp, #0x8000

	// clear out BSS section
        ldr     r4, =_bss_start
        ldr     r9, =_bss_end
        mov     r5, #0
        mov     r6, #0
        mov     r7, #0
        mov     r8, #0
	b	2f // catch the case of BSS being 0 byte
        // store multiple at r4
1:        stmia   r4!, {r5-r8}

        // If we're still below bss_end, loop again
2:	cmp     r4, r9
        blo     1b

	// Call kernel_main
        ldr     r3, =kernel_main
        blx     r3

        // halt
halt:
        wfe
        b       halt

/*
1:	// compute offset of .init_array (same as offset of '1')
	adr	r0, 1b
	ldr	r1, =1b
	sub	r0, r1
	bl	put_uint32

tmp1:	ldr	r0, [pc, #tmp2 - tmp1 - 8]
	ldr	r0, [pc, r0]
tmp2:	.word	delay - tmp1

*/

// store constants
text.boot.constants:
.ltorg

.section ".text"

// sleep a bit
// r0 - number of loops to delay (#0x100000 = ~1s)
.global delay
delay:
	subs	r0, #1
	bne	delay
	bx	lr
