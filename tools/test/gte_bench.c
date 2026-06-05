#include <stdio.h>

int atest(int *sp);

int main()
{
        int v, sp, i, first, best = 99999, worst = 0;
        asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(v));
        v |= 5; // master enable, ccnt reset
        v &= ~8; // ccnt divider 0
        asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(v));
        // enable cycle counter
        asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(1<<31));

	first = atest(&sp);
	for (i = 0; i < 13; i++) {
		v = atest(&sp);
		if (best > v)
			best = v;
		if (worst < v)
			worst = v;
	}
	printf("%d-%d-%d sp=%x\n", best, worst, first, sp);
	return 0;
}
