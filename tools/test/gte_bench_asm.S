#include "arm_features.h"

.macro hazard1 @ 22
	vst1.32   {d0}, [sp, :64]
	@vstr      d0, [sp, #0]
	ldr       r3, [sp, #0]  @ #-4; #16 - no hazard
.endm

.macro hazard2 @ 22
	vstr      d0, [sp, #0]
	ldr       r3, [sp, #0]
.endm

.macro hazard3 @ 2
        add       r2, r1
        add       r1, r3, r4
        add       r2, r1
        add       r1, r3, r4
.endm

.macro stall1 @ 6 (regardless of add, r1/r6); 7 r2; 8 r7; 9 r3; 10 r8
        smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
        smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
        smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
        add r1, r1
.endm

.macro stall2 @ 7; 8@r2; 10@r3
        smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
        smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
        smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
        lsr      r1, #12
        lsr      r2, #12
        lsr      r3, #12
.endm

.macro stall3 @ 12; can dual-issue
    smlalbb  r1, r6, r9,  r4
    ldr r8, [sp]
    smlaltb  r2, r7, r10, r4
    add r5, r5
    smlalbb  r3, r8, r12, r4
    sub r5, #1
    smlaltt  r1, r6, r9,  r4
    add r5, #1
    smlalbt  r2, r7, r11, r4
    add r5, r5, lsl r4
    smlaltt  r3, r8, r12, r4
    clz r5, r5
.endm

@ gteRTPS_sf1lm0c        278  @ +mac123 flags
@ gteRTPS_sf1lm0_nfc     165
@ gteRTPS_nf_arm_old     175  @ wrong results, 158 w/DIVIDE
@ gteRTPS_neon_old       173  @ wrong results
@ gteRTPS_sf1lm0_arm     130
@ gteRTPS_sf1lm0_nf_arm  119
@ gteRTPS_sf1lm0_neon    137
@ gteRTPS_sf1lm0_nf_neon 119

@ gteRTPT_sf1lm0c        867
@ gteRTPT_sf1lm0_nfc     524
@ gteRTPT_nf_arm_old     471  @ wrong results
@ gteRTPT_neon_old       288  @ wrong results
@ gteRTPT_sf1lm0_arm     276
@ gteRTPT_sf1lm0_nf_arm  247
@ gteRTPT_sf1lm0_neon    240
@ gteRTPT_sf1lm0_nf_neon 225

@ gteMVMVA_part_neon              60
@  +gteMACtoIR_flags_neon         76
@ gteMVMVA_part_arm_old
@  +gteMACtoIR_lm0                88
@ gteMVMVA_part_cv3sh12_arm_old+  65
@ gteMVMVA_part_sf0_arm+          49
@ gteMVMVA_part_sf1_arm+          54
@ gteMVMVA_part_sf0cv3_arm+       42
@ gteMVMVA_part_sf1cv3_arm+       50

@ gteMACtoIR_lm0/1        21->18

.align 4

.globl atest
atest:
	push      {r4-r11,lr}
	mov       r3, sp
	sub       sp, #0x400  @ 1k alignment helps cortex-a8 somehow
	bic       sp, #0x0ff
	bic       sp, #0x300
        ldr       r1, [sp]       @ cache
        ldr       r2, [sp, #-8]      @ affects benchmarks?
        add       sp, #(4*9+12)  @ rtpt reg+scratch space
	str       sp, [r0]
        str       r3, [sp, #0x3c0]
	mov       r1, #1
	str       r1, [sp]
	str       r1, [sp, #4*(32+5)]
	str       r1, [sp, #4*(32+6)]
	str       r1, [sp, #4*(32+7)]
	ldr       r2, [sp, #4*(32+31)]  @ cache gteFLAG
	ldr       r3, [sp, #4*17]       @ cache gteSZ
	ldr       r1, =table
	str       r1, [sp, #4*(64)]
        mov       r6, sp         @ MVMVA mx
        add       r7, sp, #8*3   @ MVMVA cv
        ldr       r2, [r7]
	vmov.i32  d0, #1
	mov       r0, sp
        nop                      @ affects branch preductor?? dual issue??
	mrc       p15, 0, r11, c9, c13, 0
	bl        gteRTPS_sf1lm0_arm
	@bl        gteRTPT_sf1lm0_nf_neon
	@bl        gteMVMVA_part_sf1_arm
	@bl        gteMACtoIR_lm1
	mrc       p15, 0, r0, c9, c13, 0
	sub       r0, r0, r11
	sub       r0, #53
        ldr       sp, [sp, #0x3c0]
	pop       {r4-r11,pc}
.pool

.globl gteRTPS_sf1lm0
.globl gteRTPT_sf1lm0
.globl gteMVMVA_generic
gteRTPS_sf1lm0:
gteRTPT_sf1lm0:
gteMVMVA_generic:
	bl abort

	.align	2
	.syntax unified
	.arm
	.fpu neon
	.type	gteRTPS_sf1lm0c, %function
gteRTPS_sf1lm0c:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	push	{r4, r5, r6, r7, r8, r9, r10, lr}
	mov	r5, #0
	ldrsh	lr, [r0]
	mov	r4, r0
	ldrh	r3, [r0, #128]
	ldr	r7, [r0, #148]
	ldrsh	r1, [r0, #2]
	smulbb	r3, r3, lr
	ldrsh	ip, [r0, #4]
	ldrh	r6, [r0, #130]
	ldrh	r0, [r0, #132]
	smulbb	r6, r6, r1
	asr	r2, r3, #31
	smulbb	r0, r0, ip
	lsl	r2, r2, #20
	orr	r2, r2, r3, lsr #12
	lsl	r3, r3, #20
	adds	r3, r5, r3
	adcs	r7, r7, r2
	movvs	r5, #1
	ands	r2, r5, r7, lsr #31
	movne	r5, #1073741824
	bne	.L2885
	cmp	r5, #0
	mvnne	r5, r7
	lsrne	r5, r5, #31
	lslne	r5, r5, #27
.L2885:
	asr	r2, r6, #31
	lsl	r2, r2, #20
	orr	r2, r2, r6, lsr #12
	lsl	r6, r6, #20
	adds	r6, r3, r6
	adcs	r3, r7, r2
	mov	r2, #0
	movvs	r2, #1
	ands	r7, r2, r3, lsr #31
	orrne	r5, r5, #1073741824
	bne	.L2889
	cmp	r2, #0
	mvnne	r2, r3
	lsrne	r2, r2, #31
	orrne	r5, r5, r2, lsl #27
.L2889:
	asr	r2, r0, #31
	lsl	r2, r2, #20
	orr	r2, r2, r0, lsr #12
	lsl	r0, r0, #20
	adds	r0, r6, r0
	adcs	r3, r3, r2
	mov	r2, #0
	movvs	r2, #1
	ands	r0, r2, r3, lsr #31
	orrne	r5, r5, #1073741824
	bne	.L2893
	cmp	r2, #0
	mvnne	r2, r3
	lsrne	r2, r2, #31
	orrne	r5, r5, r2, lsl #27
.L2893:
	ldrh	r2, [r4, #134]
	mov	r9, #0
	ldr	r8, [r4, #152]
	ldrh	r7, [r4, #136]
	smulbb	r2, r2, lr
	ldrh	r6, [r4, #138]
	smulbb	r7, r7, r1
	str	r3, [r4, #100]
	smulbb	r6, r6, ip
	asr	r0, r2, #31
	lsl	r0, r0, #20
	orr	r0, r0, r2, lsr #12
	lsl	r2, r2, #20
	adds	r2, r9, r2
	adcs	r8, r8, r0
	movvs	r9, #1
	ands	r0, r9, r8, lsr #31
	orrne	r5, r5, #536870912
	bne	.L2897
	cmp	r9, #0
	mvnne	r0, r8
	lsrne	r0, r0, #31
	orrne	r5, r5, r0, lsl #26
.L2897:
	asr	r0, r7, #31
	lsl	r0, r0, #20
	orr	r0, r0, r7, lsr #12
	lsl	r7, r7, #20
	adds	r7, r2, r7
	adcs	r2, r8, r0
	mov	r0, #0
	movvs	r0, #1
	ands	r8, r0, r2, lsr #31
	orrne	r5, r5, #536870912
	bne	.L2901
	cmp	r0, #0
	mvnne	r0, r2
	lsrne	r0, r0, #31
	orrne	r5, r5, r0, lsl #26
.L2901:
	asr	r0, r6, #31
	lsl	r0, r0, #20
	orr	r0, r0, r6, lsr #12
	lsl	r6, r6, #20
	adds	r6, r7, r6
	adcs	r2, r2, r0
	mov	r0, #0
	movvs	r0, #1
	ands	r6, r0, r2, lsr #31
	orrne	r5, r5, #536870912
	bne	.L2905
	cmp	r0, #0
	mvnne	r0, r2
	lsrne	r0, r0, #31
	orrne	r5, r5, r0, lsl #26
.L2905:
	ldrh	r0, [r4, #140]
	ldrh	r6, [r4, #142]
	ldr	r7, [r4, #156]
	smulbb	r0, r0, lr
	ldrh	lr, [r4, #144]
	smulbb	r6, r6, r1
	str	r2, [r4, #104]
	smulbb	lr, lr, ip
	mov	ip, #0
	asr	r1, r0, #31
	lsl	r1, r1, #20
	orr	r1, r1, r0, lsr #12
	lsl	r0, r0, #20
	adds	r0, ip, r0
	adcs	r1, r7, r1
	movvs	ip, #1
	ands	r7, ip, r1, lsr #31
	orrne	r5, r5, #268435456
	bne	.L2909
	cmp	ip, #0
	mvnne	ip, r1
	lsrne	ip, ip, #31
	orrne	r5, r5, ip, lsl #25
.L2909:
	asr	ip, r6, #31
	lsl	ip, ip, #20
	orr	ip, ip, r6, lsr #12
	lsl	r6, r6, #20
	adds	r6, r0, r6
	mov	r0, #0
	adcs	r1, r1, ip
	movvs	r0, #1
	ands	ip, r0, r1, lsr #31
	orrne	r5, r5, #268435456
	bne	.L2913
	cmp	r0, #0
	mvnne	r0, r1
	lsrne	r0, r0, #31
	orrne	r5, r5, r0, lsl #25
.L2913:
	asr	ip, lr, #31
	mov	r0, #0
	lsl	ip, ip, #20
	orr	ip, ip, lr, lsr #12
	lsl	lr, lr, #20
	adds	lr, r6, lr
	adcs	r1, r1, ip
	movvs	r0, #1
	ands	ip, r0, r1, lsr #31
	orrne	r5, r5, #268435456
	bne	.L2917
	cmp	r0, #0
	mvnne	r0, r1
	lsrne	r0, r0, #31
	orrne	r5, r5, r0, lsl #25
.L2917:
	cmp	r3, #32768
	str	r1, [r4, #108]
	orrge	r5, r5, #16777216
	movwge	r3, #32767
	bge	.L2919
	cmn	r3, #32768
	sxthge	r3, r3
	orrlt	r5, r5, #16777216
	movlt	r3, #32768
	movtlt	r3, 65535
.L2919:
	cmp	r2, #32768
	strh	r3, [r4, #36]	@ movhi
	orrge	r5, r5, #8388608
	movwge	r2, #32767
	bge	.L2922
	cmn	r2, #32768
	sxthge	r2, r2
	orrlt	r5, r5, #8388608
	movlt	r2, #32768
	movtlt	r2, 65535
.L2922:
	add	r0, r1, #32768
	strh	r2, [r4, #40]	@ movhi
	cmp	r0, #65536
	ssat	r0, #16, r1
	strh	r0, [r4, #44]	@ movhi
	bcc	.L2924
	cmp	r1, #65536
	orrlt	r5, r5, #4194304
	blt	.L2924
	ldrh	r0, [r4, #232]
	orr	r5, r5, #4456448
	movw	r6, #65535
.L2926:
	mov	r1, r6
	bl	DIVIDE
	movw	r2, #65535
	movt	r2, 1
	ldrsh	r3, [r4, #36]
	cmp	r0, r2
	movge	r0, r2
	ldrsh	r2, [r4, #40]
	asr	ip, r0, #31
.L2929:
	ldrh	r1, [r4, #68]
	asr	lr, r3, #31
	strh	r1, [r4, #64]	@ movhi
	ldrh	r1, [r4, #72]
	strh	r1, [r4, #68]	@ movhi
	ldrh	r1, [r4, #76]
	strh	r6, [r4, #76]	@ movhi
	strh	r1, [r4, #72]	@ movhi
	add	r1, r4, #52
	vld1.32	{d16}, [r1]
	mul	r1, r3, ip
	mla	r1, r0, lr, r1
	umull	r3, lr, r3, r0
	add	r1, r1, lr
	add	lr, r4, #48
	vst1.32	{d16}, [lr]
	ldr	lr, [r4, #224]
	adds	r3, lr, r3
	adc	r1, r1, lr, asr #31
	cmp	r3, #-2147483648
	sbcs	lr, r1, #0
	orrge	r5, r5, #65536
	bge	.L2931
	cmp	r3, #-2147483648
	adcs	lr, r1, #0
	orrlt	r5, r5, #32768
	blt	.L2933
	lsr	r3, r3, #16
	orr	r3, r3, r1, lsl #16
	cmp	r3, #1024
	bge	.L2931
	cmn	r3, #1024
	blt	.L2933
	sxth	r3, r3
	b	.L2935
.L2924:
	cmp	r1, #0
	orrlt	r5, r5, #262144
	movlt	r6, #0
	blt	.L2928
	ldrh	r0, [r4, #232]
	uxth	r6, r1
	cmp	r0, r1, lsl #1
	bcc	.L2926
.L2928:
	movw	r0, #65535
	orr	r5, r5, #131072
	movt	r0, 1
	mov	ip, #0
	b	.L2929
.L2931:
	orr	r5, r5, #16384
	movw	r3, #1023
.L2935:
	asr	r1, r2, #31
	strh	r3, [r4, #56]	@ movhi
	mul	r3, r2, ip
	mla	r3, r0, r1, r3
	umull	r2, r1, r2, r0
	add	r3, r3, r1
	ldr	r1, [r4, #228]
	adds	r2, r1, r2
	adc	r3, r3, r1, asr #31
	cmp	r2, #-2147483648
	sbcs	r1, r3, #0
	orrge	r5, r5, #65536
	bge	.L2937
	cmp	r2, #-2147483648
	adcs	r1, r3, #0
	orrlt	r5, r5, #32768
	blt	.L2939
	lsr	r2, r2, #16
	orr	r2, r2, r3, lsl #16
	cmp	r2, #1024
	bge	.L2937
	cmn	r2, #1024
	blt	.L2939
	sxth	r2, r2
	b	.L2941
.L2937:
	orr	r5, r5, #8192
	movw	r2, #1023
.L2941:
	ldrsh	r3, [r4, #236]
	strh	r2, [r4, #58]	@ movhi
	asr	r2, r3, #31
	mul	ip, r3, ip
	mla	ip, r0, r2, ip
	ldr	r2, [r4, #240]
	umull	r3, r0, r3, r0
	adds	r3, r3, r2
	add	ip, ip, r0
	adc	ip, ip, r2, asr #31
	cmp	r3, #-2147483648
	sbcs	r2, ip, #0
	orrge	r5, r5, #65536
	bge	.L2943
	cmp	r3, #-2147483648
	adcs	r2, ip, #0
	orrlt	r5, r5, #32768
.L2943:
	lsr	r2, r3, #12
	str	r3, [r4, #96]
	orr	r2, r2, ip, lsl #20
	cmp	r2, #4096
	orrgt	r5, r5, #4096
	movgt	r2, #4096
	bgt	.L2945
	cmp	r2, #0
	sxthge	r2, r2
	orrlt	r5, r5, #4096
	movlt	r2, #0
.L2945:
	mov	r3, #57344
	movt	r3, 32647
	and	r3, r3, r5
	strh	r2, [r4, #32]	@ movhi
	rsb	r3, r3, #0
	and	r3, r3, #-2147483648
	orr	r3, r3, r5
	str	r3, [r4, #252]
	pop	{r4, r5, r6, r7, r8, r9, r10, pc}
.L2933:
	mov	r3, #64512
	orr	r5, r5, #16384
	movt	r3, 65535
	b	.L2935
.L2939:
	mov	r2, #64512
	orr	r5, r5, #8192
	movt	r2, 65535
	b	.L2941
	.size	gteRTPS_sf1lm0c, .-gteRTPS_sf1lm0c

	.align	2
	.syntax unified
	.arm
	.fpu neon
	.type	gteRTPT_sf1lm0c, %function
gteRTPT_sf1lm0c:
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 0, uses_anonymous_args = 0
	push	{r4, r5, r6, r7, r8, r9, r10, fp, lr}
	sub	sp, sp, #44
	add	r3, r0, #12
	ldrh	r2, [r0, #232]
	str	r3, [sp, #8]
	mov	r5, r0
	ldrh	r3, [r0, #76]
	mov	r4, #0
	mov	fp, r0
	str	r2, [sp, #20]
	str	r0, [sp, #4]
	strh	r3, [r0, #64]	@ movhi
	mov	r3, #32768
	movt	r3, 65535
	str	r3, [sp, #24]
.L588:
	ldr	r3, [sp, #4]
	mov	r7, #0
	ldrh	r1, [r5, #128]
	ldr	ip, [r5, #148]
	ldrsh	lr, [r3]
	ldrsh	r6, [r3, #2]
	ldrsh	r9, [r3, #4]
	smulbb	r1, r1, lr
	ldrh	r2, [r5, #130]
	ldrh	r0, [r5, #132]
	smulbb	r2, r2, r6
	asr	r3, r1, #31
	smulbb	r0, r0, r9
	lsl	r3, r3, #20
	orr	r3, r3, r1, lsr #12
	lsl	r1, r1, #20
	adds	r1, r7, r1
	adcs	r3, ip, r3
	mov	ip, r7
	movvs	ip, #1
	ands	r7, ip, r3, lsr #31
	orrne	r4, r4, #1073741824
	bne	.L531
	cmp	ip, #0
	mvnne	ip, r3
	lsrne	ip, ip, #31
	orrne	r4, r4, ip, lsl #27
.L531:
	asr	r8, r2, #31
	lsl	r8, r8, #20
	orr	r8, r8, r2, lsr #12
	lsl	r2, r2, #20
	adds	r2, r1, r2
	adcs	r8, r3, r8
	mov	r3, #0
	movvs	r3, #1
	ands	r1, r3, r8, lsr #31
	orrne	r4, r4, #1073741824
	bne	.L535
	cmp	r3, #0
	mvnne	r3, r8
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #27
.L535:
	asr	r3, r0, #31
	lsl	r3, r3, #20
	orr	r3, r3, r0, lsr #12
	lsl	r0, r0, #20
	adds	r0, r2, r0
	adcs	r8, r8, r3
	mov	r3, #0
	movvs	r3, #1
	ands	r2, r3, r8, lsr #31
	orrne	r4, r4, #1073741824
	bne	.L539
	cmp	r3, #0
	mvnne	r3, r8
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #27
.L539:
	ldrh	r1, [r5, #134]
	mov	r7, #0
	ldr	ip, [r5, #152]
	ldrh	r2, [r5, #136]
	smulbb	r1, r1, lr
	ldrh	r0, [r5, #138]
	smulbb	r2, r2, r6
	smulbb	r0, r0, r9
	asr	r3, r1, #31
	lsl	r3, r3, #20
	orr	r3, r3, r1, lsr #12
	lsl	r1, r1, #20
	adds	r1, r7, r1
	adcs	r3, ip, r3
	mov	ip, r7
	movvs	ip, #1
	ands	r7, ip, r3, lsr #31
	orrne	r4, r4, #536870912
	bne	.L543
	cmp	ip, #0
	mvnne	ip, r3
	lsrne	ip, ip, #31
	orrne	r4, r4, ip, lsl #26
.L543:
	asr	r7, r2, #31
	lsl	r7, r7, #20
	orr	r7, r7, r2, lsr #12
	lsl	r2, r2, #20
	adds	r2, r1, r2
	adcs	r7, r3, r7
	mov	r3, #0
	movvs	r3, #1
	ands	r1, r3, r7, lsr #31
	orrne	r4, r4, #536870912
	bne	.L547
	cmp	r3, #0
	mvnne	r3, r7
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #26
.L547:
	asr	r3, r0, #31
	lsl	r3, r3, #20
	orr	r3, r3, r0, lsr #12
	lsl	r0, r0, #20
	adds	r0, r2, r0
	adcs	r7, r7, r3
	mov	r3, #0
	movvs	r3, #1
	ands	r2, r3, r7, lsr #31
	orrne	r4, r4, #536870912
	bne	.L551
	cmp	r3, #0
	mvnne	r3, r7
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #26
.L551:
	ldrh	r1, [r5, #140]
	ldr	ip, [r5, #156]
	ldrh	r2, [r5, #142]
	smulbb	r1, r1, lr
	mov	lr, #0
	ldrh	r0, [r5, #144]
	smulbb	r2, r2, r6
	smulbb	r0, r0, r9
	asr	r3, r1, #31
	lsl	r3, r3, #20
	orr	r3, r3, r1, lsr #12
	lsl	r1, r1, #20
	adds	r1, lr, r1
	adcs	r3, ip, r3
	mov	ip, lr
	movvs	ip, #1
	ands	lr, ip, r3, lsr #31
	orrne	r4, r4, #268435456
	bne	.L555
	cmp	ip, #0
	mvnne	ip, r3
	lsrne	ip, ip, #31
	orrne	r4, r4, ip, lsl #25
.L555:
	asr	r6, r2, #31
	lsl	r6, r6, #20
	orr	r6, r6, r2, lsr #12
	lsl	r2, r2, #20
	adds	r2, r1, r2
	adcs	r6, r3, r6
	mov	r3, #0
	movvs	r3, #1
	ands	r1, r3, r6, lsr #31
	orrne	r4, r4, #268435456
	bne	.L559
	cmp	r3, #0
	mvnne	r3, r6
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #25
.L559:
	asr	r3, r0, #31
	lsl	r3, r3, #20
	orr	r3, r3, r0, lsr #12
	lsl	r0, r0, #20
	adds	r0, r2, r0
	adcs	r6, r6, r3
	mov	r3, #0
	movvs	r3, #1
	ands	r2, r3, r6, lsr #31
	orrne	r4, r4, #268435456
	bne	.L563
	cmp	r3, #0
	mvnne	r3, r6
	lsrne	r3, r3, #31
	orrne	r4, r4, r3, lsl #25
.L563:
	cmp	r8, #32768
	blt	.L564
	orr	r4, r4, #16777216
	mov	r2, #0
	movw	r10, #32767
	str	r10, [sp, #16]
.L565:
	cmp	r7, #32768
	blt	.L567
	orr	r4, r4, #8388608
	mov	r3, #0
	movw	r9, #32767
	str	r9, [sp, #12]
.L568:
	add	r1, r6, #32768
	cmp	r1, #65536
	bcc	.L570
	cmp	r6, #65536
	orrlt	r4, r4, #4194304
	blt	.L570
	orr	r4, r4, #4456448
	movw	r1, #65535
.L572:
	ldr	r0, [sp, #20]
	strd	r2, [sp, #32]
	str	r1, [sp, #28]
	bl	DIVIDE
	add	r1, sp, #28
	movw	ip, #65535
	movt	ip, 1
	ldm	r1, {r1, r2, r3}
	cmp	r0, ip
	movge	r0, ip
	asr	ip, r0, #31
.L575:
	strh	r1, [fp, #68]	@ movhi
	mul	r1, r10, ip
	mla	r1, r0, r2, r1
	umull	r2, r10, r10, r0
	add	r10, r1, r10
	ldr	r1, [r5, #224]
	adds	r2, r1, r2
	adc	r10, r10, r1, asr #31
	cmp	r2, #-2147483648
	sbcs	r1, r10, #0
	orrge	r4, r4, #65536
	bge	.L577
	cmp	r2, #-2147483648
	adcs	r1, r10, #0
	orrlt	r4, r4, #32768
	blt	.L579
	lsr	r2, r2, #16
	orr	r2, r2, r10, lsl #16
	cmp	r2, #1024
	bge	.L577
	cmn	r2, #1024
	blt	.L579
	sxth	r2, r2
.L581:
	strh	r2, [fp, #48]	@ movhi
	mul	r2, r9, ip
	mla	r2, r0, r3, r2
	umull	r3, r9, r9, r0
	add	r9, r2, r9
	ldr	r2, [r5, #228]
	adds	r3, r2, r3
	adc	r9, r9, r2, asr #31
	cmp	r3, #-2147483648
	sbcs	r2, r9, #0
	orrge	r4, r4, #65536
	bge	.L583
	cmp	r3, #-2147483648
	adcs	r2, r9, #0
	orrlt	r4, r4, #32768
	blt	.L585
	lsr	r3, r3, #16
	orr	r3, r3, r9, lsl #16
	cmp	r3, #1024
	bge	.L583
	cmn	r3, #1024
	blt	.L585
	sxth	r3, r3
.L587:
	strh	r3, [fp, #50]	@ movhi
	add	fp, fp, #4
	ldr	r3, [sp, #8]
	cmp	fp, r3
	ldr	r3, [sp, #4]
	add	r3, r3, #8
	str	r3, [sp, #4]
	bne	.L588
	ldr	r3, [sp, #16]
	ldrsh	r2, [r5, #236]
	str	r6, [r5, #108]
	ssat	r6, #16, r6
	strh	r3, [r5, #36]	@ movhi
	ldr	r3, [sp, #12]
	asr	r1, r2, #31
	str	r8, [r5, #100]
	str	r7, [r5, #104]
	strh	r3, [r5, #40]	@ movhi
	mul	r3, r2, ip
	strh	r6, [r5, #44]	@ movhi
	mla	r3, r0, r1, r3
	ldr	r1, [r5, #240]
	umull	r2, r0, r2, r0
	adds	r2, r2, r1
	add	r3, r3, r0
	adc	r3, r3, r1, asr #31
	cmp	r2, #-2147483648
	sbcs	r1, r3, #0
	orrge	r4, r4, #65536
	bge	.L590
	cmp	r2, #-2147483648
	adcs	r1, r3, #0
	orrlt	r4, r4, #32768
.L590:
	lsr	r1, r2, #12
	str	r2, [r5, #96]
	orr	r1, r1, r3, lsl #20
	cmp	r1, #4096
	orrgt	r4, r4, #4096
	movgt	r1, #4096
	bgt	.L592
	cmp	r1, #0
	sxthge	r1, r1
	orrlt	r4, r4, #4096
	movlt	r1, #0
.L592:
	mov	r3, #57344
	movt	r3, 32647
	and	r3, r3, r4
	strh	r1, [r5, #32]	@ movhi
	rsb	r3, r3, #0
	and	r3, r3, #-2147483648
	orr	r3, r3, r4
	str	r3, [r5, #252]
	add	sp, sp, #44
	@ sp needed
	pop	{r4, r5, r6, r7, r8, r9, r10, fp, pc}
.L583:
	orr	r4, r4, #8192
	movw	r3, #1023
	b	.L587
.L577:
	orr	r4, r4, #16384
	movw	r2, #1023
	b	.L581
.L570:
	cmp	r6, #0
	orrlt	r4, r4, #262144
	movlt	r1, #0
	blt	.L574
	ldr	r1, [sp, #20]
	cmp	r1, r6, lsl #1
	uxth	r1, r6
	bcc	.L572
.L574:
	movw	r0, #65535
	orr	r4, r4, #131072
	movt	r0, 1
	mov	ip, #0
	b	.L575
.L567:
	cmn	r7, #32768
	strge	r7, [sp, #12]
	asrge	r3, r7, #31
	orrlt	r4, r4, #8388608
	ldrlt	r1, [sp, #24]
	movge	r9, r7
	mvnlt	r3, #0
	movlt	r9, r1
	strlt	r1, [sp, #12]
	b	.L568
.L564:
	cmn	r8, #32768
	strge	r8, [sp, #16]
	asrge	r2, r8, #31
	orrlt	r4, r4, #16777216
	ldrlt	r3, [sp, #24]
	movge	r10, r8
	mvnlt	r2, #0
	movlt	r10, r3
	strlt	r3, [sp, #16]
	b	.L565
.L585:
	mov	r3, #64512
	orr	r4, r4, #8192
	movt	r3, 65535
	b	.L587
.L579:
	mov	r2, #64512
	orr	r4, r4, #16384
	movt	r2, 65535
	b	.L581
	.size	gteRTPT_sf1lm0c, .-gteRTPT_sf1lm0c

	.align	2
	.syntax unified
	.arm
	.fpu neon
	.type	gteRTPS_sf1lm0_nfc, %function
gteRTPS_sf1lm0_nfc:
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 0, uses_anonymous_args = 0
	ldr	r1, [r0, #148]
	push	{r4, r5, r6, r7, r8, r9, r10, fp, lr}
	mov	r4, r0
	asr	ip, r1, #31
	ldrsh	lr, [r0]
	ldr	r9, [r0, #152]
	lsl	r2, r1, #12
	ldrh	r0, [r0, #128]
	lsl	r3, ip, #12
	orr	r3, r3, r1, lsr #20
	ldr	r1, [r4, #156]
	sub	sp, sp, #28
	ldrsh	r5, [r4, #2]
	smlalbb	r2, r3, lr, r0
	asr	r1, r1, #31
	str	r1, [sp, #8]
	asr	r6, r9, #31
	ldrh	r1, [r4, #130]
	ldrsh	r7, [r4, #4]
	ldrh	ip, [r4, #132]
	smlalbb	r2, r3, r5, r1
	ldrh	r0, [r4, #140]
	ldrh	r10, [r4, #134]
	ldrh	fp, [r4, #136]
	str	r0, [sp, #12]
	ldrh	r8, [r4, #232]
	smlalbb	r2, r3, r7, ip
	ldrh	ip, [r4, #144]
	str	ip, [sp, #20]
	movw	ip, #32767
	mov	r1, r3
	ldrh	r3, [r4, #142]
	mov	r0, r2
	ldrh	r2, [r4, #138]
	str	r3, [sp, #16]
	lsr	r3, r0, #12
	orr	r3, r3, r1, lsl #20
	lsl	r1, r6, #12
	orr	r1, r1, r9, lsr #20
	lsl	r0, r9, #12
	str	r2, [sp, #4]
	mov	r2, #32768
	smlalbb	r0, r1, lr, r10
	ldr	r6, [sp, #4]
	ldr	r9, [sp, #8]
	movt	r2, 65535
	ldr	r10, [r4, #156]
	cmp	r3, r2
	str	r3, [r4, #100]
	movlt	r3, r2
	cmp	r3, ip
	smlalbb	r0, r1, r5, fp
	movge	r3, ip
	sxth	r3, r3
	strh	r3, [r4, #36]	@ movhi
	smlalbb	r0, r1, r7, r6
	lsr	r6, r0, #12
	lsl	r0, r10, #12
	orr	r6, r6, r1, lsl #20
	lsl	r1, r9, #12
	ldr	r9, [sp, #12]
	orr	r1, r1, r10, lsr #20
	cmp	r6, r2
	str	r6, [r4, #104]
	smlalbb	r0, r1, lr, r9
	ldr	lr, [sp, #16]
	movlt	r6, r2
	cmp	r6, ip
	movge	r6, ip
	sxth	r6, r6
	strh	r6, [r4, #40]	@ movhi
	smlalbb	r0, r1, r5, lr
	ldr	lr, [sp, #20]
	smlalbb	r0, r1, r7, lr
	lsr	lr, r0, #12
	orr	lr, lr, r1, lsl #20
	str	lr, [r4, #108]
	cmp	lr, r2
	usat	r0, #16, lr
	movge	r2, lr
	cmp	r2, ip
	uxth	r5, r0
	movge	r2, ip
	cmp	r8, r0, lsl #1
	strh	r2, [r4, #44]	@ movhi
	movwcs	r0, #65535
	movcs	r2, #0
	movtcs	r0, 1
	bcc	.L48
.L45:
	ldrh	r1, [r4, #68]
	asr	ip, r3, #31
	strh	r1, [r4, #64]	@ movhi
	ldrh	r1, [r4, #72]
	strh	r1, [r4, #68]	@ movhi
	ldrh	r1, [r4, #76]
	strh	r5, [r4, #76]	@ movhi
	strh	r1, [r4, #72]	@ movhi
	add	r1, r4, #52
	vld1.32	{d16}, [r1]
	mul	r1, r3, r2
	mla	r1, r0, ip, r1
	umull	r3, ip, r3, r0
	add	r1, r1, ip
	add	ip, r4, #48
	vst1.32	{d16}, [ip]
	ldr	ip, [r4, #224]
	adds	r3, ip, r3
	adc	r1, r1, ip, asr #31
	asr	ip, r6, #31
	lsr	r3, r3, #16
	orr	r3, r3, r1, lsl #16
	mul	r1, r6, r2
	mla	r1, r0, ip, r1
	mov	ip, #64512
	umull	r6, lr, r6, r0
	movt	ip, 65535
	cmp	r3, ip
	add	r1, r1, lr
	movw	lr, #1023
	movlt	r3, ip
	cmp	r3, lr
	movge	r3, lr
	strh	r3, [r4, #56]	@ movhi
	ldr	r3, [r4, #228]
	adds	r6, r3, r6
	adc	r3, r1, r3, asr #31
	lsr	r6, r6, #16
	orr	r6, r6, r3, lsl #16
	ldrsh	r3, [r4, #236]
	cmp	r6, ip
	asr	r1, r3, #31
	movlt	r6, ip
	mul	r2, r3, r2
	cmp	r6, lr
	mla	r2, r0, r1, r2
	ldr	r1, [r4, #240]
	umull	r3, r0, r3, r0
	movge	r6, lr
	strh	r6, [r4, #58]	@ movhi
	adds	r3, r3, r1
	add	r2, r2, r0
	adc	r2, r2, r1, asr #31
	str	r3, [r4, #96]
	lsr	r3, r3, #12
	mov	r1, #0
	orr	r3, r3, r2, lsl #20
	str	r1, [r4, #252]
	bic	r3, r3, r3, asr #31
	cmp	r3, #4096
	movge	r3, #4096
	strh	r3, [r4, #32]	@ movhi
	add	sp, sp, #28
	@ sp needed
	pop	{r4, r5, r6, r7, r8, r9, r10, fp, pc}
.L48:
	mov	r1, r5
	mov	r0, r8
	bl	DIVIDE
	movw	r3, #65535
	movt	r3, 1
	ldrsh	r6, [r4, #40]
	cmp	r0, r3
	movge	r0, r3
	ldrsh	r3, [r4, #36]
	asr	r2, r0, #31
	b	.L45
	.size	gteRTPS_sf1lm0_nfc, .-gteRTPS_sf1lm0_nfc

	.align	2
	.syntax unified
	.arm
	.fpu neon
	.type	gteRTPT_sf1lm0_nfc, %function
gteRTPT_sf1lm0_nfc:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 0, uses_anonymous_args = 0
	ldrh	r3, [r0, #232]
	push	{r4, r5, r6, r7, r8, r9, r10, fp, lr}
	sub	sp, sp, #52
	mov	r4, r0
	ldrh	r2, [r0, #128]
	str	r3, [sp]
	ldrh	r3, [r4, #142]
	ldr	lr, [r0, #148]
	str	r2, [sp, #4]
	str	r3, [sp, #32]
	ldrh	r3, [r4, #144]
	asr	r5, lr, #31
	ldrh	r2, [r4, #130]
	ldrsh	ip, [r0, #4]
	str	r3, [sp, #36]
	ldrh	r3, [r4, #76]
	str	r2, [sp, #8]
	ldrh	r2, [r4, #132]
	ldrsh	r8, [r0]
	ldrsh	r9, [r0, #2]
	ldr	r10, [r0, #152]
	ldr	fp, [r0, #156]
	ldrh	r1, [r0, #134]
	ldrh	r0, [r0, #140]
	asr	r7, r10, #31
	strh	r3, [r4, #64]	@ movhi
	lsl	r3, r5, #12
	ldr	r5, [sp, #4]
	orr	r3, r3, lr, lsr #20
	str	r2, [sp, #12]
	lsl	r2, lr, #12
	str	r1, [sp, #16]
	asr	r6, fp, #31
	smlalbb	r2, r3, r5, r8
	ldr	r5, [sp, #8]
	ldrh	r1, [r4, #136]
	str	r0, [sp, #28]
	ldrh	r0, [r4, #138]
	str	r1, [sp, #20]
	mov	r1, #32768
	smlalbb	r2, r3, r5, r9
	ldr	r5, [sp, #12]
	str	r0, [sp, #24]
	movt	r1, 65535
	movw	r0, #32767
	smlalbb	r2, r3, r5, ip
	lsr	r5, r2, #12
	lsl	r2, r10, #12
	orr	r5, r5, r3, lsl #20
	lsl	r3, r7, #12
	ldr	r7, [sp, #16]
	orr	r3, r3, r10, lsr #20
	cmp	r5, r1
	smlalbb	r2, r3, r7, r8
	ldr	r7, [sp, #20]
	movlt	r5, r1
	cmp	r5, r0
	movge	r5, r0
	smlalbb	r2, r3, r7, r9
	ldr	r7, [sp, #24]
	smlalbb	r2, r3, r7, ip
	lsr	r7, r2, #12
	lsl	r2, fp, #12
	orr	r7, r7, r3, lsl #20
	lsl	r3, r6, #12
	cmp	r7, r1
	orr	r3, r3, fp, lsr #20
	movlt	r7, r1
	cmp	r7, r0
	ldr	r1, [sp, #32]
	movge	r7, r0
	ldr	r0, [sp, #28]
	smlalbb	r2, r3, r0, r8
	smlalbb	r2, r3, r1, r9
	ldr	r1, [sp, #36]
	smlalbb	r2, r3, r1, ip
	lsr	r6, r2, #12
	orr	r6, r6, r3, lsl #20
	ldr	r3, [sp]
	usat	r6, #16, r6
	cmp	r3, r6, lsl #1
	uxth	r6, r6
	movwcs	r3, #65535
	movcs	ip, #0
	movtcs	r3, 1
	bcc	.L90
.L83:
	asr	r1, r5, #31
	ldr	r9, [r4, #224]
	mul	r2, r5, ip
	movw	r0, #1023
	mla	r2, r3, r1, r2
	ldrsh	r8, [r4, #8]
	mul	r1, r7, ip
	strh	r6, [r4, #68]	@ movhi
	umull	ip, r5, r5, r3
	asr	r6, lr, #31
	str	r8, [sp, #44]
	asr	r8, r10, #31
	adds	ip, ip, r9
	add	r2, r2, r5
	adc	r2, r2, r9, asr #31
	ldrsh	r5, [r4, #10]
	lsr	ip, ip, #16
	orr	ip, ip, r2, lsl #16
	asr	r2, r7, #31
	mla	r2, r3, r2, r1
	mov	r1, #64512
	movt	r1, 65535
	cmp	ip, r1
	umull	r3, r7, r7, r3
	movlt	ip, r1
	cmp	ip, r0
	add	r2, r2, r7
	ldrsh	r7, [r4, #12]
	movge	ip, r0
	strh	ip, [r4, #48]	@ movhi
	ldr	ip, [r4, #228]
	adds	r3, r3, ip
	adc	r2, r2, ip, asr #31
	str	ip, [sp, #40]
	lsr	r3, r3, #16
	ldr	ip, [sp, #4]
	orr	r3, r3, r2, lsl #16
	lsl	r2, lr, #12
	cmp	r3, r1
	movlt	r3, r1
	cmp	r3, r0
	mov	r1, #32768
	movt	r1, 65535
	movge	r3, r0
	strh	r3, [r4, #50]	@ movhi
	lsl	r3, r6, #12
	ldr	r6, [sp, #44]
	orr	r3, r3, lr, lsr #20
	str	r5, [sp, #44]
	movw	r0, #32767
	smlalbb	r2, r3, ip, r6
	ldr	ip, [sp, #8]
	smlalbb	r2, r3, ip, r5
	ldr	ip, [sp, #12]
	smlalbb	r2, r3, ip, r7
	ldr	ip, [sp, #16]
	lsr	r5, r2, #12
	lsl	r2, r10, #12
	orr	r5, r5, r3, lsl #20
	lsl	r3, r8, #12
	orr	r3, r3, r10, lsr #20
	mov	r8, r6
	cmp	r5, r1
	smlalbb	r2, r3, ip, r6
	ldr	ip, [sp, #20]
	ldr	r6, [sp, #44]
	movlt	r5, r1
	cmp	r5, r0
	movge	r5, r0
	smlalbb	r2, r3, ip, r6
	ldr	ip, [sp, #24]
	smlalbb	r2, r3, ip, r7
	lsr	r6, r2, #12
	lsl	r2, fp, #12
	orr	r6, r6, r3, lsl #20
	cmp	r6, r1
	movlt	r6, r1
	asr	r1, fp, #31
	cmp	r6, r0
	lsl	r3, r1, #12
	ldr	r1, [sp, #28]
	orr	r3, r3, fp, lsr #20
	movge	r6, r0
	ldr	r0, [sp, #32]
	smlalbb	r2, r3, r1, r8
	ldr	r1, [sp, #44]
	smlalbb	r2, r3, r0, r1
	ldr	r1, [sp, #36]
	smlalbb	r2, r3, r1, r7
	lsr	r7, r2, #12
	orr	r7, r7, r3, lsl #20
	ldr	r3, [sp]
	usat	r7, #16, r7
	cmp	r3, r7, lsl #1
	uxth	r7, r7
	movwcs	r3, #65535
	movcs	r0, #0
	movtcs	r3, 1
	bcc	.L91
.L84:
	asr	r2, r5, #31
	strh	r7, [r4, #72]	@ movhi
	mul	r1, r5, r0
	asr	r7, lr, #31
	mla	r1, r3, r2, r1
	ldrsh	r8, [r4, #16]
	umull	r2, r5, r5, r3
	asr	ip, r10, #31
	mul	r0, r6, r0
	add	r5, r1, r5
	ldrsh	r1, [r4, #18]
	adds	r2, r2, r9
	adc	r5, r5, r9, asr #31
	lsr	r2, r2, #16
	str	r1, [sp, #44]
	asr	r1, r6, #31
	orr	r2, r2, r5, lsl #16
	mov	r5, #64512
	mla	r1, r3, r1, r0
	movt	r5, 65535
	umull	r3, r6, r6, r3
	cmp	r2, r5
	ldrsh	r0, [r4, #20]
	movlt	r2, r5
	add	r1, r1, r6
	movw	r6, #1023
	cmp	r2, r6
	movge	r2, r6
	strh	r2, [r4, #52]	@ movhi
	ldr	r2, [sp, #40]
	adds	r3, r3, r2
	adc	r1, r1, r2, asr #31
	lsl	r2, lr, #12
	lsr	r3, r3, #16
	orr	r3, r3, r1, lsl #16
	movw	r1, #32767
	cmp	r3, r5
	movlt	r3, r5
	cmp	r3, r6
	mov	r5, #32768
	movt	r5, 65535
	movge	r3, r6
	ldr	r6, [sp, #4]
	strh	r3, [r4, #54]	@ movhi
	lsl	r3, r7, #12
	orr	r3, r3, lr, lsr #20
	ldr	r7, [sp, #44]
	mov	lr, r8
	smlalbb	r2, r3, r8, r6
	ldr	r6, [sp, #8]
	smlalbb	r2, r3, r7, r6
	ldr	r6, [sp, #12]
	smlalbb	r2, r3, r0, r6
	lsr	r8, r2, #12
	lsl	r2, r10, #12
	orr	r8, r8, r3, lsl #20
	lsl	r3, ip, #12
	ldr	ip, [sp, #16]
	orr	r3, r3, r10, lsr #20
	cmp	r8, r5
	mov	r10, r7
	smlalbb	r2, r3, lr, ip
	ldr	ip, [sp, #20]
	movge	r6, r8
	movlt	r6, r5
	cmp	r6, r1
	movge	r6, r1
	smlalbb	r2, r3, r7, ip
	ldr	ip, [sp, #24]
	smlalbb	r2, r3, r0, ip
	lsr	r7, r2, #12
	lsl	r2, fp, #12
	orr	r7, r7, r3, lsl #20
	cmp	r7, r5
	movge	r5, r7
	cmp	r5, r1
	movge	r5, r1
	asr	r1, fp, #31
	lsl	r3, r1, #12
	ldr	r1, [sp, #28]
	orr	r3, r3, fp, lsr #20
	smlalbb	r2, r3, lr, r1
	ldr	r1, [sp, #32]
	smlalbb	r2, r3, r10, r1
	ldr	r1, [sp, #36]
	smlalbb	r2, r3, r0, r1
	lsr	r10, r2, #12
	orr	r10, r10, r3, lsl #20
	ldr	r3, [sp]
	usat	fp, #16, r10
	cmp	r3, fp, lsl #1
	uxth	fp, fp
	movwcs	r2, #65535
	movcs	r1, #0
	movtcs	r2, 1
	bcc	.L92
.L85:
	asr	r3, r6, #31
	strh	r6, [r4, #36]	@ movhi
	mul	r0, r6, r1
	asr	lr, r5, #31
	mla	r0, r2, r3, r0
	strh	r5, [r4, #40]	@ movhi
	umull	r6, r3, r6, r2
	ldr	ip, [sp, #40]
	str	r10, [r4, #108]
	ssat	r10, #16, r10
	add	r0, r0, r3
	adds	r3, r9, r6
	adc	r0, r0, r9, asr #31
	strh	fp, [r4, #76]	@ movhi
	lsr	r3, r3, #16
	str	r8, [r4, #100]
	orr	r3, r3, r0, lsl #16
	str	r7, [r4, #104]
	mul	r0, r5, r1
	strh	r10, [r4, #44]	@ movhi
	umull	r5, r6, r5, r2
	mla	r0, r2, lr, r0
	mov	lr, #64512
	movt	lr, 65535
	cmp	r3, lr
	add	r0, r0, r6
	movw	r6, #1023
	movlt	r3, lr
	cmp	r3, r6
	movge	r3, r6
	strh	r3, [r4, #56]	@ movhi
	adds	r3, ip, r5
	adc	r0, r0, ip, asr #31
	lsr	r3, r3, #16
	orr	r3, r3, r0, lsl #16
	ldrsh	r0, [r4, #236]
	cmp	r3, lr
	movlt	r3, lr
	cmp	r3, r6
	mul	r1, r0, r1
	movge	r3, r6
	strh	r3, [r4, #58]	@ movhi
	asr	r3, r0, #31
	mla	r1, r2, r3, r1
	umull	r0, r2, r0, r2
	add	r2, r1, r2
	ldr	r1, [r4, #240]
	adds	r3, r0, r1
	str	r3, [r4, #96]
	adc	r2, r2, r1, asr #31
	mov	r1, #0
	lsr	r3, r3, #12
	str	r1, [r4, #252]
	orr	r3, r3, r2, lsl #20
	bic	r3, r3, r3, asr #31
	cmp	r3, #4096
	movge	r3, #4096
	strh	r3, [r4, #32]	@ movhi
	add	sp, sp, #52
	@ sp needed
	pop	{r4, r5, r6, r7, r8, r9, r10, fp, pc}
.L90:
	mov	r0, r3
	mov	r1, r6
	bl	DIVIDE
	movw	r2, #65535
	movt	r2, 1
	ldr	lr, [r4, #148]
	ldr	r10, [r4, #152]
	ldr	fp, [r4, #156]
	cmp	r0, r2
	movge	r3, r2
	ldrh	r2, [r4, #128]
	movlt	r3, r0
	asr	ip, r3, #31
	str	r2, [sp, #4]
	ldrh	r2, [r4, #130]
	str	r2, [sp, #8]
	ldrh	r2, [r4, #132]
	str	r2, [sp, #12]
	ldrh	r2, [r4, #134]
	str	r2, [sp, #16]
	ldrh	r2, [r4, #136]
	str	r2, [sp, #20]
	ldrh	r2, [r4, #138]
	str	r2, [sp, #24]
	ldrh	r2, [r4, #140]
	str	r2, [sp, #28]
	ldrh	r2, [r4, #142]
	str	r2, [sp, #32]
	ldrh	r2, [r4, #144]
	str	r2, [sp, #36]
	b	.L83
.L92:
	mov	r0, r3
	mov	r1, fp
	bl	DIVIDE
	movw	r3, #65535
	movt	r3, 1
	ldr	r9, [r4, #224]
	cmp	r0, r3
	movge	r2, r3
	ldr	r3, [r4, #228]
	movlt	r2, r0
	asr	r1, r2, #31
	str	r3, [sp, #40]
	b	.L85
.L91:
	mov	r0, r3
	mov	r1, r7
	bl	DIVIDE
	movw	r2, #65535
	movt	r2, 1
	ldr	lr, [r4, #148]
	ldr	r10, [r4, #152]
	ldr	fp, [r4, #156]
	ldr	r9, [r4, #224]
	cmp	r0, r2
	movge	r3, r2
	ldrh	r2, [r4, #128]
	movlt	r3, r0
	asr	r0, r3, #31
	str	r2, [sp, #4]
	ldrh	r2, [r4, #130]
	str	r2, [sp, #8]
	ldrh	r2, [r4, #132]
	str	r2, [sp, #12]
	ldrh	r2, [r4, #134]
	str	r2, [sp, #16]
	ldrh	r2, [r4, #136]
	str	r2, [sp, #20]
	ldrh	r2, [r4, #138]
	str	r2, [sp, #24]
	ldrh	r2, [r4, #140]
	str	r2, [sp, #28]
	ldrh	r2, [r4, #142]
	str	r2, [sp, #32]
	ldrh	r2, [r4, #144]
	str	r2, [sp, #36]
	ldr	r2, [r4, #228]
	str	r2, [sp, #40]
	b	.L84
	.size	gteRTPT_sf1lm0_nfc, .-gteRTPT_sf1lm0_nfc

.macro gteMACtoIR lm
    ldr      r2, [r0, #4*25]      @ gteMAC1
    mov      r1, #1<<15
    ldr      r12,[r0, #4*(32+31)] @ gteFLAG
    cmp      r2, r1
    subge    r2, r1, #1
    orrge    r12, #(1<<31)|(1<<24)
.if \lm
    cmp      r2, #0
    movlt    r2, #0
.else
    cmn      r2, r1
    rsblt    r2, r1, #0
.endif
    str      r2, [r0, #4*9]
#ifdef HAVE_ARMV5
    ldrd     r2, r3, [r0, #4*26]  @ gteMAC23
#else
    ldr      r2, [r0, #4*26]
    ldr      r3, [r0, #4*27]
#endif
    orrlt    r12, #(1<<31)|(1<<24)
    cmp      r2, r1
    subge    r2, r1, #1
    orrge    r12, #1<<23
    orrge    r12, #1<<31
.if \lm
    cmp      r2, #0
    movlt    r2, #0
.else
    cmn      r2, r1
    rsblt    r2, r1, #0
.endif
    orrlt    r12, #1<<23
    orrlt    r12, #1<<31
    cmp      r3, r1
    subge    r3, r1, #1
    orrge    r12, #1<<22
.if \lm
    cmp      r3, #0
    movlt    r3, #0
.else
    cmn      r3, r1
    rsblt    r3, r1, #0
.endif
    orrlt    r12, #1<<22
#ifdef HAVE_ARMV5
    strd     r2, r3, [r0, #4*10]  @ gteIR23
#else
    str      r2, [r0, #4*10]
    str      r3, [r0, #4*11]
#endif
    str      r12,[r0, #4*(32+31)] @ gteFLAG
    bx       lr
.endm

gteMACtoIR_lm0_arm_old: @ r0=CP2 (d,c)
    gteMACtoIR 0

gteMACtoIR_lm1_arm_old: @ r0=CP2 (d,c)
    gteMACtoIR 1

@ vim:expandtab
