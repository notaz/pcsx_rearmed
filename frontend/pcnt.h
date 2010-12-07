
enum pcounters {
	PCNT_ALL,
	PCNT_GPU,
	PCNT_SPU,
	PCNT_CNT
};

#ifdef PCNT

extern unsigned int pcounters[PCNT_CNT];
extern unsigned int pcounter_starts[PCNT_CNT];

#define pcnt_start(id) \
	pcounter_starts[id] = pcnt_get()

#define pcnt_end(id) \
	pcounters[id] += pcnt_get() - pcounter_starts[id]

void pcnt_hook_plugins(void);

static inline void pcnt_print(float fps)
{
	unsigned int total, gpu, spu, rem;
	int i;

	for (i = 0; i < PCNT_CNT; i++)
		pcounters[i] >>= 10;

	total = pcounters[PCNT_ALL];
	gpu = pcounters[PCNT_GPU];
	spu = pcounters[PCNT_SPU];
	rem = total - gpu - spu;
	if (!total)
		total++;

	printf("%2.1f %6u %6u %6u (%2d %2d %2d)\n", fps, gpu, spu, rem,
		gpu * 100 / total, spu * 100 / total, rem * 100 / total);

	memset(pcounters, 0, sizeof(pcounters));
}

static inline unsigned int pcnt_get(void)
{
	unsigned int val;
#ifdef __ARM_ARCH_7A__
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 0"
			 : "=r"(val));
#else
	val = 0;
#endif
	return val;
}

#else

#define pcnt_start(id)
#define pcnt_end(id)
#define pcnt_hook_plugins()

static inline void pcnt_print(float fps)
{
	printf("%2.1f\n", fps);
}

#endif
