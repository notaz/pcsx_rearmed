#include "misc.h"
#include "sio.h"
#include "ppf.h"
#include "new_dynarec/new_dynarec.h"
#include "lightrec/plugin.h"

/* It's duplicated from emu_if.c */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/* Corresponds to LIGHTREC_OPT_INV_DMA_ONLY of lightrec.h */
#define LIGHTREC_HACK_INV_DMA_ONLY (1 << 0)

u32 lightrec_hacks;

static const char * const MemorycardHack_db[] =
{
	/* Lifeforce Tenka, also known as Codename Tenka */
	"SLES00613", "SLED00690", "SLES00614", "SLES00615",
	"SLES00616", "SLES00617", "SCUS94409"
};

static const char * const cdr_read_hack_db[] =
{
	/* T'ai Fu - Wrath of the Tiger */
	"SLUS00787",
};

static const char * const gpu_slow_llist_db[] =
{
	/* Bomberman Fantasy Race */
	"SLES01712", "SLPS01525", "SLPS91138", "SLPM87102", "SLUS00823",
	/* Crash Bash */
	"SCES02834", "SCUS94570", "SCUS94616", "SCUS94654",
	/* Final Fantasy IV */
	"SCES03840", "SLPM86028", "SLUS01360",
	/* Point Blank - calibration cursor */
	"SCED00287", "SCES00886", "SLUS00481",
	/* Simple 1500 Series Vol. 57: The Meiro */
	"SLPM86715",
	/* Spot Goes to Hollywood */
	"SLES00330", "SLPS00394", "SLUS00014",
	/* Tiny Tank */
	"SCES01338", "SCES02072", "SCES02072", "SCES02072", "SCES02072", "SCUS94427",
	/* Vampire Hunter D */
	"SLES02731", "SLPS02477", "SLPS03198", "SLUS01138",
};

static const char * const gpu_centering_hack_db[] =
{
	/* Gradius Gaiden */
	"SLPM86042", "SLPM86103", "SLPM87323",
	/* Sexy Parodius */
	"SLPM86009",
};

static const char * const dualshock_timing1024_hack_db[] =
{
	/* Judge Dredd - could also be poor cdrom+mdec+dma timing */
	"SLUS00630", "SLES00755",
};

static const char * const dualshock_init_analog_hack_db[] =
{
	/* Formula 1 Championship Edition */
	"SLUS00546",
};

#define HACK_ENTRY(var, list) \
	{ #var, &Config.hacks.var, list, ARRAY_SIZE(list) }

static const struct
{
	const char *name;
	boolean *var;
	const char * const * id_list;
	size_t id_list_len;
}
hack_db[] =
{
	HACK_ENTRY(cdr_read_timing, cdr_read_hack_db),
	HACK_ENTRY(gpu_slow_list_walking, gpu_slow_llist_db),
	HACK_ENTRY(gpu_centering, gpu_centering_hack_db),
	HACK_ENTRY(gpu_timing1024, dualshock_timing1024_hack_db),
	HACK_ENTRY(dualshock_init_analog, dualshock_init_analog_hack_db),
};

static const struct
{
	int mult;
	const char * const id[4];
}
cycle_multiplier_overrides[] =
{
	/* note: values are = (10000 / gui_option) */
	/* Internal Section - fussy about timings */
	{ 202, { "SLPS01868" } },
	/* Super Robot Taisen Alpha - on the edge with 175,
	 * changing memcard settings is enough to break/unbreak it */
	{ 190, { "SLPS02528", "SLPS02636" } },
	/* Brave Fencer Musashi - cd sectors arrive too fast */
	{ 170, { "SLUS00726", "SLPS01490" } },
#if defined(DRC_DISABLE) || defined(LIGHTREC) /* new_dynarec has a hack for this game */
	/* Parasite Eve II - internal timer checks */
	{ 125, { "SLUS01042", "SLUS01055", "SLES02558", "SLES12558" } },
#endif
	/* Discworld Noir - audio skips if CPU runs too fast */
	{ 222, { "SLES01549", "SLES02063", "SLES02064" } },
	/* Digimon World */
	{ 153, { "SLUS01032", "SLES02914" } },
	/* Syphon Filter - reportedly hangs under unknown conditions */
	{ 169, { "SCUS94240" } },
	/* Psychic Detective - some weird race condition in the game's cdrom code */
	{ 222, { "SLUS00165", "SLUS00166", "SLUS00167" } },
	{ 222, { "SLES00070", "SLES10070", "SLES20070" } },
	/* Vib-Ribbon - cd timing issues (PAL+ari64drc only?) */
	{ 200, { "SCES02873" } },
	/* Zero Divide - sometimes too fast */
	{ 200, { "SLUS00183", "SLES00159", "SLPS00083", "SLPM80008" } },
};

static const struct
{
	const char * const id;
	u32 hacks;
}
lightrec_hacks_db[] =
{
	/* Formula One Arcade */
	{ "SCES03886", LIGHTREC_HACK_INV_DMA_ONLY },

	/* Formula One '99 */
	{ "SLUS00870", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCPS10101", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES01979", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SLES01979", LIGHTREC_HACK_INV_DMA_ONLY },

	/* Formula One 2000 */
	{ "SLUS01134", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES02777", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES02778", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES02779", LIGHTREC_HACK_INV_DMA_ONLY },

	/* Formula One 2001 */
	{ "SCES03404", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES03423", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES03424", LIGHTREC_HACK_INV_DMA_ONLY },
	{ "SCES03524", LIGHTREC_HACK_INV_DMA_ONLY },
};

/* Function for automatic patching according to GameID. */
void Apply_Hacks_Cdrom(void)
{
	size_t i, j;

	memset(&Config.hacks, 0, sizeof(Config.hacks));

	for (i = 0; i < ARRAY_SIZE(hack_db); i++)
	{
		for (j = 0; j < hack_db[i].id_list_len; j++)
		{
			if (strncmp(CdromId, hack_db[i].id_list[j], 9))
				continue;
			*hack_db[i].var = 1;
			SysPrintf("using hack: %s\n", hack_db[i].name);
			break;
		}
	}

	if (Config.hacks.dualshock_init_analog) {
		// assume the default is off, see LoadPAD1plugin()
		for (i = 0; i < 8; i++)
			padToggleAnalog(i);
	}

	/* Apply Memory card hack for Codename Tenka. (The game needs one of the memory card slots to be empty) */
	for (i = 0; i < ARRAY_SIZE(MemorycardHack_db); i++)
	{
		if (strncmp(CdromId, MemorycardHack_db[i], 9) == 0)
		{
			/* Disable the second memory card slot for the game */
			Config.Mcd2[0] = 0;
			/* This also needs to be done because in sio.c, they don't use Config.Mcd2 for that purpose */
			McdDisable[1] = 1;
			break;
		}
	}

	/* Dynarec game-specific hacks */
	new_dynarec_hacks_pergame = 0;
	Config.cycle_multiplier_override = 0;

	for (i = 0; i < ARRAY_SIZE(cycle_multiplier_overrides); i++)
	{
		const char * const * const ids = cycle_multiplier_overrides[i].id;
		for (j = 0; j < ARRAY_SIZE(cycle_multiplier_overrides[i].id); j++)
			if (ids[j] && strcmp(ids[j], CdromId) == 0)
				break;
		if (j < ARRAY_SIZE(cycle_multiplier_overrides[i].id))
		{
			Config.cycle_multiplier_override = cycle_multiplier_overrides[i].mult;
			new_dynarec_hacks_pergame |= NDHACK_OVERRIDE_CYCLE_M;
			SysPrintf("using cycle_multiplier_override: %d\n",
				Config.cycle_multiplier_override);
			break;
		}
	}

	lightrec_hacks = 0;

	for (i = 0; drc_is_lightrec() && i < ARRAY_SIZE(lightrec_hacks_db); i++) {
		if (strcmp(CdromId, lightrec_hacks_db[i].id) == 0)
		{
			lightrec_hacks = lightrec_hacks_db[i].hacks;
			SysPrintf("using lightrec_hacks: 0x%x\n", lightrec_hacks);
			break;
		}
	}
}

// from duckstation's gamedb.json
static const u16 libcrypt_ids[] = {
	   17,   311,   995,  1041,  1226,  1241,  1301,  1362,  1431,  1444,
	 1492,  1493,  1494,  1495,  1516,  1517,  1518,  1519,  1545,  1564,
	 1695,  1700,  1701,  1702,  1703,  1704,  1715,  1733,  1763,  1882,
	 1906,  1907,  1909,  1943,  1979,  2004,  2005,  2006,  2007,  2024,
	 2025,  2026,  2027,  2028,  2029,  2030,  2031,  2061,  2071,  2080,
	 2081,  2082,  2083,  2084,  2086,  2104,  2105,  2112,  2113,  2118,
	 2181,  2182,  2184,  2185,  2207,  2208,  2209,  2210,  2211,  2222,
	 2264,  2290,  2292,  2293,  2328,  2329,  2330,  2354,  2355,  2365,
	 2366,  2367,  2368,  2369,  2395,  2396,  2402,  2430,  2431,  2432,
	 2433,  2487,  2488,  2489,  2490,  2491,  2529,  2530,  2531,  2532,
	 2533,  2538,  2544,  2545,  2546,  2558,  2559,  2560,  2561,  2562,
	 2563,  2572,  2573,  2681,  2688,  2689,  2698,  2700,  2704,  2705,
	 2706,  2707,  2708,  2722,  2723,  2724,  2733,  2754,  2755,  2756,
	 2763,  2766,  2767,  2768,  2769,  2824,  2830,  2831,  2834,  2835,
	 2839,  2857,  2858,  2859,  2860,  2861,  2862,  2965,  2966,  2967,
	 2968,  2969,  2975,  2976,  2977,  2978,  2979,  3061,  3062,  3189,
	 3190,  3191,  3241,  3242,  3243,  3244,  3245,  3324,  3489,  3519,
	 3520,  3521,  3522,  3523,  3530,  3603,  3604,  3605,  3606,  3607,
	 3626,  3648, 12080, 12081, 12082, 12083, 12084, 12328, 12329, 12330,
	12558, 12559, 12560, 12561, 12562, 12965, 12966, 12967, 12968, 12969,
	22080, 22081, 22082, 22083, 22084, 22328, 22329, 22330, 22965, 22966,
	22967, 22968, 22969, 32080, 32081, 32082, 32083, 32084, 32965, 32966,
	32967, 32968, 32969
};

// as documented by nocash
static const u16 libcrypt_sectors[16] = {
	14105, 14231, 14485, 14579, 14649, 14899, 15056, 15130,
	15242, 15312, 15378, 15628, 15919, 16031, 16101, 16167
};

int check_unsatisfied_libcrypt(void)
{
	const char *p = CdromId + 4;
	u16 id, key = 0;
	size_t i;

	if (strncmp(CdromId, "SCE", 3) && strncmp(CdromId, "SLE", 3))
		return 0;
	while (*p == '0')
		p++;
	id = (u16)atoi(p);
	for (i = 0; i < ARRAY_SIZE(libcrypt_ids); i++)
		if (id == libcrypt_ids[i])
			break;
	if (i == ARRAY_SIZE(libcrypt_ids))
		return 0;

	// detected a protected game
	if (!CDR_getBufferSub(libcrypt_sectors[0]) && !sbi_sectors) {
		SysPrintf("==================================================\n");
		SysPrintf("LibCrypt game detected with missing SBI/subchannel\n");
		SysPrintf("==================================================\n");
		return 1;
	}

	if (sbi_sectors) {
		// calculate key just for fun (we don't really need it)
		for (i = 0; i < 16; i++)
			if (CheckSBI(libcrypt_sectors[i] - 2*75))
				key |= 1u << (15 - i);
	}
	if (key)
		SysPrintf("%s, possible key=%04X\n", "LibCrypt detected", key);
	else
		SysPrintf("%s\n", "LibCrypt detected");
	return 0;
}
