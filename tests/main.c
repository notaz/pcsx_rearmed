#include "asm.h"

#define u8      unsigned char
#define u16     unsigned short
#define u32     unsigned int
#define s16     signed short

#define HW8(v,  ofs) *((volatile u8  *)&v[ofs])
#define HW32(v, ofs) *((volatile u32 *)&v[ofs])

int main()
{
	u8 *hw = (u8 *)0x1f800000;
	register u32 ra asm("ra");

	printf("started, ra=%x\n", ra);
	printf("irq stat/mask %08x/%08x\n", HW32(hw, 0x1070), HW32(hw, 0x1074));

	return 0;
}
