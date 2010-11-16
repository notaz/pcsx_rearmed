/***************************************************************************
 *   Copyright (C) 2010 Gabriele Gorla                                     *
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#include "mdec.h"

#define DSIZE			8
#define DSIZE2			(DSIZE * DSIZE)

#define SCALE(x, n)		((x) >> (n))
#define SCALER(x, n)	(((x) + ((1 << (n)) >> 1)) >> (n))

#define AAN_CONST_BITS			12
#define AAN_PRESCALE_BITS		16

#define AAN_CONST_SIZE			24
#define AAN_CONST_SCALE			(AAN_CONST_SIZE - AAN_CONST_BITS)

#define AAN_PRESCALE_SIZE		20
#define AAN_PRESCALE_SCALE		(AAN_PRESCALE_SIZE-AAN_PRESCALE_BITS)
#define AAN_EXTRA				12

#define FIX_1_082392200		SCALER(18159528, AAN_CONST_SCALE) // B6
#define FIX_1_414213562		SCALER(23726566, AAN_CONST_SCALE) // A4
#define FIX_1_847759065		SCALER(31000253, AAN_CONST_SCALE) // A2
#define FIX_2_613125930		SCALER(43840978, AAN_CONST_SCALE) // B2

#define MULS(var, const)	(SCALE((var) * (const), AAN_CONST_BITS))

#define	RLE_RUN(a)	((a) >> 10)
#define	RLE_VAL(a)	(((int)(a) << (sizeof(int) * 8 - 10)) >> (sizeof(int) * 8 - 10))

#if 0
static void printmatrixu8(u8 *m) {
	int i;
	for(i = 0; i < DSIZE2; i++) {
		printf("%3d ",m[i]);
		if((i+1) % 8 == 0) printf("\n");
	}
}
#endif

static inline void fillcol(int *blk, int val) {
	blk[0 * DSIZE] = blk[1 * DSIZE] = blk[2 * DSIZE] = blk[3 * DSIZE]
		= blk[4 * DSIZE] = blk[5 * DSIZE] = blk[6 * DSIZE] = blk[7 * DSIZE] = val;
}

static inline void fillrow(int *blk, int val) {
	blk[0] = blk[1] = blk[2] = blk[3]
		= blk[4] = blk[5] = blk[6] = blk[7] = val;
}

void idct(int *block,int used_col) {
	int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	int z5, z10, z11, z12, z13;
	int *ptr;
	int i;

	// the block has only the DC coefficient
	if (used_col == -1) { 
		int v = block[0];
		for (i = 0; i < DSIZE2; i++) block[i] = v;
		return;
	}

	// last_col keeps track of the highest column with non zero coefficients
	ptr = block;
	for (i = 0; i < DSIZE; i++, ptr++) {
		if ((used_col & (1 << i)) == 0) {
			// the column is empty or has only the DC coefficient
			if (ptr[DSIZE * 0]) {
				fillcol(ptr, ptr[0]);
				used_col |= (1 << i);
			}
			continue;
		}

		// further optimization could be made by keeping track of 
		// last_row in rl2blk
		z10 = ptr[DSIZE * 0] + ptr[DSIZE * 4]; // s04
		z11 = ptr[DSIZE * 0] - ptr[DSIZE * 4]; // d04
		z13 = ptr[DSIZE * 2] + ptr[DSIZE * 6]; // s26
		z12 = MULS(ptr[DSIZE * 2] - ptr[DSIZE * 6], FIX_1_414213562) - z13; 
		//^^^^  d26=d26*2*A4-s26

		tmp0 = z10 + z13; // os07 = s04 + s26
		tmp3 = z10 - z13; // os34 = s04 - s26
		tmp1 = z11 + z12; // os16 = d04 + d26
		tmp2 = z11 - z12; // os25 = d04 - d26

		z13 = ptr[DSIZE * 3] + ptr[DSIZE * 5]; //s53
		z10 = ptr[DSIZE * 3] - ptr[DSIZE * 5]; //-d53 
		z11 = ptr[DSIZE * 1] + ptr[DSIZE * 7]; //s17
		z12 = ptr[DSIZE * 1] - ptr[DSIZE * 7]; //d17

		tmp7 = z11 + z13; // od07 = s17 + s53

		z5 = (z12 - z10) * (FIX_1_847759065); 
		tmp6 = SCALE(z10*(FIX_2_613125930) + z5, AAN_CONST_BITS) - tmp7; 
		tmp5 = MULS(z11 - z13, FIX_1_414213562) - tmp6;
		tmp4 = SCALE(z12*(FIX_1_082392200) - z5, AAN_CONST_BITS) + tmp5; 

		// path #1
		//z5 = (z12 - z10)* FIX_1_847759065; 
		// tmp0 = (d17 + d53) * 2*A2

		//tmp6 = DESCALE(z10*FIX_2_613125930 + z5, CONST_BITS) - tmp7; 
		// od16 = (d53*-2*B2 + tmp0) - od07

		//tmp4 = DESCALE(z12*FIX_1_082392200 - z5, CONST_BITS) + tmp5; 
		// od34 = (d17*2*B6 - tmp0) + od25

		// path #2

		// od34 = d17*2*(B6-A2) - d53*2*A2
		// od16 = d53*2*(A2-B2) + d17*2*A2

		// end

		//    tmp5 = MULS(z11 - z13, FIX_1_414213562) - tmp6;
		// od25 = (s17 - s53)*2*A4 - od16

		ptr[DSIZE * 0] = (tmp0 + tmp7); // os07 + od07
		ptr[DSIZE * 7] = (tmp0 - tmp7); // os07 - od07
		ptr[DSIZE * 1] = (tmp1 + tmp6); // os16 + od16
		ptr[DSIZE * 6] = (tmp1 - tmp6); // os16 - od16
		ptr[DSIZE * 2] = (tmp2 + tmp5); // os25 + od25
		ptr[DSIZE * 5] = (tmp2 - tmp5); // os25 - od25
		ptr[DSIZE * 4] = (tmp3 + tmp4); // os34 + od34
		ptr[DSIZE * 3] = (tmp3 - tmp4); // os34 - od34
	}

	ptr = block;
	if (used_col == 1) {
		for (i = 0; i < DSIZE; i++)
			fillrow(block + DSIZE * i, block[DSIZE * i]);    
	} else {
		for (i = 0; i < DSIZE; i++, ptr += DSIZE) {
			z10 = ptr[0] + ptr[4];
			z11 = ptr[0] - ptr[4];
			z13 = ptr[2] + ptr[6];
			z12 = MULS(ptr[2] - ptr[6], FIX_1_414213562) - z13;

			tmp0 = z10 + z13;
			tmp3 = z10 - z13;
			tmp1 = z11 + z12;
			tmp2 = z11 - z12;
			
			z13 = ptr[3] + ptr[5];
			z10 = ptr[3] - ptr[5];
			z11 = ptr[1] + ptr[7];
			z12 = ptr[1] - ptr[7];

			tmp7 = z11 + z13;
			z5 = (z12 - z10) * FIX_1_847759065; 
			tmp6 = SCALE(z10 * FIX_2_613125930 + z5, AAN_CONST_BITS) - tmp7;
			tmp5 = MULS(z11 - z13, FIX_1_414213562) - tmp6;
			tmp4 = SCALE(z12 * FIX_1_082392200 - z5, AAN_CONST_BITS) + tmp5;

			ptr[0] = tmp0 + tmp7;

			ptr[7] = tmp0 - tmp7;
			ptr[1] = tmp1 + tmp6;
			ptr[6] = tmp1 - tmp6;
			ptr[2] = tmp2 + tmp5;
			ptr[5] = tmp2 - tmp5;
			ptr[4] = tmp3 + tmp4;
			ptr[3] = tmp3 - tmp4;
		}
	}
}

// mdec0: command register
#define MDEC0_STP			0x02000000
#define MDEC0_RGB24			0x08000000
#define MDEC0_SIZE_MASK		0xFFFF

// mdec1: status register
#define MDEC1_BUSY			0x20000000
#define MDEC1_DREQ			0x18000000
#define MDEC1_FIFO			0xc0000000
#define MDEC1_RGB24			0x02000000
#define MDEC1_STP			0x00800000
#define MDEC1_RESET			0x80000000

struct {
    u32 reg0;
    u32 reg1;
    unsigned short *rl;
    int rlsize;
} mdec;

static int iq_y[DSIZE2], iq_uv[DSIZE2];

static int zscan[DSIZE2] = {
	0 , 1 , 8 , 16, 9 , 2 , 3 , 10,
	17, 24, 32, 25, 18, 11, 4 , 5 ,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6 , 7 , 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static int aanscales[DSIZE2] = {
	1048576, 1454417, 1370031, 1232995, 1048576,  823861, 567485, 289301,
	1454417, 2017334, 1900287, 1710213, 1454417, 1142728, 787125, 401273,
	1370031, 1900287, 1790031, 1610986, 1370031, 1076426, 741455, 377991,
	1232995, 1710213, 1610986, 1449849, 1232995,  968758, 667292, 340183,
	1048576, 1454417, 1370031, 1232995, 1048576,  823861, 567485, 289301,
	823861,  1142728, 1076426, 968758,  823861,  647303, 445870, 227303,
	567485,  787125,  741455,  667292,  567485,  445870, 307121, 156569,
	289301,  401273,  377991,  340183,  289301,  227303, 156569,  79818
};

static void iqtab_init(int *iqtab, unsigned char *iq_y) {
	int i;

	for (i = 0; i < DSIZE2; i++) {
		iqtab[i] = (iq_y[i] * SCALER(aanscales[zscan[i]], AAN_PRESCALE_SCALE));
	}
}

#define	MDEC_END_OF_DATA	0xfe00

unsigned short *rl2blk(int *blk, unsigned short *mdec_rl) {
	int i, k, q_scale, rl, used_col;
 	int *iqtab;

	memset(blk, 0, 6 * DSIZE2 * sizeof(int));
	iqtab = iq_uv;
	for (i = 0; i < 6; i++) {
		// decode blocks (Cr,Cb,Y1,Y2,Y3,Y4)
		if (i == 2) iqtab = iq_y;

		rl = SWAP16(*mdec_rl); mdec_rl++;
		q_scale = RLE_RUN(rl);
		blk[0] = SCALER(iqtab[0] * RLE_VAL(rl), AAN_EXTRA - 3);
		for (k = 0, used_col = 0;;) {
			rl = SWAP16(*mdec_rl); mdec_rl++;
			if (rl == MDEC_END_OF_DATA) break;
			k += RLE_RUN(rl) + 1;	// skip zero-coefficients

			if (k > 63) {
				// printf("run lenght exceeded 64 enties\n");
				break;
			}

			// zigzag transformation
			blk[zscan[k]] = SCALER(RLE_VAL(rl) * iqtab[k] * q_scale, AAN_EXTRA);
			// keep track of used columns to speed up the idtc
			used_col |= (zscan[k] > 7) ? 1 << (zscan[k] & 7) : 0;
		}

		if (k == 0) used_col = -1;
		// used_col is -1 for blocks with only the DC coefficient
		// any other value is a bitmask of the columns that have 
		// at least one non zero cofficient in the rows 1-7
		// single coefficients in row 0 are treted specially 
		// in the idtc function
		idct(blk, used_col);
		blk += DSIZE2;
	}
	return mdec_rl;
}

// full scale (JPEG)
// Y/Cb/Cr[0...255] -> R/G/B[0...255]
// R = 1.000 * (Y) + 1.400 * (Cr - 128)
// G = 1.000 * (Y) - 0.343 * (Cb - 128) - 0.711 (Cr - 128)
// B = 1.000 * (Y) + 1.765 * (Cb - 128)
#define	MULR(a)			((1434 * (a))) 
#define	MULB(a)			((1807 * (a))) 
#define	MULG2(a, b)		((-351 * (a) - 728 * (b)))
#define MULY(a)			((a) << 10)

#define	MAKERGB15(r, g, b, a)	(SWAP16(a | ((b) << 10) | ((g) << 5) | (r)))
#define	SCALE8(c)				SCALER(c, 20) 
#define SCALE5(c)				SCALER(c, 23)

#define CLAMP5(c)	( ((c) < -16) ? 0 : (((c) > (31 - 16)) ? 31 : ((c) + 16)) )
#define CLAMP8(c)	( ((c) < -128) ? 0 : (((c) > (255 - 128)) ? 255 : ((c) + 128)) )

#define CLAMP_SCALE8(a)   (CLAMP8(SCALE8(a)))
#define CLAMP_SCALE5(a)   (CLAMP5(SCALE5(a)))

static inline void putlinebw15(unsigned short *image, int *Yblk) {
	int i;
	int A = (mdec.reg0 & MDEC0_STP) ? 0x8000 : 0;

	for (i = 0; i < 8; i++, Yblk++) {
		int Y = *Yblk;
		// missing rounding
		image[i] = SWAP16((CLAMP5(Y >> 3) * 0x421) | A);
	}
}

static void putquadrgb15(unsigned short *image, int *Yblk, int Cr, int Cb) {
	int Y, R, G, B;
	int A = (mdec.reg0 & MDEC0_STP) ? 0x8000 : 0;
	R = MULR(Cr);
	G = MULG2(Cb, Cr);
	B = MULB(Cb);

	// added transparency
	Y = MULY(Yblk[0]);
	image[0] = MAKERGB15(CLAMP_SCALE5(Y + R), CLAMP_SCALE5(Y + G), CLAMP_SCALE5(Y + B), A);
	Y = MULY(Yblk[1]);
	image[1] = MAKERGB15(CLAMP_SCALE5(Y + R), CLAMP_SCALE5(Y + G), CLAMP_SCALE5(Y + B), A);
	Y = MULY(Yblk[8]);
	image[16] = MAKERGB15(CLAMP_SCALE5(Y + R), CLAMP_SCALE5(Y + G), CLAMP_SCALE5(Y + B), A);
	Y = MULY(Yblk[9]);
	image[17] = MAKERGB15(CLAMP_SCALE5(Y + R), CLAMP_SCALE5(Y + G), CLAMP_SCALE5(Y + B), A);
}

static void yuv2rgb15(int *blk, unsigned short *image) {
	int x, y;
	int *Yblk = blk + DSIZE2 * 2;
	int *Crblk = blk;
	int *Cbblk = blk + DSIZE2;

	if (!Config.Mdec) {
		for (y = 0; y < 16; y += 2, Crblk += 4, Cbblk += 4, Yblk += 8, image += 24) {
			if (y == 8) Yblk += DSIZE2;
			for (x = 0; x < 4; x++, image += 2, Crblk++, Cbblk++, Yblk += 2) {
				putquadrgb15(image, Yblk, *Crblk, *Cbblk);
				putquadrgb15(image + 8, Yblk + DSIZE2, *(Crblk + 4), *(Cbblk + 4));
			}
		} 
	} else {
		for (y = 0; y < 16; y++, Yblk += 8, image += 16) {
			if (y == 8) Yblk += DSIZE2;
			putlinebw15(image, Yblk);
			putlinebw15(image + 8, Yblk + DSIZE2);
		}
	}
}

static inline void putlinebw24(unsigned char *image, int *Yblk) {
	int i;
	unsigned char Y;
	for (i = 0; i < 8 * 3; i += 3, Yblk++) {
		Y = CLAMP8(*Yblk);
		image[i + 0] = Y;
		image[i + 1] = Y;
		image[i + 2] = Y;
	}
}

static void putquadrgb24(unsigned char *image, int *Yblk, int Cr, int Cb) {
	int Y, R, G, B;

	R = MULR(Cr);
	G = MULG2(Cb,Cr);
	B = MULB(Cb);

	Y = MULY(Yblk[0]);
	image[0 * 3 + 0] = CLAMP_SCALE8(Y + R);
	image[0 * 3 + 1] = CLAMP_SCALE8(Y + G);
	image[0 * 3 + 2] = CLAMP_SCALE8(Y + B);
	Y = MULY(Yblk[1]);
	image[1 * 3 + 0] = CLAMP_SCALE8(Y + R);
	image[1 * 3 + 1] = CLAMP_SCALE8(Y + G);
	image[1 * 3 + 2] = CLAMP_SCALE8(Y + B);
	Y = MULY(Yblk[8]);
	image[16 * 3 + 0] = CLAMP_SCALE8(Y + R);
	image[16 * 3 + 1] = CLAMP_SCALE8(Y + G);
	image[16 * 3 + 2] = CLAMP_SCALE8(Y + B);
	Y = MULY(Yblk[9]);
	image[17 * 3 + 0] = CLAMP_SCALE8(Y + R);
	image[17 * 3 + 1] = CLAMP_SCALE8(Y + G);
	image[17 * 3 + 2] = CLAMP_SCALE8(Y + B);
}

static void yuv2rgb24(int *blk, unsigned char *image) {
	int x, y;
	int *Yblk = blk + DSIZE2 * 2;
	int *Crblk = blk;
	int *Cbblk = blk + DSIZE2;

	if (!Config.Mdec) {
		for (y = 0; y < 16; y += 2, Crblk += 4, Cbblk += 4, Yblk += 8, image += 24 * 3) {
			if (y == 8) Yblk += DSIZE2;
			for (x = 0; x < 4; x++, image += 6, Crblk++, Cbblk++, Yblk += 2) {
				putquadrgb24(image, Yblk, *Crblk, *Cbblk);
				putquadrgb24(image + 8 * 3, Yblk + DSIZE2, *(Crblk + 4), *(Cbblk + 4));
			}
		}
	} else {
		for (y = 0; y < 16; y++, Yblk += 8, image += 16 * 3) {
			if (y == 8) Yblk += DSIZE2;
			putlinebw24(image, Yblk);
			putlinebw24(image + 8 * 3, Yblk + DSIZE2);
		}
	}
}

void mdecInit(void) {
	mdec.rl = (u16 *)&psxM[0x100000];
	mdec.reg0 = 0;
	mdec.reg1 = 0;
}

// command register
void mdecWrite0(u32 data) {
#ifdef CDR_LOG
	CDR_LOG("mdec0 write %08x\n", data);
#endif
	mdec.reg0 = data;
}

u32 mdecRead0(void) {
#ifdef CDR_LOG
	CDR_LOG("mdec0 read %08x\n", mdec.reg0);
#endif
	// mame is returning 0
	return mdec.reg0;
}

// status register
void mdecWrite1(u32 data) {
#ifdef CDR_LOG
	CDR_LOG("mdec1 write %08x\n", data);
#endif
	if (data & MDEC1_RESET) { // mdec reset
		mdec.reg0 = 0;
		mdec.reg1 = 0;
	}
}

u32 mdecRead1(void) {
	u32 v = mdec.reg1;
	v |= (mdec.reg0 & MDEC0_STP) ? MDEC1_STP : 0;
	v |= (mdec.reg0 & MDEC0_RGB24) ? MDEC1_RGB24 : 0;
#ifdef CDR_LOG
	CDR_LOG("mdec1 read %08x\n", v);
#endif
	return v;
}

void psxDma0(u32 adr, u32 bcr, u32 chcr) {
	int cmd = mdec.reg0;
	int size;
	
#ifdef CDR_LOG
	CDR_LOG("DMA0 %08x %08x %08x\n", adr, bcr, chcr);
#endif

	if (chcr != 0x01000201) {
		// printf("chcr != 0x01000201\n");
		return;
	}

	size = (bcr >> 16) * (bcr & 0xffff);

	switch (cmd >> 28) {
		case 0x3: // decode
			mdec.rl = (u16 *)PSXM(adr);
			mdec.rlsize = mdec.reg0 & MDEC0_SIZE_MASK;
			break;

		case 0x4: // quantization table upload
			{
				u8 *p = (u8 *)PSXM(adr);
				// printf("uploading new quantization table\n");
				// printmatrixu8(p);
				// printmatrixu8(p + 64);
				iqtab_init(iq_y, p);
				iqtab_init(iq_uv, p + 64);
			}
			break;

		case 0x6: // cosine table
			// printf("mdec cosine table\n");
			break;

		default:
			// printf("mdec unknown command\n");
			break;
	}

	HW_DMA0_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(0);
}

void psxDma1(u32 adr, u32 bcr, u32 chcr) {
	int blk[DSIZE2 * 6];
	unsigned short *image;
	int size;

#ifdef CDR_LOG
	CDR_LOG("DMA1 %08x %08x %08x (cmd = %08x)\n", adr, bcr, chcr, mdec.reg0);
#endif

	if (chcr != 0x01000200) return;

	size = (bcr >> 16) * (bcr & 0xffff);

	image = (u16 *)PSXM(adr);

	if (mdec.reg0 & MDEC0_RGB24) { // 15-b decoding
		// MDECOUTDMA_INT(((size * (1000000 / 9000)) / 4) /** 4*/);
		MDECOUTDMA_INT(size / 4);
		size = size / ((16 * 16) / 2);
		for (; size > 0; size--, image += (16 * 16)) {
			mdec.rl = rl2blk(blk, mdec.rl);
			yuv2rgb15(blk, image);
		}
	} else { // 24-b decoding
		// MDECOUTDMA_INT(((size * (1000000 / 9000)) / 4) /** 4*/);
		MDECOUTDMA_INT(size / 4);
		size = size / ((24 * 16) / 2);
		for (; size > 0; size--, image += (24 * 16)) {
			mdec.rl = rl2blk(blk, mdec.rl);
			yuv2rgb24(blk, (u8 *)image);
		}
	}

	mdec.reg1 |= MDEC1_BUSY;
}

void mdec1Interrupt() {
#ifdef CDR_LOG
	CDR_LOG("mdec1Interrupt\n");
#endif
	if (HW_DMA1_CHCR & SWAP32(0x01000000)) {
		// Set a fixed value totaly arbitrarie another sound value is
		// PSXCLK / 60 or PSXCLK / 50 since the bug happened at end of frame.
		// PSXCLK / 1000 seems good for FF9. (for FF9 need < ~28000)
		// CAUTION: commented interrupt-handling may lead to problems, keep an eye ;-)
		MDECOUTDMA_INT(PSXCLK / 1000 * BIAS);
//		psxRegs.interrupt |= 0x02000000;
//		psxRegs.intCycle[5 + 24 + 1] *= 8;
//		psxRegs.intCycle[5 + 24] = psxRegs.cycle;
		HW_DMA1_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(1);
	} else {
		mdec.reg1 &= ~MDEC1_BUSY;
	}
}

int mdecFreeze(gzFile f, int Mode) {
	gzfreeze(&mdec, sizeof(mdec));
	gzfreeze(iq_y, sizeof(iq_y));
	gzfreeze(iq_uv, sizeof(iq_uv));

	return 0;
}
