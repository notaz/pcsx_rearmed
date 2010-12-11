
enum pcounters {
	PCNT_ALL,
	PCNT_GPU,
	PCNT_SPU,
	PCNT_BLIT,
	PCNT_TEST,
	PCNT_CNT
};

#ifdef PCNT

static const char *pcnt_names[PCNT_CNT] = { "", "gpu", "spu", "blit", "test" };

#define PCNT_FRAMES 10

extern unsigned int pcounters[PCNT_CNT];
extern unsigned int pcounter_starts[PCNT_CNT];

#define pcnt_start(id) \
	pcounter_starts[id] = pcnt_get()

#define pcnt_end(id) \
	pcounters[id] += pcnt_get() - pcounter_starts[id]

void pcnt_hook_plugins(void);

static inline void pcnt_print(float fps)
{
	static int print_counter;
	unsigned int total, rem;
	int i;

	for (i = 0; i < PCNT_CNT; i++)
		pcounters[i] /= 1000 * PCNT_FRAMES;

	rem = total = pcounters[PCNT_ALL];
	for (i = 1; i < PCNT_CNT; i++)
		rem -= pcounters[i];
	if (!total)
		total++;

	if (--print_counter < 0) {
		printf("     ");
		for (i = 1; i < PCNT_CNT; i++)
			printf("%5s ", pcnt_names[i]);
		printf("%5s\n", "rem");
		print_counter = 30;
	}

	printf("%4.1f ", fps);
	for (i = 1; i < PCNT_CNT; i++)
		printf("%5u ", pcounters[i]);
	printf("%5u (", rem);
	for (i = 1; i < PCNT_CNT; i++)
		printf("%2u ", pcounters[i] * 100 / total);
	printf("%2u) %u\n", rem * 100 / total, total);

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
#define pcnt_print(fps)

#endif
