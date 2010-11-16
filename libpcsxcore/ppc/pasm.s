

#define OLD_REGISTER_OFFSET	(19*4)
#define SP_SIZE			(OLD_REGISTER_OFFSET+4+8)

/*asm void recRun(register void (*func)(), register u32 hw1, register u32 hw2)*/
        .text
        .align  4
        .globl  recRun
recRun:
	/* prologue code */
	mflr	r0
	stmw	r13, -(32-13)*4(r1)
	stw		r0, 4(r1)
	stwu	r1, -((32-13)*4+8)(r1)
	
	/* execute code */
	mtctr	r3
	mr	r31, r4
	mr	r30, r5
	bctrl
/*
}
asm void returnPC()
{*/
        .text
        .align  4
        .globl  returnPC
returnPC:
	// end code
	lwz		r0, (32-13)*4+8+4(r1)
	addi	r1, r1, (32-13)*4+8
	mtlr	r0
	lmw		r13, -(32-13)*4(r1)
	blr
//}*/

// Memory functions that only works with a linear memory

        .text
        .align  4
        .globl  dynMemRead8
dynMemRead8:
// assumes that memory pointer is in r30
	addis    r2,r3,-0x1f80
	srwi.     r4,r2,16
	bne+     .norm8
	cmplwi   r2,0x1000
	blt-     .norm8
	b        psxHwRead8
.norm8:
	clrlwi   r5,r3,3
	lbzx     r3,r5,r30
	blr

        .text
        .align  4
        .globl  dynMemRead16
dynMemRead16:
// assumes that memory pointer is in r30
	addis    r2,r3,-0x1f80
	srwi.     r4,r2,16
	bne+     .norm16
	cmplwi   r2,0x1000
	blt-     .norm16
	b        psxHwRead16
.norm16:
	clrlwi   r5,r3,3
	lhbrx    r3,r5,r30
	blr

        .text
        .align  4
        .globl  dynMemRead32
dynMemRead32:
// assumes that memory pointer is in r30
	addis    r2,r3,-0x1f80
	srwi.     r4,r2,16
	bne+     .norm32
	cmplwi   r2,0x1000
	blt-     .norm32
	b        psxHwRead32
.norm32:
	clrlwi   r5,r3,3
	lwbrx    r3,r5,r30
	blr

/*
	N P Z
	0 0 0 X
-	0 0 1 X
	1 0 0 X
	1 0 1 X

P | (!N & Z)
P | !(N | !Z)
*/

        .text
        .align  4
        .globl  dynMemWrite32
dynMemWrite32:
// assumes that memory pointer is in r30
	addis    r2,r3,-0x1f80
	srwi.    r5,r2,16
	bne+     .normw32
	cmplwi   r2,0x1000
	blt      .normw32
	b        psxHwWrite32
.normw32:
	mtcrf    0xFF, r3
	clrlwi   r5,r3,3
	crandc   0, 2, 0
	cror     2, 1, 0
	bne+     .okw32
	// write test
	li			r2,0x0130
	addis    r2,r2,0xfffe
	cmplw    r3,r2
	bnelr
.okw32:
	stwbrx   r4,r5,r30
	blr

