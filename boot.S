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

// core wakes up here, set stack and call core_main
.global core_wakeup
core_wakeup:
	// get core ID
	mrc	p15,0,r0,c0,c0,5
	and	r0, #3

	// get stack
	lsl	r0, #2
	ldr	r1, =stacks
	ldr	sp, [r1, r0]
	
	// Call kernel_main
        ldr     r3, =core_main
        blx     r3

	// busy loop
1:	b	1b
	
.section ".data"
	.align 2
	.global stacks
stacks:	.word stack0
	.word stack1
	.word stack2
	.word stack3
	
.section ".bss"
	.align 12
	.fill 16384, 1, 0
	.global stack0
stack0:	
	.fill 16384, 1, 0
	.global stack1
stack1:
	.fill 16384, 1, 0
	.global stack2
stack2:
	.fill 16384, 1, 0
	.global stack3
stack3:
