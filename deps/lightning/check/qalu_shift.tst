#define GENTABLE	0
#define EXPANDFULL	0

#if GENTABLE
.data	128
fmt:
#  if __WORDSIZE == 32
.c	"%s(%2d, 0x%08x, %2d, 0x%08x, 0x%08x)"
#  else
.c	"%s(%2d, 0x%016lx, %2d, 0x%016lx, 0x%016lx)"
#  endif
opt_start:
.c	"\tOPTIONAL("
opt_end:
.c	")\n"
use_start:
.c	"\t"
use_end:
.c	"\n"
QLSH:
.c		" QLSH"
QLSHU:
.c		"QLSHU"
QRSH:
.c		" QRSH"
QRSHU:
.c		"QRSHU"
#else
#include "qalu.inc"
#endif

#define  QLSH(N, VAL, SH, LO, HI)	QALU(N,   , qlsh, VAL, SH, LO, HI)
#define QLSHU(N, VAL, SH, LO, HI)	QALU(N, _u, qlsh, VAL, SH, LO, HI)
#define  QRSH(N, VAL, SH, LO, HI)	QALU(N,   , qrsh, VAL, SH, LO, HI)
#define QRSHU(N, VAL, SH, LO, HI)	QALU(N, _u, qrsh, VAL, SH, LO, HI)

#if EXPANDFULL
#  define OPTIONAL(OPT) 		OPT
#else
#  define OPTIONAL(OPT) 		/**/
#endif

.code
#if GENTABLE
	jmpi main
func_qlsh:
	prolog
	arg $value
	arg $shift
	getarg %v0 $value
	getarg %v1 $shift
	allocai $((__WORDSIZE >> 3) * 2) $buf
	beqi func_qlsh_zero %v1 0
	beqi func_qlsh_overflow %v1 __WORDSIZE
	rsbi %r0 %v1 __WORDSIZE
	rshr %r1 %v0 %r0
	lshr %r0 %v0 %v1
	jmpi func_qlsh_done
func_qlsh_overflow:
	movr %r1 %v0
	movi %r0 0
	jmpi func_qlsh_done
func_qlsh_zero:
	movr %r0 %v0
	rshi %r1 %v0 $(__WORDSIZE - 1)
func_qlsh_done:
	stxi $buf %fp %r0
	stxi $($buf + (__WORDSIZE >> 3)) %fp %r1
	beqi func_qlsh_not_optional %v1 0
	beqi func_qlsh_not_optional %v1 1
	beqi func_qlsh_not_optional %v1 $(__WORDSIZE / 2 - 1)
	beqi func_qlsh_not_optional %v1 $(__WORDSIZE / 2)
	beqi func_qlsh_not_optional %v1 $(__WORDSIZE / 2 + 1)
	beqi func_qlsh_not_optional %v1 $(__WORDSIZE - 1)
	beqi func_qlsh_not_optional %v1 $(__WORDSIZE)
	jmpi func_qlsh_optional
func_qlsh_not_optional:
	prepare
		pushargi use_start
	finishi @printf
	movi %v2 0
	jmpi func_qlsh_printf
func_qlsh_optional:
	prepare
		pushargi opt_start
	finishi @printf
	movi %v2 1
func_qlsh_printf:
	ldxi %r0 %fp $buf
	ldxi %r1 %fp $($buf + (__WORDSIZE >> 3))
	prepare
		pushargi fmt
		ellipsis
		pushargi QLSH
		pushargr %v1
		pushargr %v0
		pushargr %v1
		pushargr %r0
		pushargr %r1
	finishi @printf
	beqi func_qlsh_not_optional_end %v2 0
	prepare
		pushargi opt_end
	finishi @printf
	jmpi func_qlsh_ret
func_qlsh_not_optional_end:
	prepare
		pushargi use_end
	finishi @printf
func_qlsh_ret:
	ret
	epilog

func_qlsh_u:
	prolog
	arg $value
	arg $shift
	getarg %v0 $value
	getarg %v1 $shift
	allocai $((__WORDSIZE >> 3) * 2) $buf
	beqi func_qlsh_u_zero %v1 0
	beqi func_qlsh_u_overflow %v1 __WORDSIZE
	rsbi %r0 %v1 __WORDSIZE
	rshr_u %r1 %v0 %r0
	lshr %r0 %v0 %v1
	jmpi func_qlsh_u_done
func_qlsh_u_overflow:
	movr %r1 %v0
	movi %r0 0
	jmpi func_qlsh_u_done
func_qlsh_u_zero:
	movr %r0 %v0
	movi %r1 0
func_qlsh_u_done:
	stxi $buf %fp %r0
	stxi $($buf + (__WORDSIZE >> 3)) %fp %r1
	beqi func_qlsh_u_not_optional %v1 0
	beqi func_qlsh_u_not_optional %v1 1
	beqi func_qlsh_u_not_optional %v1 $(__WORDSIZE / 2 - 1)
	beqi func_qlsh_u_not_optional %v1 $(__WORDSIZE / 2)
	beqi func_qlsh_u_not_optional %v1 $(__WORDSIZE / 2 + 1)
	beqi func_qlsh_u_not_optional %v1 $(__WORDSIZE - 1)
	beqi func_qlsh_u_not_optional %v1 $(__WORDSIZE)
	jmpi func_qlsh_u_optional
func_qlsh_u_not_optional:
	prepare
		pushargi use_start
	finishi @printf
	movi %v2 0
	jmpi func_qlsh_u_printf
func_qlsh_u_optional:
	prepare
		pushargi opt_start
	finishi @printf
	movi %v2 1
func_qlsh_u_printf:
	ldxi %r0 %fp $buf
	ldxi %r1 %fp $($buf + (__WORDSIZE >> 3))
	prepare
		pushargi fmt
		ellipsis
		pushargi QLSHU
		pushargr %v1
		pushargr %v0
		pushargr %v1
		pushargr %r0
		pushargr %r1
	finishi @printf
	beqi func_qlsh_u_not_optional_end %v2 0
	prepare
		pushargi opt_end
	finishi @printf
	jmpi func_qlsh_u_ret
func_qlsh_u_not_optional_end:
	prepare
		pushargi use_end
	finishi @printf
func_qlsh_u_ret:
	ret
	epilog

func_qrsh:
	prolog
	arg $value
	arg $shift
	getarg %v0 $value
	getarg %v1 $shift
	allocai $((__WORDSIZE >> 3) * 2) $buf
	beqi func_qrsh_zero %v1 0
	beqi func_qrsh_overflow %v1 __WORDSIZE
	rsbi %r0 %v1 __WORDSIZE
	lshr %r1 %v0 %r0
	rshr %r0 %v0 %v1
	jmpi func_qrsh_done
func_qrsh_overflow:
	movr %r1 %v0
	rshi %r0 %v0 $(__WORDSIZE - 1)
	jmpi func_qrsh_done
func_qrsh_zero:
	movr %r0 %v0
	rshi %r1 %v0 $(__WORDSIZE - 1)
func_qrsh_done:
	stxi $buf %fp %r0
	stxi $($buf + (__WORDSIZE >> 3)) %fp %r1
	beqi func_qrsh_not_optional %v1 0
	beqi func_qrsh_not_optional %v1 1
	beqi func_qrsh_not_optional %v1 $(__WORDSIZE / 2 - 1)
	beqi func_qrsh_not_optional %v1 $(__WORDSIZE / 2)
	beqi func_qrsh_not_optional %v1 $(__WORDSIZE / 2 + 1)
	beqi func_qrsh_not_optional %v1 $(__WORDSIZE - 1)
	beqi func_qrsh_not_optional %v1 $(__WORDSIZE)
	jmpi func_qrsh_optional
func_qrsh_not_optional:
	prepare
		pushargi use_start
	finishi @printf
	movi %v2 0
	jmpi func_qrsh_printf
func_qrsh_optional:
	prepare
		pushargi opt_start
	finishi @printf
	movi %v2 1
func_qrsh_printf:
	ldxi %r0 %fp $buf
	ldxi %r1 %fp $($buf + (__WORDSIZE >> 3))
	prepare
		pushargi fmt
		ellipsis
		pushargi QRSH
		pushargr %v1
		pushargr %v0
		pushargr %v1
		pushargr %r0
		pushargr %r1
	finishi @printf
	beqi func_qrsh_not_optional_end %v2 0
	prepare
		pushargi opt_end
	finishi @printf
	jmpi func_qrsh_ret
func_qrsh_not_optional_end:
	prepare
		pushargi use_end
	finishi @printf
func_qrsh_ret:
	ret
	epilog

func_qrsh_u:
	prolog
	arg $value
	arg $shift
	getarg %v0 $value
	getarg %v1 $shift
	allocai $((__WORDSIZE >> 3) * 2) $buf
	beqi func_qrsh_u_zero %v1 0
	beqi func_qrsh_u_overflow %v1 __WORDSIZE
	rsbi %r0 %v1 __WORDSIZE
	lshr %r1 %v0 %r0
	rshr_u %r0 %v0 %v1
	jmpi func_qrsh_u_done
func_qrsh_u_overflow:
	movr %r1 %v0
	movi %r0 0
	jmpi func_qrsh_u_done
func_qrsh_u_zero:
	movr %r0 %v0
	movi %r1 0
func_qrsh_u_done:
	stxi $buf %fp %r0
	stxi $($buf + (__WORDSIZE >> 3)) %fp %r1
	beqi func_qrsh_u_not_optional %v1 0
	beqi func_qrsh_u_not_optional %v1 1
	beqi func_qrsh_u_not_optional %v1 $(__WORDSIZE / 2 - 1)
	beqi func_qrsh_u_not_optional %v1 $(__WORDSIZE / 2)
	beqi func_qrsh_u_not_optional %v1 $(__WORDSIZE / 2 + 1)
	beqi func_qrsh_u_not_optional %v1 $(__WORDSIZE - 1)
	beqi func_qrsh_u_not_optional %v1 $(__WORDSIZE)
	jmpi func_qrsh_u_optional
func_qrsh_u_not_optional:
	prepare
		pushargi use_start
	finishi @printf
	movi %v2 0
	jmpi func_qrsh_u_printf
func_qrsh_u_optional:
	prepare
		pushargi opt_start
	finishi @printf
	movi %v2 1
func_qrsh_u_printf:
	ldxi %r0 %fp $buf
	ldxi %r1 %fp $($buf + (__WORDSIZE >> 3))
	prepare
		pushargi fmt
		ellipsis
		pushargi QRSHU
		pushargr %v1
		pushargr %v0
		pushargr %v1
		pushargr %r0
		pushargr %r1
	finishi @printf
	beqi func_qrsh_u_not_optional_end %v2 0
	prepare
		pushargi opt_end
	finishi @printf
	jmpi func_qrsh_u_ret
func_qrsh_u_not_optional_end:
	prepare
		pushargi use_end
	finishi @printf
func_qrsh_u_ret:
	epilog
#endif

	name main
main:
	prolog
#if GENTABLE
#  if __WORDSIZE == 32
	movi %v0 0x89abcdef
#  else
	movi %v0 0x89abcdef01234567
#  endif
	movi %v1 0
loop:
	prepare
		pushargr %v0
		pushargr %v1
	finishi func_qlsh
	prepare
		pushargr %v0
		pushargr %v1
	finishi func_qlsh_u
	prepare
		pushargr %v0
		pushargr %v1
	finishi func_qrsh
	prepare
		pushargr %v0
		pushargr %v1
	finishi func_qrsh_u
	addi %v1 %v1 1
	blei loop %v1 __WORDSIZE
#else
#  if __WORDSIZE == 32
	 QLSH( 0, 0x89abcdef,  0, 0x89abcdef, 0xffffffff)
	QLSHU( 0, 0x89abcdef,  0, 0x89abcdef, 0x00000000)
	 QRSH( 0, 0x89abcdef,  0, 0x89abcdef, 0xffffffff)
	QRSHU( 0, 0x89abcdef,  0, 0x89abcdef, 0x00000000)
	 QLSH( 1, 0x89abcdef,  1, 0x13579bde, 0xffffffff)
	QLSHU( 1, 0x89abcdef,  1, 0x13579bde, 0x00000001)
	 QRSH( 1, 0x89abcdef,  1, 0xc4d5e6f7, 0x80000000)
	QRSHU( 1, 0x89abcdef,  1, 0x44d5e6f7, 0x80000000)
	OPTIONAL( QLSH( 2, 0x89abcdef,  2, 0x26af37bc, 0xfffffffe))
	OPTIONAL(QLSHU( 2, 0x89abcdef,  2, 0x26af37bc, 0x00000002))
	OPTIONAL( QRSH( 2, 0x89abcdef,  2, 0xe26af37b, 0xc0000000))
	OPTIONAL(QRSHU( 2, 0x89abcdef,  2, 0x226af37b, 0xc0000000))
	OPTIONAL( QLSH( 3, 0x89abcdef,  3, 0x4d5e6f78, 0xfffffffc))
	OPTIONAL(QLSHU( 3, 0x89abcdef,  3, 0x4d5e6f78, 0x00000004))
	OPTIONAL( QRSH( 3, 0x89abcdef,  3, 0xf13579bd, 0xe0000000))
	OPTIONAL(QRSHU( 3, 0x89abcdef,  3, 0x113579bd, 0xe0000000))
	OPTIONAL( QLSH( 4, 0x89abcdef,  4, 0x9abcdef0, 0xfffffff8))
	OPTIONAL(QLSHU( 4, 0x89abcdef,  4, 0x9abcdef0, 0x00000008))
	OPTIONAL( QRSH( 4, 0x89abcdef,  4, 0xf89abcde, 0xf0000000))
	OPTIONAL(QRSHU( 4, 0x89abcdef,  4, 0x089abcde, 0xf0000000))
	OPTIONAL( QLSH( 5, 0x89abcdef,  5, 0x3579bde0, 0xfffffff1))
	OPTIONAL(QLSHU( 5, 0x89abcdef,  5, 0x3579bde0, 0x00000011))
	OPTIONAL( QRSH( 5, 0x89abcdef,  5, 0xfc4d5e6f, 0x78000000))
	OPTIONAL(QRSHU( 5, 0x89abcdef,  5, 0x044d5e6f, 0x78000000))
	OPTIONAL( QLSH( 6, 0x89abcdef,  6, 0x6af37bc0, 0xffffffe2))
	OPTIONAL(QLSHU( 6, 0x89abcdef,  6, 0x6af37bc0, 0x00000022))
	OPTIONAL( QRSH( 6, 0x89abcdef,  6, 0xfe26af37, 0xbc000000))
	OPTIONAL(QRSHU( 6, 0x89abcdef,  6, 0x0226af37, 0xbc000000))
	OPTIONAL( QLSH( 7, 0x89abcdef,  7, 0xd5e6f780, 0xffffffc4))
	OPTIONAL(QLSHU( 7, 0x89abcdef,  7, 0xd5e6f780, 0x00000044))
	OPTIONAL( QRSH( 7, 0x89abcdef,  7, 0xff13579b, 0xde000000))
	OPTIONAL(QRSHU( 7, 0x89abcdef,  7, 0x0113579b, 0xde000000))
	OPTIONAL( QLSH( 8, 0x89abcdef,  8, 0xabcdef00, 0xffffff89))
	OPTIONAL(QLSHU( 8, 0x89abcdef,  8, 0xabcdef00, 0x00000089))
	OPTIONAL( QRSH( 8, 0x89abcdef,  8, 0xff89abcd, 0xef000000))
	OPTIONAL(QRSHU( 8, 0x89abcdef,  8, 0x0089abcd, 0xef000000))
	OPTIONAL( QLSH( 9, 0x89abcdef,  9, 0x579bde00, 0xffffff13))
	OPTIONAL(QLSHU( 9, 0x89abcdef,  9, 0x579bde00, 0x00000113))
	OPTIONAL( QRSH( 9, 0x89abcdef,  9, 0xffc4d5e6, 0xf7800000))
	OPTIONAL(QRSHU( 9, 0x89abcdef,  9, 0x0044d5e6, 0xf7800000))
	OPTIONAL( QLSH(10, 0x89abcdef, 10, 0xaf37bc00, 0xfffffe26))
	OPTIONAL(QLSHU(10, 0x89abcdef, 10, 0xaf37bc00, 0x00000226))
	OPTIONAL( QRSH(10, 0x89abcdef, 10, 0xffe26af3, 0x7bc00000))
	OPTIONAL(QRSHU(10, 0x89abcdef, 10, 0x00226af3, 0x7bc00000))
	OPTIONAL( QLSH(11, 0x89abcdef, 11, 0x5e6f7800, 0xfffffc4d))
	OPTIONAL(QLSHU(11, 0x89abcdef, 11, 0x5e6f7800, 0x0000044d))
	OPTIONAL( QRSH(11, 0x89abcdef, 11, 0xfff13579, 0xbde00000))
	OPTIONAL(QRSHU(11, 0x89abcdef, 11, 0x00113579, 0xbde00000))
	OPTIONAL( QLSH(12, 0x89abcdef, 12, 0xbcdef000, 0xfffff89a))
	OPTIONAL(QLSHU(12, 0x89abcdef, 12, 0xbcdef000, 0x0000089a))
	OPTIONAL( QRSH(12, 0x89abcdef, 12, 0xfff89abc, 0xdef00000))
	OPTIONAL(QRSHU(12, 0x89abcdef, 12, 0x00089abc, 0xdef00000))
	OPTIONAL( QLSH(13, 0x89abcdef, 13, 0x79bde000, 0xfffff135))
	OPTIONAL(QLSHU(13, 0x89abcdef, 13, 0x79bde000, 0x00001135))
	OPTIONAL( QRSH(13, 0x89abcdef, 13, 0xfffc4d5e, 0x6f780000))
	OPTIONAL(QRSHU(13, 0x89abcdef, 13, 0x00044d5e, 0x6f780000))
	OPTIONAL( QLSH(14, 0x89abcdef, 14, 0xf37bc000, 0xffffe26a))
	OPTIONAL(QLSHU(14, 0x89abcdef, 14, 0xf37bc000, 0x0000226a))
	OPTIONAL( QRSH(14, 0x89abcdef, 14, 0xfffe26af, 0x37bc0000))
	OPTIONAL(QRSHU(14, 0x89abcdef, 14, 0x000226af, 0x37bc0000))
	 QLSH(15, 0x89abcdef, 15, 0xe6f78000, 0xffffc4d5)
	QLSHU(15, 0x89abcdef, 15, 0xe6f78000, 0x000044d5)
	 QRSH(15, 0x89abcdef, 15, 0xffff1357, 0x9bde0000)
	QRSHU(15, 0x89abcdef, 15, 0x00011357, 0x9bde0000)
	 QLSH(16, 0x89abcdef, 16, 0xcdef0000, 0xffff89ab)
	QLSHU(16, 0x89abcdef, 16, 0xcdef0000, 0x000089ab)
	 QRSH(16, 0x89abcdef, 16, 0xffff89ab, 0xcdef0000)
	QRSHU(16, 0x89abcdef, 16, 0x000089ab, 0xcdef0000)
	 QLSH(17, 0x89abcdef, 17, 0x9bde0000, 0xffff1357)
	QLSHU(17, 0x89abcdef, 17, 0x9bde0000, 0x00011357)
	 QRSH(17, 0x89abcdef, 17, 0xffffc4d5, 0xe6f78000)
	QRSHU(17, 0x89abcdef, 17, 0x000044d5, 0xe6f78000)
	OPTIONAL( QLSH(18, 0x89abcdef, 18, 0x37bc0000, 0xfffe26af))
	OPTIONAL(QLSHU(18, 0x89abcdef, 18, 0x37bc0000, 0x000226af))
	OPTIONAL( QRSH(18, 0x89abcdef, 18, 0xffffe26a, 0xf37bc000))
	OPTIONAL(QRSHU(18, 0x89abcdef, 18, 0x0000226a, 0xf37bc000))
	OPTIONAL( QLSH(19, 0x89abcdef, 19, 0x6f780000, 0xfffc4d5e))
	OPTIONAL(QLSHU(19, 0x89abcdef, 19, 0x6f780000, 0x00044d5e))
	OPTIONAL( QRSH(19, 0x89abcdef, 19, 0xfffff135, 0x79bde000))
	OPTIONAL(QRSHU(19, 0x89abcdef, 19, 0x00001135, 0x79bde000))
	OPTIONAL( QLSH(20, 0x89abcdef, 20, 0xdef00000, 0xfff89abc))
	OPTIONAL(QLSHU(20, 0x89abcdef, 20, 0xdef00000, 0x00089abc))
	OPTIONAL( QRSH(20, 0x89abcdef, 20, 0xfffff89a, 0xbcdef000))
	OPTIONAL(QRSHU(20, 0x89abcdef, 20, 0x0000089a, 0xbcdef000))
	OPTIONAL( QLSH(21, 0x89abcdef, 21, 0xbde00000, 0xfff13579))
	OPTIONAL(QLSHU(21, 0x89abcdef, 21, 0xbde00000, 0x00113579))
	OPTIONAL( QRSH(21, 0x89abcdef, 21, 0xfffffc4d, 0x5e6f7800))
	OPTIONAL(QRSHU(21, 0x89abcdef, 21, 0x0000044d, 0x5e6f7800))
	OPTIONAL( QLSH(22, 0x89abcdef, 22, 0x7bc00000, 0xffe26af3))
	OPTIONAL(QLSHU(22, 0x89abcdef, 22, 0x7bc00000, 0x00226af3))
	OPTIONAL( QRSH(22, 0x89abcdef, 22, 0xfffffe26, 0xaf37bc00))
	OPTIONAL(QRSHU(22, 0x89abcdef, 22, 0x00000226, 0xaf37bc00))
	OPTIONAL( QLSH(23, 0x89abcdef, 23, 0xf7800000, 0xffc4d5e6))
	OPTIONAL(QLSHU(23, 0x89abcdef, 23, 0xf7800000, 0x0044d5e6))
	OPTIONAL( QRSH(23, 0x89abcdef, 23, 0xffffff13, 0x579bde00))
	OPTIONAL(QRSHU(23, 0x89abcdef, 23, 0x00000113, 0x579bde00))
	OPTIONAL( QLSH(24, 0x89abcdef, 24, 0xef000000, 0xff89abcd))
	OPTIONAL(QLSHU(24, 0x89abcdef, 24, 0xef000000, 0x0089abcd))
	OPTIONAL( QRSH(24, 0x89abcdef, 24, 0xffffff89, 0xabcdef00))
	OPTIONAL(QRSHU(24, 0x89abcdef, 24, 0x00000089, 0xabcdef00))
	OPTIONAL( QLSH(25, 0x89abcdef, 25, 0xde000000, 0xff13579b))
	OPTIONAL(QLSHU(25, 0x89abcdef, 25, 0xde000000, 0x0113579b))
	OPTIONAL( QRSH(25, 0x89abcdef, 25, 0xffffffc4, 0xd5e6f780))
	OPTIONAL(QRSHU(25, 0x89abcdef, 25, 0x00000044, 0xd5e6f780))
	OPTIONAL( QLSH(26, 0x89abcdef, 26, 0xbc000000, 0xfe26af37))
	OPTIONAL(QLSHU(26, 0x89abcdef, 26, 0xbc000000, 0x0226af37))
	OPTIONAL( QRSH(26, 0x89abcdef, 26, 0xffffffe2, 0x6af37bc0))
	OPTIONAL(QRSHU(26, 0x89abcdef, 26, 0x00000022, 0x6af37bc0))
	OPTIONAL( QLSH(27, 0x89abcdef, 27, 0x78000000, 0xfc4d5e6f))
	OPTIONAL(QLSHU(27, 0x89abcdef, 27, 0x78000000, 0x044d5e6f))
	OPTIONAL( QRSH(27, 0x89abcdef, 27, 0xfffffff1, 0x3579bde0))
	OPTIONAL(QRSHU(27, 0x89abcdef, 27, 0x00000011, 0x3579bde0))
	OPTIONAL( QLSH(28, 0x89abcdef, 28, 0xf0000000, 0xf89abcde))
	OPTIONAL(QLSHU(28, 0x89abcdef, 28, 0xf0000000, 0x089abcde))
	OPTIONAL( QRSH(28, 0x89abcdef, 28, 0xfffffff8, 0x9abcdef0))
	OPTIONAL(QRSHU(28, 0x89abcdef, 28, 0x00000008, 0x9abcdef0))
	OPTIONAL( QLSH(29, 0x89abcdef, 29, 0xe0000000, 0xf13579bd))
	OPTIONAL(QLSHU(29, 0x89abcdef, 29, 0xe0000000, 0x113579bd))
	OPTIONAL( QRSH(29, 0x89abcdef, 29, 0xfffffffc, 0x4d5e6f78))
	OPTIONAL(QRSHU(29, 0x89abcdef, 29, 0x00000004, 0x4d5e6f78))
	OPTIONAL( QLSH(30, 0x89abcdef, 30, 0xc0000000, 0xe26af37b))
	OPTIONAL(QLSHU(30, 0x89abcdef, 30, 0xc0000000, 0x226af37b))
	OPTIONAL( QRSH(30, 0x89abcdef, 30, 0xfffffffe, 0x26af37bc))
	OPTIONAL(QRSHU(30, 0x89abcdef, 30, 0x00000002, 0x26af37bc))
	 QLSH(31, 0x89abcdef, 31, 0x80000000, 0xc4d5e6f7)
	QLSHU(31, 0x89abcdef, 31, 0x80000000, 0x44d5e6f7)
	 QRSH(31, 0x89abcdef, 31, 0xffffffff, 0x13579bde)
	QRSHU(31, 0x89abcdef, 31, 0x00000001, 0x13579bde)
	 QLSH(32, 0x89abcdef, 32, 0x00000000, 0x89abcdef)
	QLSHU(32, 0x89abcdef, 32, 0x00000000, 0x89abcdef)
	 QRSH(32, 0x89abcdef, 32, 0xffffffff, 0x89abcdef)
	QRSHU(32, 0x89abcdef, 32, 0x00000000, 0x89abcdef)
#  else
	 QLSH( 0, 0x89abcdef01234567,  0, 0x89abcdef01234567, 0xffffffffffffffff)
	QLSHU( 0, 0x89abcdef01234567,  0, 0x89abcdef01234567, 0x0000000000000000)
	 QRSH( 0, 0x89abcdef01234567,  0, 0x89abcdef01234567, 0xffffffffffffffff)
	QRSHU( 0, 0x89abcdef01234567,  0, 0x89abcdef01234567, 0x0000000000000000)
	 QLSH( 1, 0x89abcdef01234567,  1, 0x13579bde02468ace, 0xffffffffffffffff)
	QLSHU( 1, 0x89abcdef01234567,  1, 0x13579bde02468ace, 0x0000000000000001)
	 QRSH( 1, 0x89abcdef01234567,  1, 0xc4d5e6f78091a2b3, 0x8000000000000000)
	QRSHU( 1, 0x89abcdef01234567,  1, 0x44d5e6f78091a2b3, 0x8000000000000000)
	OPTIONAL( QLSH( 2, 0x89abcdef01234567,  2, 0x26af37bc048d159c, 0xfffffffffffffffe))
	OPTIONAL(QLSHU( 2, 0x89abcdef01234567,  2, 0x26af37bc048d159c, 0x0000000000000002))
	OPTIONAL( QRSH( 2, 0x89abcdef01234567,  2, 0xe26af37bc048d159, 0xc000000000000000))
	OPTIONAL(QRSHU( 2, 0x89abcdef01234567,  2, 0x226af37bc048d159, 0xc000000000000000))
	OPTIONAL( QLSH( 3, 0x89abcdef01234567,  3, 0x4d5e6f78091a2b38, 0xfffffffffffffffc))
	OPTIONAL(QLSHU( 3, 0x89abcdef01234567,  3, 0x4d5e6f78091a2b38, 0x0000000000000004))
	OPTIONAL( QRSH( 3, 0x89abcdef01234567,  3, 0xf13579bde02468ac, 0xe000000000000000))
	OPTIONAL(QRSHU( 3, 0x89abcdef01234567,  3, 0x113579bde02468ac, 0xe000000000000000))
	OPTIONAL( QLSH( 4, 0x89abcdef01234567,  4, 0x9abcdef012345670, 0xfffffffffffffff8))
	OPTIONAL(QLSHU( 4, 0x89abcdef01234567,  4, 0x9abcdef012345670, 0x0000000000000008))
	OPTIONAL( QRSH( 4, 0x89abcdef01234567,  4, 0xf89abcdef0123456, 0x7000000000000000))
	OPTIONAL(QRSHU( 4, 0x89abcdef01234567,  4, 0x089abcdef0123456, 0x7000000000000000))
	OPTIONAL( QLSH( 5, 0x89abcdef01234567,  5, 0x3579bde02468ace0, 0xfffffffffffffff1))
	OPTIONAL(QLSHU( 5, 0x89abcdef01234567,  5, 0x3579bde02468ace0, 0x0000000000000011))
	OPTIONAL( QRSH( 5, 0x89abcdef01234567,  5, 0xfc4d5e6f78091a2b, 0x3800000000000000))
	OPTIONAL(QRSHU( 5, 0x89abcdef01234567,  5, 0x044d5e6f78091a2b, 0x3800000000000000))
	OPTIONAL( QLSH( 6, 0x89abcdef01234567,  6, 0x6af37bc048d159c0, 0xffffffffffffffe2))
	OPTIONAL(QLSHU( 6, 0x89abcdef01234567,  6, 0x6af37bc048d159c0, 0x0000000000000022))
	OPTIONAL( QRSH( 6, 0x89abcdef01234567,  6, 0xfe26af37bc048d15, 0x9c00000000000000))
	OPTIONAL(QRSHU( 6, 0x89abcdef01234567,  6, 0x0226af37bc048d15, 0x9c00000000000000))
	OPTIONAL( QLSH( 7, 0x89abcdef01234567,  7, 0xd5e6f78091a2b380, 0xffffffffffffffc4))
	OPTIONAL(QLSHU( 7, 0x89abcdef01234567,  7, 0xd5e6f78091a2b380, 0x0000000000000044))
	OPTIONAL( QRSH( 7, 0x89abcdef01234567,  7, 0xff13579bde02468a, 0xce00000000000000))
	OPTIONAL(QRSHU( 7, 0x89abcdef01234567,  7, 0x0113579bde02468a, 0xce00000000000000))
	OPTIONAL( QLSH( 8, 0x89abcdef01234567,  8, 0xabcdef0123456700, 0xffffffffffffff89))
	OPTIONAL(QLSHU( 8, 0x89abcdef01234567,  8, 0xabcdef0123456700, 0x0000000000000089))
	OPTIONAL( QRSH( 8, 0x89abcdef01234567,  8, 0xff89abcdef012345, 0x6700000000000000))
	OPTIONAL(QRSHU( 8, 0x89abcdef01234567,  8, 0x0089abcdef012345, 0x6700000000000000))
	OPTIONAL( QLSH( 9, 0x89abcdef01234567,  9, 0x579bde02468ace00, 0xffffffffffffff13))
	OPTIONAL(QLSHU( 9, 0x89abcdef01234567,  9, 0x579bde02468ace00, 0x0000000000000113))
	OPTIONAL( QRSH( 9, 0x89abcdef01234567,  9, 0xffc4d5e6f78091a2, 0xb380000000000000))
	OPTIONAL(QRSHU( 9, 0x89abcdef01234567,  9, 0x0044d5e6f78091a2, 0xb380000000000000))
	OPTIONAL( QLSH(10, 0x89abcdef01234567, 10, 0xaf37bc048d159c00, 0xfffffffffffffe26))
	OPTIONAL(QLSHU(10, 0x89abcdef01234567, 10, 0xaf37bc048d159c00, 0x0000000000000226))
	OPTIONAL( QRSH(10, 0x89abcdef01234567, 10, 0xffe26af37bc048d1, 0x59c0000000000000))
	OPTIONAL(QRSHU(10, 0x89abcdef01234567, 10, 0x00226af37bc048d1, 0x59c0000000000000))
	OPTIONAL( QLSH(11, 0x89abcdef01234567, 11, 0x5e6f78091a2b3800, 0xfffffffffffffc4d))
	OPTIONAL(QLSHU(11, 0x89abcdef01234567, 11, 0x5e6f78091a2b3800, 0x000000000000044d))
	OPTIONAL( QRSH(11, 0x89abcdef01234567, 11, 0xfff13579bde02468, 0xace0000000000000))
	OPTIONAL(QRSHU(11, 0x89abcdef01234567, 11, 0x00113579bde02468, 0xace0000000000000))
	OPTIONAL( QLSH(12, 0x89abcdef01234567, 12, 0xbcdef01234567000, 0xfffffffffffff89a))
	OPTIONAL(QLSHU(12, 0x89abcdef01234567, 12, 0xbcdef01234567000, 0x000000000000089a))
	OPTIONAL( QRSH(12, 0x89abcdef01234567, 12, 0xfff89abcdef01234, 0x5670000000000000))
	OPTIONAL(QRSHU(12, 0x89abcdef01234567, 12, 0x00089abcdef01234, 0x5670000000000000))
	OPTIONAL( QLSH(13, 0x89abcdef01234567, 13, 0x79bde02468ace000, 0xfffffffffffff135))
	OPTIONAL(QLSHU(13, 0x89abcdef01234567, 13, 0x79bde02468ace000, 0x0000000000001135))
	OPTIONAL( QRSH(13, 0x89abcdef01234567, 13, 0xfffc4d5e6f78091a, 0x2b38000000000000))
	OPTIONAL(QRSHU(13, 0x89abcdef01234567, 13, 0x00044d5e6f78091a, 0x2b38000000000000))
	OPTIONAL( QLSH(14, 0x89abcdef01234567, 14, 0xf37bc048d159c000, 0xffffffffffffe26a))
	OPTIONAL(QLSHU(14, 0x89abcdef01234567, 14, 0xf37bc048d159c000, 0x000000000000226a))
	OPTIONAL( QRSH(14, 0x89abcdef01234567, 14, 0xfffe26af37bc048d, 0x159c000000000000))
	OPTIONAL(QRSHU(14, 0x89abcdef01234567, 14, 0x000226af37bc048d, 0x159c000000000000))
	OPTIONAL( QLSH(15, 0x89abcdef01234567, 15, 0xe6f78091a2b38000, 0xffffffffffffc4d5))
	OPTIONAL(QLSHU(15, 0x89abcdef01234567, 15, 0xe6f78091a2b38000, 0x00000000000044d5))
	OPTIONAL( QRSH(15, 0x89abcdef01234567, 15, 0xffff13579bde0246, 0x8ace000000000000))
	OPTIONAL(QRSHU(15, 0x89abcdef01234567, 15, 0x000113579bde0246, 0x8ace000000000000))
	OPTIONAL( QLSH(16, 0x89abcdef01234567, 16, 0xcdef012345670000, 0xffffffffffff89ab))
	OPTIONAL(QLSHU(16, 0x89abcdef01234567, 16, 0xcdef012345670000, 0x00000000000089ab))
	OPTIONAL( QRSH(16, 0x89abcdef01234567, 16, 0xffff89abcdef0123, 0x4567000000000000))
	OPTIONAL(QRSHU(16, 0x89abcdef01234567, 16, 0x000089abcdef0123, 0x4567000000000000))
	OPTIONAL( QLSH(17, 0x89abcdef01234567, 17, 0x9bde02468ace0000, 0xffffffffffff1357))
	OPTIONAL(QLSHU(17, 0x89abcdef01234567, 17, 0x9bde02468ace0000, 0x0000000000011357))
	OPTIONAL( QRSH(17, 0x89abcdef01234567, 17, 0xffffc4d5e6f78091, 0xa2b3800000000000))
	OPTIONAL(QRSHU(17, 0x89abcdef01234567, 17, 0x000044d5e6f78091, 0xa2b3800000000000))
	OPTIONAL( QLSH(18, 0x89abcdef01234567, 18, 0x37bc048d159c0000, 0xfffffffffffe26af))
	OPTIONAL(QLSHU(18, 0x89abcdef01234567, 18, 0x37bc048d159c0000, 0x00000000000226af))
	OPTIONAL( QRSH(18, 0x89abcdef01234567, 18, 0xffffe26af37bc048, 0xd159c00000000000))
	OPTIONAL(QRSHU(18, 0x89abcdef01234567, 18, 0x0000226af37bc048, 0xd159c00000000000))
	OPTIONAL( QLSH(19, 0x89abcdef01234567, 19, 0x6f78091a2b380000, 0xfffffffffffc4d5e))
	OPTIONAL(QLSHU(19, 0x89abcdef01234567, 19, 0x6f78091a2b380000, 0x0000000000044d5e))
	OPTIONAL( QRSH(19, 0x89abcdef01234567, 19, 0xfffff13579bde024, 0x68ace00000000000))
	OPTIONAL(QRSHU(19, 0x89abcdef01234567, 19, 0x0000113579bde024, 0x68ace00000000000))
	OPTIONAL( QLSH(20, 0x89abcdef01234567, 20, 0xdef0123456700000, 0xfffffffffff89abc))
	OPTIONAL(QLSHU(20, 0x89abcdef01234567, 20, 0xdef0123456700000, 0x0000000000089abc))
	OPTIONAL( QRSH(20, 0x89abcdef01234567, 20, 0xfffff89abcdef012, 0x3456700000000000))
	OPTIONAL(QRSHU(20, 0x89abcdef01234567, 20, 0x0000089abcdef012, 0x3456700000000000))
	OPTIONAL( QLSH(21, 0x89abcdef01234567, 21, 0xbde02468ace00000, 0xfffffffffff13579))
	OPTIONAL(QLSHU(21, 0x89abcdef01234567, 21, 0xbde02468ace00000, 0x0000000000113579))
	OPTIONAL( QRSH(21, 0x89abcdef01234567, 21, 0xfffffc4d5e6f7809, 0x1a2b380000000000))
	OPTIONAL(QRSHU(21, 0x89abcdef01234567, 21, 0x0000044d5e6f7809, 0x1a2b380000000000))
	OPTIONAL( QLSH(22, 0x89abcdef01234567, 22, 0x7bc048d159c00000, 0xffffffffffe26af3))
	OPTIONAL(QLSHU(22, 0x89abcdef01234567, 22, 0x7bc048d159c00000, 0x0000000000226af3))
	OPTIONAL( QRSH(22, 0x89abcdef01234567, 22, 0xfffffe26af37bc04, 0x8d159c0000000000))
	OPTIONAL(QRSHU(22, 0x89abcdef01234567, 22, 0x00000226af37bc04, 0x8d159c0000000000))
	OPTIONAL( QLSH(23, 0x89abcdef01234567, 23, 0xf78091a2b3800000, 0xffffffffffc4d5e6))
	OPTIONAL(QLSHU(23, 0x89abcdef01234567, 23, 0xf78091a2b3800000, 0x000000000044d5e6))
	OPTIONAL( QRSH(23, 0x89abcdef01234567, 23, 0xffffff13579bde02, 0x468ace0000000000))
	OPTIONAL(QRSHU(23, 0x89abcdef01234567, 23, 0x00000113579bde02, 0x468ace0000000000))
	OPTIONAL( QLSH(24, 0x89abcdef01234567, 24, 0xef01234567000000, 0xffffffffff89abcd))
	OPTIONAL(QLSHU(24, 0x89abcdef01234567, 24, 0xef01234567000000, 0x000000000089abcd))
	OPTIONAL( QRSH(24, 0x89abcdef01234567, 24, 0xffffff89abcdef01, 0x2345670000000000))
	OPTIONAL(QRSHU(24, 0x89abcdef01234567, 24, 0x00000089abcdef01, 0x2345670000000000))
	OPTIONAL( QLSH(25, 0x89abcdef01234567, 25, 0xde02468ace000000, 0xffffffffff13579b))
	OPTIONAL(QLSHU(25, 0x89abcdef01234567, 25, 0xde02468ace000000, 0x000000000113579b))
	OPTIONAL( QRSH(25, 0x89abcdef01234567, 25, 0xffffffc4d5e6f780, 0x91a2b38000000000))
	OPTIONAL(QRSHU(25, 0x89abcdef01234567, 25, 0x00000044d5e6f780, 0x91a2b38000000000))
	OPTIONAL( QLSH(26, 0x89abcdef01234567, 26, 0xbc048d159c000000, 0xfffffffffe26af37))
	OPTIONAL(QLSHU(26, 0x89abcdef01234567, 26, 0xbc048d159c000000, 0x000000000226af37))
	OPTIONAL( QRSH(26, 0x89abcdef01234567, 26, 0xffffffe26af37bc0, 0x48d159c000000000))
	OPTIONAL(QRSHU(26, 0x89abcdef01234567, 26, 0x000000226af37bc0, 0x48d159c000000000))
	OPTIONAL( QLSH(27, 0x89abcdef01234567, 27, 0x78091a2b38000000, 0xfffffffffc4d5e6f))
	OPTIONAL(QLSHU(27, 0x89abcdef01234567, 27, 0x78091a2b38000000, 0x00000000044d5e6f))
	OPTIONAL( QRSH(27, 0x89abcdef01234567, 27, 0xfffffff13579bde0, 0x2468ace000000000))
	OPTIONAL(QRSHU(27, 0x89abcdef01234567, 27, 0x000000113579bde0, 0x2468ace000000000))
	OPTIONAL( QLSH(28, 0x89abcdef01234567, 28, 0xf012345670000000, 0xfffffffff89abcde))
	OPTIONAL(QLSHU(28, 0x89abcdef01234567, 28, 0xf012345670000000, 0x00000000089abcde))
	OPTIONAL( QRSH(28, 0x89abcdef01234567, 28, 0xfffffff89abcdef0, 0x1234567000000000))
	OPTIONAL(QRSHU(28, 0x89abcdef01234567, 28, 0x000000089abcdef0, 0x1234567000000000))
	OPTIONAL( QLSH(29, 0x89abcdef01234567, 29, 0xe02468ace0000000, 0xfffffffff13579bd))
	OPTIONAL(QLSHU(29, 0x89abcdef01234567, 29, 0xe02468ace0000000, 0x00000000113579bd))
	OPTIONAL( QRSH(29, 0x89abcdef01234567, 29, 0xfffffffc4d5e6f78, 0x091a2b3800000000))
	OPTIONAL(QRSHU(29, 0x89abcdef01234567, 29, 0x000000044d5e6f78, 0x091a2b3800000000))
	OPTIONAL( QLSH(30, 0x89abcdef01234567, 30, 0xc048d159c0000000, 0xffffffffe26af37b))
	OPTIONAL(QLSHU(30, 0x89abcdef01234567, 30, 0xc048d159c0000000, 0x00000000226af37b))
	OPTIONAL( QRSH(30, 0x89abcdef01234567, 30, 0xfffffffe26af37bc, 0x048d159c00000000))
	OPTIONAL(QRSHU(30, 0x89abcdef01234567, 30, 0x0000000226af37bc, 0x048d159c00000000))
	 QLSH(31, 0x89abcdef01234567, 31, 0x8091a2b380000000, 0xffffffffc4d5e6f7)
	QLSHU(31, 0x89abcdef01234567, 31, 0x8091a2b380000000, 0x0000000044d5e6f7)
	 QRSH(31, 0x89abcdef01234567, 31, 0xffffffff13579bde, 0x02468ace00000000)
	QRSHU(31, 0x89abcdef01234567, 31, 0x0000000113579bde, 0x02468ace00000000)
	 QLSH(32, 0x89abcdef01234567, 32, 0x0123456700000000, 0xffffffff89abcdef)
	QLSHU(32, 0x89abcdef01234567, 32, 0x0123456700000000, 0x0000000089abcdef)
	 QRSH(32, 0x89abcdef01234567, 32, 0xffffffff89abcdef, 0x0123456700000000)
	QRSHU(32, 0x89abcdef01234567, 32, 0x0000000089abcdef, 0x0123456700000000)
	 QLSH(33, 0x89abcdef01234567, 33, 0x02468ace00000000, 0xffffffff13579bde)
	QLSHU(33, 0x89abcdef01234567, 33, 0x02468ace00000000, 0x0000000113579bde)
	 QRSH(33, 0x89abcdef01234567, 33, 0xffffffffc4d5e6f7, 0x8091a2b380000000)
	QRSHU(33, 0x89abcdef01234567, 33, 0x0000000044d5e6f7, 0x8091a2b380000000)
	OPTIONAL( QLSH(34, 0x89abcdef01234567, 34, 0x048d159c00000000, 0xfffffffe26af37bc))
	OPTIONAL(QLSHU(34, 0x89abcdef01234567, 34, 0x048d159c00000000, 0x0000000226af37bc))
	OPTIONAL( QRSH(34, 0x89abcdef01234567, 34, 0xffffffffe26af37b, 0xc048d159c0000000))
	OPTIONAL(QRSHU(34, 0x89abcdef01234567, 34, 0x00000000226af37b, 0xc048d159c0000000))
	OPTIONAL( QLSH(35, 0x89abcdef01234567, 35, 0x091a2b3800000000, 0xfffffffc4d5e6f78))
	OPTIONAL(QLSHU(35, 0x89abcdef01234567, 35, 0x091a2b3800000000, 0x000000044d5e6f78))
	OPTIONAL( QRSH(35, 0x89abcdef01234567, 35, 0xfffffffff13579bd, 0xe02468ace0000000))
	OPTIONAL(QRSHU(35, 0x89abcdef01234567, 35, 0x00000000113579bd, 0xe02468ace0000000))
	OPTIONAL( QLSH(36, 0x89abcdef01234567, 36, 0x1234567000000000, 0xfffffff89abcdef0))
	OPTIONAL(QLSHU(36, 0x89abcdef01234567, 36, 0x1234567000000000, 0x000000089abcdef0))
	OPTIONAL( QRSH(36, 0x89abcdef01234567, 36, 0xfffffffff89abcde, 0xf012345670000000))
	OPTIONAL(QRSHU(36, 0x89abcdef01234567, 36, 0x00000000089abcde, 0xf012345670000000))
	OPTIONAL( QLSH(37, 0x89abcdef01234567, 37, 0x2468ace000000000, 0xfffffff13579bde0))
	OPTIONAL(QLSHU(37, 0x89abcdef01234567, 37, 0x2468ace000000000, 0x000000113579bde0))
	OPTIONAL( QRSH(37, 0x89abcdef01234567, 37, 0xfffffffffc4d5e6f, 0x78091a2b38000000))
	OPTIONAL(QRSHU(37, 0x89abcdef01234567, 37, 0x00000000044d5e6f, 0x78091a2b38000000))
	OPTIONAL( QLSH(38, 0x89abcdef01234567, 38, 0x48d159c000000000, 0xffffffe26af37bc0))
	OPTIONAL(QLSHU(38, 0x89abcdef01234567, 38, 0x48d159c000000000, 0x000000226af37bc0))
	OPTIONAL( QRSH(38, 0x89abcdef01234567, 38, 0xfffffffffe26af37, 0xbc048d159c000000))
	OPTIONAL(QRSHU(38, 0x89abcdef01234567, 38, 0x000000000226af37, 0xbc048d159c000000))
	OPTIONAL( QLSH(39, 0x89abcdef01234567, 39, 0x91a2b38000000000, 0xffffffc4d5e6f780))
	OPTIONAL(QLSHU(39, 0x89abcdef01234567, 39, 0x91a2b38000000000, 0x00000044d5e6f780))
	OPTIONAL( QRSH(39, 0x89abcdef01234567, 39, 0xffffffffff13579b, 0xde02468ace000000))
	OPTIONAL(QRSHU(39, 0x89abcdef01234567, 39, 0x000000000113579b, 0xde02468ace000000))
	OPTIONAL( QLSH(40, 0x89abcdef01234567, 40, 0x2345670000000000, 0xffffff89abcdef01))
	OPTIONAL(QLSHU(40, 0x89abcdef01234567, 40, 0x2345670000000000, 0x00000089abcdef01))
	OPTIONAL( QRSH(40, 0x89abcdef01234567, 40, 0xffffffffff89abcd, 0xef01234567000000))
	OPTIONAL(QRSHU(40, 0x89abcdef01234567, 40, 0x000000000089abcd, 0xef01234567000000))
	OPTIONAL( QLSH(41, 0x89abcdef01234567, 41, 0x468ace0000000000, 0xffffff13579bde02))
	OPTIONAL(QLSHU(41, 0x89abcdef01234567, 41, 0x468ace0000000000, 0x00000113579bde02))
	OPTIONAL( QRSH(41, 0x89abcdef01234567, 41, 0xffffffffffc4d5e6, 0xf78091a2b3800000))
	OPTIONAL(QRSHU(41, 0x89abcdef01234567, 41, 0x000000000044d5e6, 0xf78091a2b3800000))
	OPTIONAL( QLSH(42, 0x89abcdef01234567, 42, 0x8d159c0000000000, 0xfffffe26af37bc04))
	OPTIONAL(QLSHU(42, 0x89abcdef01234567, 42, 0x8d159c0000000000, 0x00000226af37bc04))
	OPTIONAL( QRSH(42, 0x89abcdef01234567, 42, 0xffffffffffe26af3, 0x7bc048d159c00000))
	OPTIONAL(QRSHU(42, 0x89abcdef01234567, 42, 0x0000000000226af3, 0x7bc048d159c00000))
	OPTIONAL( QLSH(43, 0x89abcdef01234567, 43, 0x1a2b380000000000, 0xfffffc4d5e6f7809))
	OPTIONAL(QLSHU(43, 0x89abcdef01234567, 43, 0x1a2b380000000000, 0x0000044d5e6f7809))
	OPTIONAL( QRSH(43, 0x89abcdef01234567, 43, 0xfffffffffff13579, 0xbde02468ace00000))
	OPTIONAL(QRSHU(43, 0x89abcdef01234567, 43, 0x0000000000113579, 0xbde02468ace00000))
	OPTIONAL( QLSH(44, 0x89abcdef01234567, 44, 0x3456700000000000, 0xfffff89abcdef012))
	OPTIONAL(QLSHU(44, 0x89abcdef01234567, 44, 0x3456700000000000, 0x0000089abcdef012))
	OPTIONAL( QRSH(44, 0x89abcdef01234567, 44, 0xfffffffffff89abc, 0xdef0123456700000))
	OPTIONAL(QRSHU(44, 0x89abcdef01234567, 44, 0x0000000000089abc, 0xdef0123456700000))
	OPTIONAL( QLSH(45, 0x89abcdef01234567, 45, 0x68ace00000000000, 0xfffff13579bde024))
	OPTIONAL(QLSHU(45, 0x89abcdef01234567, 45, 0x68ace00000000000, 0x0000113579bde024))
	OPTIONAL( QRSH(45, 0x89abcdef01234567, 45, 0xfffffffffffc4d5e, 0x6f78091a2b380000))
	OPTIONAL(QRSHU(45, 0x89abcdef01234567, 45, 0x0000000000044d5e, 0x6f78091a2b380000))
	OPTIONAL( QLSH(46, 0x89abcdef01234567, 46, 0xd159c00000000000, 0xffffe26af37bc048))
	OPTIONAL(QLSHU(46, 0x89abcdef01234567, 46, 0xd159c00000000000, 0x0000226af37bc048))
	OPTIONAL( QRSH(46, 0x89abcdef01234567, 46, 0xfffffffffffe26af, 0x37bc048d159c0000))
	OPTIONAL(QRSHU(46, 0x89abcdef01234567, 46, 0x00000000000226af, 0x37bc048d159c0000))
	OPTIONAL( QLSH(47, 0x89abcdef01234567, 47, 0xa2b3800000000000, 0xffffc4d5e6f78091))
	OPTIONAL(QLSHU(47, 0x89abcdef01234567, 47, 0xa2b3800000000000, 0x000044d5e6f78091))
	OPTIONAL( QRSH(47, 0x89abcdef01234567, 47, 0xffffffffffff1357, 0x9bde02468ace0000))
	OPTIONAL(QRSHU(47, 0x89abcdef01234567, 47, 0x0000000000011357, 0x9bde02468ace0000))
	OPTIONAL( QLSH(48, 0x89abcdef01234567, 48, 0x4567000000000000, 0xffff89abcdef0123))
	OPTIONAL(QLSHU(48, 0x89abcdef01234567, 48, 0x4567000000000000, 0x000089abcdef0123))
	OPTIONAL( QRSH(48, 0x89abcdef01234567, 48, 0xffffffffffff89ab, 0xcdef012345670000))
	OPTIONAL(QRSHU(48, 0x89abcdef01234567, 48, 0x00000000000089ab, 0xcdef012345670000))
	OPTIONAL( QLSH(49, 0x89abcdef01234567, 49, 0x8ace000000000000, 0xffff13579bde0246))
	OPTIONAL(QLSHU(49, 0x89abcdef01234567, 49, 0x8ace000000000000, 0x000113579bde0246))
	OPTIONAL( QRSH(49, 0x89abcdef01234567, 49, 0xffffffffffffc4d5, 0xe6f78091a2b38000))
	OPTIONAL(QRSHU(49, 0x89abcdef01234567, 49, 0x00000000000044d5, 0xe6f78091a2b38000))
	OPTIONAL( QLSH(50, 0x89abcdef01234567, 50, 0x159c000000000000, 0xfffe26af37bc048d))
	OPTIONAL(QLSHU(50, 0x89abcdef01234567, 50, 0x159c000000000000, 0x000226af37bc048d))
	OPTIONAL( QRSH(50, 0x89abcdef01234567, 50, 0xffffffffffffe26a, 0xf37bc048d159c000))
	OPTIONAL(QRSHU(50, 0x89abcdef01234567, 50, 0x000000000000226a, 0xf37bc048d159c000))
	OPTIONAL( QLSH(51, 0x89abcdef01234567, 51, 0x2b38000000000000, 0xfffc4d5e6f78091a))
	OPTIONAL(QLSHU(51, 0x89abcdef01234567, 51, 0x2b38000000000000, 0x00044d5e6f78091a))
	OPTIONAL( QRSH(51, 0x89abcdef01234567, 51, 0xfffffffffffff135, 0x79bde02468ace000))
	OPTIONAL(QRSHU(51, 0x89abcdef01234567, 51, 0x0000000000001135, 0x79bde02468ace000))
	OPTIONAL( QLSH(52, 0x89abcdef01234567, 52, 0x5670000000000000, 0xfff89abcdef01234))
	OPTIONAL(QLSHU(52, 0x89abcdef01234567, 52, 0x5670000000000000, 0x00089abcdef01234))
	OPTIONAL( QRSH(52, 0x89abcdef01234567, 52, 0xfffffffffffff89a, 0xbcdef01234567000))
	OPTIONAL(QRSHU(52, 0x89abcdef01234567, 52, 0x000000000000089a, 0xbcdef01234567000))
	OPTIONAL( QLSH(53, 0x89abcdef01234567, 53, 0xace0000000000000, 0xfff13579bde02468))
	OPTIONAL(QLSHU(53, 0x89abcdef01234567, 53, 0xace0000000000000, 0x00113579bde02468))
	OPTIONAL( QRSH(53, 0x89abcdef01234567, 53, 0xfffffffffffffc4d, 0x5e6f78091a2b3800))
	OPTIONAL(QRSHU(53, 0x89abcdef01234567, 53, 0x000000000000044d, 0x5e6f78091a2b3800))
	OPTIONAL( QLSH(54, 0x89abcdef01234567, 54, 0x59c0000000000000, 0xffe26af37bc048d1))
	OPTIONAL(QLSHU(54, 0x89abcdef01234567, 54, 0x59c0000000000000, 0x00226af37bc048d1))
	OPTIONAL( QRSH(54, 0x89abcdef01234567, 54, 0xfffffffffffffe26, 0xaf37bc048d159c00))
	OPTIONAL(QRSHU(54, 0x89abcdef01234567, 54, 0x0000000000000226, 0xaf37bc048d159c00))
	OPTIONAL( QLSH(55, 0x89abcdef01234567, 55, 0xb380000000000000, 0xffc4d5e6f78091a2))
	OPTIONAL(QLSHU(55, 0x89abcdef01234567, 55, 0xb380000000000000, 0x0044d5e6f78091a2))
	OPTIONAL( QRSH(55, 0x89abcdef01234567, 55, 0xffffffffffffff13, 0x579bde02468ace00))
	OPTIONAL(QRSHU(55, 0x89abcdef01234567, 55, 0x0000000000000113, 0x579bde02468ace00))
	OPTIONAL( QLSH(56, 0x89abcdef01234567, 56, 0x6700000000000000, 0xff89abcdef012345))
	OPTIONAL(QLSHU(56, 0x89abcdef01234567, 56, 0x6700000000000000, 0x0089abcdef012345))
	OPTIONAL( QRSH(56, 0x89abcdef01234567, 56, 0xffffffffffffff89, 0xabcdef0123456700))
	OPTIONAL(QRSHU(56, 0x89abcdef01234567, 56, 0x0000000000000089, 0xabcdef0123456700))
	OPTIONAL( QLSH(57, 0x89abcdef01234567, 57, 0xce00000000000000, 0xff13579bde02468a))
	OPTIONAL(QLSHU(57, 0x89abcdef01234567, 57, 0xce00000000000000, 0x0113579bde02468a))
	OPTIONAL( QRSH(57, 0x89abcdef01234567, 57, 0xffffffffffffffc4, 0xd5e6f78091a2b380))
	OPTIONAL(QRSHU(57, 0x89abcdef01234567, 57, 0x0000000000000044, 0xd5e6f78091a2b380))
	OPTIONAL( QLSH(58, 0x89abcdef01234567, 58, 0x9c00000000000000, 0xfe26af37bc048d15))
	OPTIONAL(QLSHU(58, 0x89abcdef01234567, 58, 0x9c00000000000000, 0x0226af37bc048d15))
	OPTIONAL( QRSH(58, 0x89abcdef01234567, 58, 0xffffffffffffffe2, 0x6af37bc048d159c0))
	OPTIONAL(QRSHU(58, 0x89abcdef01234567, 58, 0x0000000000000022, 0x6af37bc048d159c0))
	OPTIONAL( QLSH(59, 0x89abcdef01234567, 59, 0x3800000000000000, 0xfc4d5e6f78091a2b))
	OPTIONAL(QLSHU(59, 0x89abcdef01234567, 59, 0x3800000000000000, 0x044d5e6f78091a2b))
	OPTIONAL( QRSH(59, 0x89abcdef01234567, 59, 0xfffffffffffffff1, 0x3579bde02468ace0))
	OPTIONAL(QRSHU(59, 0x89abcdef01234567, 59, 0x0000000000000011, 0x3579bde02468ace0))
	OPTIONAL( QLSH(60, 0x89abcdef01234567, 60, 0x7000000000000000, 0xf89abcdef0123456))
	OPTIONAL(QLSHU(60, 0x89abcdef01234567, 60, 0x7000000000000000, 0x089abcdef0123456))
	OPTIONAL( QRSH(60, 0x89abcdef01234567, 60, 0xfffffffffffffff8, 0x9abcdef012345670))
	OPTIONAL(QRSHU(60, 0x89abcdef01234567, 60, 0x0000000000000008, 0x9abcdef012345670))
	OPTIONAL( QLSH(61, 0x89abcdef01234567, 61, 0xe000000000000000, 0xf13579bde02468ac))
	OPTIONAL(QLSHU(61, 0x89abcdef01234567, 61, 0xe000000000000000, 0x113579bde02468ac))
	OPTIONAL( QRSH(61, 0x89abcdef01234567, 61, 0xfffffffffffffffc, 0x4d5e6f78091a2b38))
	OPTIONAL(QRSHU(61, 0x89abcdef01234567, 61, 0x0000000000000004, 0x4d5e6f78091a2b38))
	OPTIONAL( QLSH(62, 0x89abcdef01234567, 62, 0xc000000000000000, 0xe26af37bc048d159))
	OPTIONAL(QLSHU(62, 0x89abcdef01234567, 62, 0xc000000000000000, 0x226af37bc048d159))
	OPTIONAL( QRSH(62, 0x89abcdef01234567, 62, 0xfffffffffffffffe, 0x26af37bc048d159c))
	OPTIONAL(QRSHU(62, 0x89abcdef01234567, 62, 0x0000000000000002, 0x26af37bc048d159c))
	 QLSH(63, 0x89abcdef01234567, 63, 0x8000000000000000, 0xc4d5e6f78091a2b3)
	QLSHU(63, 0x89abcdef01234567, 63, 0x8000000000000000, 0x44d5e6f78091a2b3)
	 QRSH(63, 0x89abcdef01234567, 63, 0xffffffffffffffff, 0x13579bde02468ace)
	QRSHU(63, 0x89abcdef01234567, 63, 0x0000000000000001, 0x13579bde02468ace)
	 QLSH(64, 0x89abcdef01234567, 64, 0x0000000000000000, 0x89abcdef01234567)
	QLSHU(64, 0x89abcdef01234567, 64, 0x0000000000000000, 0x89abcdef01234567)
	 QRSH(64, 0x89abcdef01234567, 64, 0xffffffffffffffff, 0x89abcdef01234567)
	QRSHU(64, 0x89abcdef01234567, 64, 0x0000000000000000, 0x89abcdef01234567)
  #endif
#endif
	prepare
		pushargi ok
	finishi @printf
	ret
	epilog
