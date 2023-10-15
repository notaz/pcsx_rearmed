#include "misc.h"
#include "sio.h"
#include "new_dynarec/new_dynarec.h"

/* It's duplicated from emu_if.c */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

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
	/* Spot Goes to Hollywood */
	"SLES00330", "SLPS00394", "SLUS00014",
	/* Vampire Hunter D */
	"SLES02731", "SLPS02477", "SLPS03198", "SLUS01138",
};

static const char * const gpu_busy_hack_db[] =
{
	/* ToHeart (Japan) */
	"SLPS01919", "SLPS01920",
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
	HACK_ENTRY(gpu_busy_hack, gpu_busy_hack_db),
};

static const struct
{
	const char * const id;
	int mult;
}
cycle_multiplier_overrides[] =
{
	/* note: values are = (10000 / gui_option) */
	/* Internal Section - fussy about timings */
	{ "SLPS01868", 202 },
	/* Super Robot Taisen Alpha - on the edge with 175,
	 * changing memcard settings is enough to break/unbreak it */
	{ "SLPS02528", 190 },
	{ "SLPS02636", 190 },
	/* Brave Fencer Musashi - cd sectors arrive too fast */
	{ "SLUS00726", 170 },
	{ "SLPS01490", 170 },
#if defined(DRC_DISABLE) || defined(LIGHTREC) /* new_dynarec has a hack for this game */
	/* Parasite Eve II - internal timer checks */
	{ "SLUS01042", 125 },
	{ "SLUS01055", 125 },
	{ "SLES02558", 125 },
	{ "SLES12558", 125 },
#endif
	/* Discworld Noir - audio skips if CPU runs too fast */
	{ "SLES01549", 222 },
	{ "SLES02063", 222 },
	{ "SLES02064", 222 },
	/* Judge Dredd - could also be poor MDEC timing */
	{ "SLUS00630", 128 },
	{ "SLES00755", 128 },
	/* Digimon World */
	{ "SLUS01032", 153 },
	{ "SLES02914", 153 },
};

/* Function for automatic patching according to GameID. */
void Apply_Hacks_Cdrom()
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
		if (strcmp(CdromId, cycle_multiplier_overrides[i].id) == 0)
		{
			Config.cycle_multiplier_override = cycle_multiplier_overrides[i].mult;
			new_dynarec_hacks_pergame |= NDHACK_OVERRIDE_CYCLE_M;
			SysPrintf("using cycle_multiplier_override: %d\n",
				Config.cycle_multiplier_override);
			break;
		}
	}
}
