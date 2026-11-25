/*
 * A minimal libretro frontend for the link cable tests: loads the core,
 * starts a PS-X EXE with the HLE BIOS, runs frames without video or sound
 * and prints the result block the test program leaves in RAM.
 *
 *   retro_host <core.so> <program.exe> <role> <link> <timing> <drc> [frames]
 *     role:   1, 2 or 3, written to 0x80100200 for the program
 *     link:   "disabled", "console A", "console B" or "loopback"
 *     timing: "exact" or "fast"
 *     drc:    "enabled" or "disabled" (the interpreter)
 * PCSX_SIO1_LINK_NAME selects the link, HOST_JITTER=<seed> makes the
 * process sleep at random between frames.
 * HOST_STATE_AT=<frame> does a savestate round trip after that frame,
 * HOST_STATE_SAVE=<file> writes that state out as well and
 * HOST_STATE_LOAD=<file> loads one instead of making it.
 * HOST_RAM_HASH=1 prints a hash of the 2 MB of RAM at the end.
 */

#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <libretro.h>

static const char *opt_link, *opt_timing, *opt_drc;
static int verbose;

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
	va_list ap;

	if (!verbose && level < RETRO_LOG_WARN && strstr(fmt, "sio1") == NULL)
		return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static bool environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *)data)->log = log_cb;
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return true;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char **)data = getenv("HOST_DIR") ? getenv("HOST_DIR") : "/tmp";
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *var = data;

		var->value = NULL;
		if (strcmp(var->key, "pcsx_rearmed_link_cable") == 0)
			var->value = opt_link;
		else if (strcmp(var->key, "pcsx_rearmed_link_timing") == 0)
			var->value = opt_timing;
		else if (strcmp(var->key, "pcsx_rearmed_drc") == 0)
			var->value = opt_drc;
		else if (strcmp(var->key, "pcsx_rearmed_bios") == 0)
			var->value = "HLE";
		else if (strcmp(var->key, "pcsx_rearmed_memcard2") == 0)
			var->value = "disabled";
		return var->value != NULL;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*(bool *)data = false;
		return true;
	default:
		return false;
	}
}

static void video(const void *d, unsigned w, unsigned h, size_t p)
{
	(void)d; (void)w; (void)h; (void)p;
}
static void audio(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch(const int16_t *d, size_t n) { (void)d; return n; }
static void input_poll(void) {}
static int16_t input_state(unsigned a, unsigned b, unsigned c, unsigned d)
{
	(void)a; (void)b; (void)c; (void)d;
	return 0;
}

struct result {
	uint32_t magic, role, status, count, sum, wrong, errors, pad;
	uint16_t rt_poll[64], rt_irq[64];
};

#define SYM(name) do { \
	*(void **)&name = dlsym(core, #name); \
	if (name == NULL) { fprintf(stderr, "no %s\n", #name); return 2; } \
} while (0)

int main(int argc, char *argv[])
{
	void (*retro_set_environment)(retro_environment_t);
	void (*retro_set_video_refresh)(retro_video_refresh_t);
	void (*retro_set_audio_sample)(retro_audio_sample_t);
	void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
	void (*retro_set_input_poll)(retro_input_poll_t);
	void (*retro_set_input_state)(retro_input_state_t);
	void (*retro_init)(void);
	void (*retro_deinit)(void);
	bool (*retro_load_game)(const struct retro_game_info *);
	void (*retro_unload_game)(void);
	void (*retro_run)(void);
	void *(*retro_get_memory_data)(unsigned);
	size_t (*retro_serialize_size)(void);
	bool (*retro_serialize)(void *, size_t);
	bool (*retro_unserialize)(const void *, size_t);
	struct retro_game_info info;
	const struct result *r;
	struct timespec t0, t1;
	uint8_t *ram;
	void *core;
	unsigned jitter = 0;
	int frames, max_frames = 600, i, role;
	int state_at = getenv("HOST_STATE_AT") ? atoi(getenv("HOST_STATE_AT")) : -1;

	if (argc < 7) {
		fprintf(stderr, "usage: %s <core.so> <program.exe> <role> "
			"<link> <timing> <drc> [frames]\n", argv[0]);
		return 2;
	}
	role = atoi(argv[3]);
	opt_link = argv[4];
	opt_timing = argv[5];
	opt_drc = argv[6];
	if (argc > 7)
		max_frames = atoi(argv[7]);
	verbose = getenv("HOST_VERBOSE") != NULL;
	if (getenv("HOST_JITTER"))
		jitter = atoi(getenv("HOST_JITTER")) * 2654435761u + role;

	/* As root the lightrec builds manage to map the console's RAM at
	 * address 0, and a NULL pointer to it is more than the savestate
	 * code (or this program) can take. Keep that page occupied. */
	mmap(NULL, 4096, PROT_NONE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

	core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (core == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		return 2;
	}
	SYM(retro_set_environment); SYM(retro_set_video_refresh);
	SYM(retro_set_audio_sample); SYM(retro_set_audio_sample_batch);
	SYM(retro_set_input_poll); SYM(retro_set_input_state);
	SYM(retro_init); SYM(retro_deinit); SYM(retro_load_game);
	SYM(retro_unload_game); SYM(retro_run); SYM(retro_get_memory_data);
	SYM(retro_serialize_size); SYM(retro_serialize); SYM(retro_unserialize);

	retro_set_environment(environment);
	retro_set_video_refresh(video);
	retro_set_audio_sample(audio);
	retro_set_audio_sample_batch(audio_batch);
	retro_set_input_poll(input_poll);
	retro_set_input_state(input_state);
	retro_init();

	memset(&info, 0, sizeof(info));
	info.path = argv[2];
	if (!retro_load_game(&info)) {
		fprintf(stderr, "could not load %s\n", argv[2]);
		return 2;
	}
	ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	if (ram == NULL) {
		fprintf(stderr, "no RAM\n");
		return 2;
	}
	memcpy(ram + 0x100200, &role, 4);
	r = (const struct result *)(ram + 0x100000);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (frames = 0; frames < max_frames; frames++) {
		retro_run();
		if (r->magic == 0x314b4e4c && r->status != 0)
			break;
		if (frames == state_at) {
			/* a savestate round trip in the middle of it all */
			size_t size = retro_serialize_size();
			void *buf = calloc(1, size);
			bool ok = buf != NULL;
			FILE *f;

			if (ok && getenv("HOST_STATE_LOAD")) {
				f = fopen(getenv("HOST_STATE_LOAD"), "rb");
				ok = f && fread(buf, 1, size, f) > 0;
				if (f) fclose(f);
			}
			else
				ok = ok && retro_serialize(buf, size);
			if (ok && getenv("HOST_STATE_SAVE")) {
				f = fopen(getenv("HOST_STATE_SAVE"), "wb");
				ok = f && fwrite(buf, 1, size, f) == size;
				if (f) fclose(f);
			}
			ok = ok && retro_unserialize(buf, size);
			fprintf(stderr, "savestate round trip at frame %d: %s\n",
				frames, ok ? "ok" : "FAILED");
			free(buf);
		}
		if (jitter) {
			jitter = jitter * 1103515245u + 12345;
			if ((jitter >> 16) % 3 == 0)
				usleep((jitter >> 8) % 4000);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	printf("role %u status %x count %u sum %08x wrong %u errors %u\n",
		r->role, r->status, r->count, r->sum, r->wrong, r->errors);
	printf("poll");
	for (i = 0; i < 64; i++)
		printf(" %u", r->rt_poll[i]);
	printf("\nirq ");
	for (i = 0; i < 64; i++)
		printf(" %u", r->rt_irq[i]);
	printf("\n");
	fprintf(stderr, "role %d: %d frames in %.2fs host\n", role, frames + 1,
		(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);

	/* a few more frames so that the other console can finish too */
	for (i = 0; i < 4 && role != 3; i++) {
		retro_run();
		usleep(20000);
	}
	if (getenv("HOST_RAM_HASH")) {
		uint64_t h = 0xcbf29ce484222325ull;
		uint32_t n;

		for (n = 0; n < 0x200000; n++)
			h = (h ^ ram[n]) * 0x100000001b3ull;
		printf("ram %016llx after %d frames\n", (unsigned long long)h,
			frames);
	}
	i = !(r->status == 1 && r->wrong == 0);
	fflush(stdout);
	retro_unload_game();
	retro_deinit(); /* RAM is gone after this */
	return i;
}
