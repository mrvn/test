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

.section ".data"

	/****************************************************************
	 * Memory map
	 ****************************************************************
	 * [000] 0x00000000 - 0x000FFFFF kernel
	 * [001] 0x00100000 - 0x001FFFFF leaf_table
	 * [3F0] 0x3F000000 - 0x3F0FFFFF peripherals
	 * [3F1] 0x3F100000 - 0x3F1FFFFF peripherals
	 * [3F2] 0x3F200000 - 0x3F2FFFFF peripherals
	*/

.section ".data.leaf_table"
	.balign 1024
	.globl leaf_table
leaf_table:
	// 4K small page (non cacheable)
	// addr(20) nG S AP[2] TEX(3) AP[1:0] C B 1 XN
	.word	page + 0x00000102
	.fill	255, 4, 0

.section ".data.page_table"
	.balign 16384
	.globl page_table
page_table:
	// 1MB kernel section (non cacheable)
	// addr(12) NS 0 nG S AP[2] TEX(3) AP[1:0] IMPL Domain(4) XN C B 1 PXN
	.word	0x00004002
	// 1MB leaf table (non cacheable)
	// addr(22) IMPL DOM(4) 0 NS 0 0 1
	.word	leaf_table + 0x00004001
	// unmapped till 0x3F000000
	.fill	1006, 4, 0
	// 3MB peripherals (shareable device)
	// addr(12) NS 0 nG S AP[2] TEX(3) AP[1:0] IMPL Domain(4) XN C B 1 PXN
	.word	0x3F000006
	.word	0x3F100006
	.word	0x3F200006
	// unmapped till end
	.fill	3085, 4, 0

.section ".bss.page"
	.balign 4096
	.globl page
page:
	.fill	4096, 1, 0

	.balign 4096
	.globl page2
page2:
	.fill	4096, 1, 0

// no TEX remapping
// TEX[2:0] C B
// 1BB      A A       AA = Inner attribute, BB = Outer attribute
// 00 - Non-cacheable
// 01 - write-back, write-allocate
// 10 - write-through, no write-allocate
// 11 - write-back, no write-allocate

// set SCTLR.AFE (simpler acess model)
