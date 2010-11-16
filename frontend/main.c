#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../gui/Linux.h"
#include "../libpcsxcore/misc.h"

int UseGui;

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

static void CreateMemcard(char *filename, char *conf_mcd) {
	struct stat buf;

	make_path(conf_mcd, MAXPATHLEN, MEMCARD_DIR, filename);

	/* Only create a memory card if an existing one does not exist */
	if (stat(conf_mcd, &buf) == -1) {
		SysPrintf(_("Creating memory card: %s\n"), conf_mcd);
		CreateMcd(conf_mcd);
	}
}

int main(int argc, char *argv[])
{
	char file[MAXPATHLEN] = "";
	char path[MAXPATHLEN];
	int runcd = 0;
	int loadst = 0;
	int i;

	// what is the name of the config file?
	// it may be redefined by -cfg on the command line
	strcpy(cfgfile_basename, "pcsx.cfg");

	SetIsoFile(NULL);
	Config.PsxOut = 1;

	// read command line options
	for (i = 1; i < argc; i++) {
		     if (!strcmp(argv[i], "-nogui")) UseGui = FALSE;
		else if (!strcmp(argv[i], "-psxout")) Config.PsxOut = 1;
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

			SetIsoFile(isofilename);
			runcd = 1;
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

	// try to load config
	// if the config file doesn't exist
	if (LoadConfig() == -1) {
		// Uh oh, no config file found, use some defaults
		Config.PsxAuto = 1;

		snprintf(Config.BiosDir, sizeof(Config.BiosDir), "." BIOS_DIR);
		snprintf(Config.PluginsDir, sizeof(Config.PluginsDir), "." PLUGINS_DIR);

		// Update available plugins, but not GUI
		//UpdatePluginsBIOS();

		// Pick some defaults, if they're available
/*
		set_default_plugin(GpuConfS.plist[0], Config.Gpu);
		set_default_plugin(SpuConfS.plist[0], Config.Spu);
		set_default_plugin(CdrConfS.plist[0], Config.Cdr);
		set_default_plugin(Pad1ConfS.plist[0], Config.Pad1);
		set_default_plugin(Pad2ConfS.plist[0], Config.Pad2);
		set_default_plugin(BiosConfS.plist[0], Config.Bios);
*/
		// create & load default memcards if they don't exist
		CreateMemcard("card1.mcd", Config.Mcd1);
		CreateMemcard("card2.mcd", Config.Mcd2);

		LoadMcds(Config.Mcd1, Config.Mcd2);

		SaveConfig();
	}

	snprintf(Config.PatchesDir, sizeof(Config.PatchesDir), "." PATCHES_DIR);
/*
	// switch to plugin dotdir
	// this lets plugins work without modification!
	gchar *plugin_default_dir = g_build_filename(getenv("HOME"), PLUGINS_DIR, NULL);
	chdir(plugin_default_dir);
	g_free(plugin_default_dir);
*/
	if (SysInit() == -1) return 1;

	// if !gui
	{
		// the following only occurs if the gui isn't started
		if (LoadPlugins() == -1) {
			SysMessage("Failed loading plugins!");
			return 1;
		}

		if (OpenPlugins() == -1) {
			return 1;
		}

		SysReset();
		CheckCdrom();

		if (file[0] != '\0') {
			Load(file);
		} else {
			if (runcd) {
				if (LoadCdrom() == -1) {
					ClosePlugins();
					printf(_("Could not load CD-ROM!\n"));
					return -1;
				}
			}
		}

		// If a state has been specified, then load that
		if (loadst) {
			StatesC = loadst - 1;
			char *state_filename = get_state_filename(StatesC);
			LoadState(state_filename);
			free(state_filename);
		}

		psxCpu->Execute();
	}

	return 0;
}

int SysInit() {
	emuLog = stdout;

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

	if (Config.PsxOut) {
		static char linestart = 1;
		int l = strlen(msg);

		printf(linestart ? " * %s" : "%s", msg);

		if (l > 0 && msg[l - 1] == '\n') {
			linestart = 1;
		} else {
			linestart = 0;
		}
	}

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

void *SysLoadLibrary(const char *lib) {
	return dlopen(lib, RTLD_NOW);
}

void *SysLoadSym(void *lib, const char *sym) {
	return dlsym(lib, sym);
}

const char *SysLibError() {
	return dlerror();
}

void SysCloseLibrary(void *lib) {
	dlclose(lib);
}


