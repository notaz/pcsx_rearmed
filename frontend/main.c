/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "plugin.h"
#include "pcnt.h"
#include "menu.h"
#include "../gui/Linux.h"
#include "../libpcsxcore/misc.h"
#include "../plugins/cdrcimg/cdrcimg.h"
#include "common/plat.h"
#include "common/input.h"

int ready_to_go;
int UseGui;
static char *(*real_getenv)(const char *name);

static void make_path(char *buf, size_t size, const char *dir, const char *fname)
{
	if (fname)
		snprintf(buf, size, ".%s%s", dir, fname);
	else
		snprintf(buf, size, ".%s", dir);
}
#define MAKE_PATH(buf, dir, fname) \
	make_path(buf, sizeof(buf), dir, fname)

static void create_profile_dir(const char *directory) {
	char path[MAXPATHLEN];

	MAKE_PATH(path, directory, NULL);
	mkdir(path, S_IRWXU | S_IRWXG);
}

static void CheckSubDir() {
	// make sure that ~/.pcsx exists
	create_profile_dir(PCSX_DOT_DIR);

	create_profile_dir(BIOS_DIR);
	create_profile_dir(MEMCARD_DIR);
	create_profile_dir(STATES_DIR);
	create_profile_dir(PLUGINS_DIR);
	create_profile_dir(PLUGINS_CFG_DIR);
	create_profile_dir(CHEATS_DIR);
	create_profile_dir(PATCHES_DIR);
}

void set_cd_image(const char *fname)
{
	const char *ext;
	int len;
	
	len = strlen(fname);
	ext = fname;
	if (len > 2)
		ext = fname + len - 2;

	if (strcasecmp(ext, ".z") == 0) {
		SetIsoFile(NULL);
		cdrcimg_set_fname(fname);
		strcpy(Config.Cdr, "builtin_cdrcimg");
	} else {
		SetIsoFile(fname);
		strcpy(Config.Cdr, "builtin_cdr");
	}
}

int main(int argc, char *argv[])
{
	char file[MAXPATHLEN] = "";
	char path[MAXPATHLEN];
	const char *cdfile = NULL;
	int loadst = 0;
	void *tmp;
	int i;

	tmp = dlopen("/lib/libdl.so.2", RTLD_LAZY);
	if (tmp == NULL)
		tmp = dlopen("/lib32/libdl.so.2", RTLD_LAZY);
	if (tmp != NULL)
		real_getenv = dlsym(tmp, "getenv");
	if (real_getenv == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		return 1;
	}
	dlclose(tmp);

	// what is the name of the config file?
	// it may be redefined by -cfg on the command line
	strcpy(cfgfile_basename, "pcsx.cfg");

	emuLog = stdout;
	SetIsoFile(NULL);

	// read command line options
	for (i = 1; i < argc; i++) {
		     if (!strcmp(argv[i], "-psxout")) Config.PsxOut = 1;
		else if (!strcmp(argv[i], "-load")) loadst = atol(argv[++i]);
		else if (!strcmp(argv[i], "-cfg")) {
			if (i+1 >= argc) break;
			strncpy(cfgfile_basename, argv[++i], MAXPATHLEN-100);	/* TODO buffer overruns */
			printf("Using config file %s.\n", cfgfile_basename);
		}
		else if (!strcmp(argv[i], "-cdfile")) {
			char isofilename[MAXPATHLEN];

			if (i+1 >= argc) break;
			strncpy(isofilename, argv[++i], MAXPATHLEN);
			if (isofilename[0] != '/') {
				getcwd(path, MAXPATHLEN);
				if (strlen(path) + strlen(isofilename) + 1 < MAXPATHLEN) {
					strcat(path, "/");
					strcat(path, isofilename);
					strcpy(isofilename, path);
				} else
					isofilename[0] = 0;
			}

			cdfile = isofilename;
		}
		else if (!strcmp(argv[i], "-h") ||
			 !strcmp(argv[i], "-help") ||
			 !strcmp(argv[i], "--help")) {
			 printf(PACKAGE_NAME " " PACKAGE_VERSION "\n");
			 printf("%s\n", _(
							" pcsx [options] [file]\n"
							"\toptions:\n"
							"\t-cdfile FILE\tRuns a CD image file\n"
							"\t-nogui\t\tDon't open the GTK GUI\n"
							"\t-cfg FILE\tLoads desired configuration file (default: ~/.pcsx/pcsx.cfg)\n"
							"\t-psxout\t\tEnable PSX output\n"
							"\t-load STATENUM\tLoads savestate STATENUM (1-5)\n"
							"\t-h -help\tDisplay this message\n"
							"\tfile\t\tLoads file\n"));
			 return 0;
		} else {
			strncpy(file, argv[i], MAXPATHLEN);
			if (file[0] != '/') {
				getcwd(path, MAXPATHLEN);
				if (strlen(path) + strlen(file) + 1 < MAXPATHLEN) {
					strcat(path, "/");
					strcat(path, file);
					strcpy(file, path);
				} else
					file[0] = 0;
			}
		}
	}

	memset(&Config, 0, sizeof(PcsxConfig));
	strcpy(Config.Net, "Disabled");

	CheckSubDir();
//	ScanAllPlugins();

	MAKE_PATH(Config.Mcd1, MEMCARD_DIR, "card1.mcd");
	MAKE_PATH(Config.Mcd2, MEMCARD_DIR, "card2.mcd");
	strcpy(Config.Bios, "HLE");
	strcpy(Config.BiosDir, "./");

	strcpy(Config.PluginsDir, "plugins");
	strcpy(Config.Gpu, "builtin_gpu");
	strcpy(Config.Spu, "builtin_spu");
	strcpy(Config.Cdr, "builtin_cdr");
	strcpy(Config.Pad1, "builtin_pad");
	strcpy(Config.Pad2, "builtin_pad");
	Config.PsxAuto = 1;

	snprintf(Config.PatchesDir, sizeof(Config.PatchesDir), "." PATCHES_DIR);
/*
	// switch to plugin dotdir
	// this lets plugins work without modification!
	gchar *plugin_default_dir = g_build_filename(getenv("HOME"), PLUGINS_DIR, NULL);
	chdir(plugin_default_dir);
	g_free(plugin_default_dir);
*/

	if (cdfile)
		set_cd_image(cdfile);

	if (SysInit() == -1)
		return 1;

	// frontend stuff
	in_init();
	in_probe();
	plat_init();
	menu_init();

	if (LoadPlugins() == -1) {
		SysMessage("Failed loading plugins!");
		return 1;
	}
	pcnt_hook_plugins();

	if (OpenPlugins() == -1) {
		return 1;
	}

	SysReset();
	CheckCdrom();

	if (file[0] != '\0') {
		if (Load(file) != -1)
			ready_to_go = 1;
	} else {
		if (cdfile) {
			if (LoadCdrom() == -1) {
				ClosePlugins();
				printf(_("Could not load CD-ROM!\n"));
				return -1;
			}
			ready_to_go = 1;
		}
	}

	// If a state has been specified, then load that
	if (loadst) {
		StatesC = loadst - 1;
		char *state_filename = get_state_filename(StatesC);
		int ret = LoadState(state_filename);
		printf("%s state %s\n", ret ? "failed to load" : "loaded", state_filename);
		free(state_filename);
	}

	if (ready_to_go)
		menu_prepare_emu();
	else
		menu_loop();

	while (1)
	{
		psxCpu->Execute();
		menu_loop();
	}

	return 0;
}

int SysInit() {
	if (EmuInit() == -1) {
		printf("PSX emulator couldn't be initialized.\n");
		return -1;
	}

	LoadMcds(Config.Mcd1, Config.Mcd2);	/* TODO Do we need to have this here, or in the calling main() function?? */

	if (Config.Debug) {
		StartDebugger();
	}

	return 0;
}

void SysRunGui() {
        printf("SysRunGui\n");
}

void StartGui() {
        printf("StartGui\n");
}

void SysReset() {
	EmuReset();

	// hmh core forgets this
	CDR_stop();
}

void SysClose() {
	EmuShutdown();
	ReleasePlugins();

	StopDebugger();

	if (emuLog != NULL) fclose(emuLog);
}

void SysUpdate() {
	PADhandleKey(PAD1_keypressed());
	PADhandleKey(PAD2_keypressed());
}

void UpdateMenuSlots() {
}

void OnFile_Exit() {
	printf("OnFile_Exit\n");
	plat_finish();
	SysClose();
	exit(0);
}

void state_save(gchar *state_filename) {
        char Text[MAXPATHLEN + 20];

        GPU_updateLace();

        if (SaveState(state_filename) == 0)
                sprintf(Text, _("Saved state %s."), state_filename);
        else
                sprintf(Text, _("Error saving state %s!"), state_filename);

        GPU_displayText(Text);
}

void state_load(gchar *state_filename) {
	int ret;
	char Text[MAXPATHLEN + 20];
	FILE *fp;

	// check if the state file actually exists
	fp = fopen(state_filename, "rb");
	if (fp == NULL) {
		// file does not exist
		return;
	}

	fclose(fp);

	ret = CheckState(state_filename);

	if (ret == 0) {
		SysReset();
		ret = LoadState(state_filename);
	}

	if (ret == 0) {
		// Check the CD-ROM is valid
		if (CheckCdrom() == -1) {
			ClosePlugins();
			SysRunGui();
			return;
		}

		sprintf(Text, _("Loaded state %s."), state_filename);
	} else {
		sprintf(Text, _("Error loading state %s!"), state_filename);
	}
	GPU_displayText(Text);
}

char *get_state_filename(int i) {
	char SStateFile[256];
	char trimlabel[33];
	int j;

	strncpy(trimlabel, CdromLabel, 32);
	trimlabel[32] = 0;
	for (j = 31; j >= 0; j--)
		if (trimlabel[j] == ' ')
			trimlabel[j] = 0;
		else
			continue;

	snprintf(SStateFile, sizeof(SStateFile), "." STATES_DIR "%.32s-%.9s.%3.3d",
		trimlabel, CdromId, i);

	return strdup(SStateFile);
}

void SysPrintf(const char *fmt, ...) {
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	fprintf(emuLog, "%s", msg);
}

void SysMessage(const char *fmt, ...) {
        va_list list;
        char msg[512];

        va_start(list, fmt);
        vsprintf(msg, fmt, list);
        va_end(list);

        if (msg[strlen(msg) - 1] == '\n')
                msg[strlen(msg) - 1] = 0;

	fprintf(stderr, "%s\n", msg);
}

#if 1
/* this is to avoid having to hack every plugin to stop using $HOME */
char *getenv(const char *name)
{
	static char ret[8] = ".";

	if (name && strcmp(name, "HOME") == 0 &&
			((int)name >> 28) == 0) // HACK: let libs find home
		return ret;

	return real_getenv(name);
}
#endif

/* we hook statically linked plugins here */
static const char *builtin_plugins[] = {
	"builtin_gpu", "builtin_spu", "builtin_cdr", "builtin_pad",
	"builtin_cdrcimg",
};

static const int builtin_plugin_ids[] = {
	PLUGIN_GPU, PLUGIN_SPU, PLUGIN_CDR, PLUGIN_PAD,
	PLUGIN_CDRCIMG,
};

void *SysLoadLibrary(const char *lib) {
	const char *tmp = strrchr(lib, '/');
	int i;

	printf("dlopen %s\n", lib);
	if (tmp != NULL) {
		tmp++;
		for (i = 0; i < ARRAY_SIZE(builtin_plugins); i++)
			if (strcmp(tmp, builtin_plugins[i]) == 0)
				return (void *)(long)(PLUGIN_DL_BASE + builtin_plugin_ids[i]);
	}

	return dlopen(lib, RTLD_NOW);
}

void *SysLoadSym(void *lib, const char *sym) {
	unsigned int plugid = (unsigned int)(long)lib;

	if (PLUGIN_DL_BASE <= plugid && plugid < PLUGIN_DL_BASE + ARRAY_SIZE(builtin_plugins))
		return plugin_link(plugid - PLUGIN_DL_BASE, sym);

	return dlsym(lib, sym);
}

const char *SysLibError() {
	return dlerror();
}

void SysCloseLibrary(void *lib) {
	unsigned int plugid = (unsigned int)(long)lib;

	if (PLUGIN_DL_BASE <= plugid && plugid < PLUGIN_DL_BASE + ARRAY_SIZE(builtin_plugins))
		return;

	dlclose(lib);
}


