/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../libpcsxcore/sio.h"

#include "Linux.h"
#include "ConfDlg.h"

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#include <X11/extensions/XTest.h>

enum {
	RUN = 0,
	RUN_CD,
};

gboolean UseGui = TRUE;

static void CreateMemcard(char *filename, char *conf_mcd) {
	gchar *mcd;
	struct stat buf;

	mcd = g_build_filename(getenv("HOME"), MEMCARD_DIR, filename, NULL);

	strcpy(conf_mcd, mcd);

	/* Only create a memory card if an existing one does not exist */
	if (stat(mcd, &buf) == -1) {
		SysPrintf(_("Creating memory card: %s\n"), mcd);
		CreateMcd(mcd);
	}

	g_free (mcd);
}

/* Create a directory under the $HOME directory, if that directory doesn't already exist */
static void CreateHomeConfigDir(char *directory) {
	struct stat buf;

	if (stat(directory, &buf) == -1) {
		gchar *dir_name = g_build_filename (getenv("HOME"), directory, NULL);
		mkdir(dir_name, S_IRWXU | S_IRWXG);
		g_free (dir_name);
	}
}

static void CheckSubDir() {
	// make sure that ~/.pcsx exists
	CreateHomeConfigDir(PCSX_DOT_DIR);

	CreateHomeConfigDir(BIOS_DIR);
	CreateHomeConfigDir(MEMCARD_DIR);
	CreateHomeConfigDir(STATES_DIR);
	CreateHomeConfigDir(PLUGINS_DIR);
	CreateHomeConfigDir(PLUGINS_CFG_DIR);
	CreateHomeConfigDir(CHEATS_DIR);
	CreateHomeConfigDir(PATCHES_DIR);
}

static void ScanPlugins(gchar* scandir) {
	// scan for plugins and configuration tools
	DIR *dir;
	struct dirent *ent;

	gchar *linkname;
	gchar *filename;

	/* Any plugins found will be symlinked to the following directory */
	dir = opendir(scandir);
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			filename = g_build_filename (scandir, ent->d_name, NULL);

			if (match(filename, ".*\\.so$") == 0 &&
				match(filename, ".*\\.dylib$") == 0 &&
				match(filename, "cfg.*") == 0) {
				continue;	/* Skip this file */
			} else {
				/* Create a symlink from this file to the directory ~/.pcsx/plugin */
				linkname = g_build_filename (getenv("HOME"), PLUGINS_DIR, ent->d_name, NULL);
				symlink(filename, linkname);

				/* If it's a config tool, make one in the cfg dir as well.
				   This allows plugins with retarded cfg finding to work :- ) */
				if (match(filename, "cfg.*") == 1) {
					linkname = g_build_filename (getenv("HOME"), PLUGINS_CFG_DIR, ent->d_name, NULL);
					symlink(filename, linkname);
				}
				g_free (linkname);
			}
			g_free (filename);
		}
		closedir(dir);
	}
}

static void ScanBios(gchar* scandir) {
	// scan for bioses
	DIR *dir;
	struct dirent *ent;

	gchar *linkname;
	gchar *filename;

	/* Any bioses found will be symlinked to the following directory */
	dir = opendir(scandir);
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			filename = g_build_filename(scandir, ent->d_name, NULL);

			if (match(filename, ".*\\.bin$") == 0 &&
				match(filename, ".*\\.BIN$") == 0) {
				continue;	/* Skip this file */
			} else {
				/* Create a symlink from this file to the directory ~/.pcsx/plugin */
				linkname = g_build_filename(getenv("HOME"), BIOS_DIR, ent->d_name, NULL);
				symlink(filename, linkname);

				g_free(linkname);
			}
			g_free(filename);
		}
		closedir(dir);
	}
}

static void CheckSymlinksInPath(char* dotdir) {
	DIR *dir;
	struct dirent *ent;
	struct stat stbuf;
	gchar *linkname;

	dir = opendir(dotdir);
	if (dir == NULL) {
		SysMessage(_("Could not open directory: '%s'\n"), dotdir);
		return;
	}

	/* Check for any bad links in the directory. If the remote
	   file no longer exists, remove the link */
	while ((ent = readdir(dir)) != NULL) {
		linkname = g_strconcat (dotdir, ent->d_name, NULL);

		if (stat(linkname, &stbuf) == -1) {
			/* File link is bad, remove it */
			unlink(linkname);
		}
		g_free (linkname);
	}
	closedir(dir);
}

static void ScanAllPlugins (void) {
	gchar *currentdir;

	// scan some default locations to find plugins
	ScanPlugins("/usr/lib/games/psemu/");
	ScanPlugins("/usr/lib/games/psemu/lib/");
	ScanPlugins("/usr/lib/games/psemu/config/");
	ScanPlugins("/usr/local/lib/games/psemu/lib/");
	ScanPlugins("/usr/local/lib/games/psemu/config/");
	ScanPlugins("/usr/local/lib/games/psemu/");
	ScanPlugins("/usr/lib64/games/psemu/");
	ScanPlugins("/usr/lib64/games/psemu/lib/");
	ScanPlugins("/usr/lib64/games/psemu/config/");
	ScanPlugins("/usr/local/lib64/games/psemu/lib/");
	ScanPlugins("/usr/local/lib64/games/psemu/config/");
	ScanPlugins("/usr/local/lib64/games/psemu/");
	ScanPlugins("/usr/lib32/games/psemu/");
	ScanPlugins("/usr/lib32/games/psemu/lib/");
	ScanPlugins("/usr/lib32/games/psemu/config/");
	ScanPlugins("/usr/local/lib32/games/psemu/lib/");
	ScanPlugins("/usr/local/lib32/games/psemu/config/");
	ScanPlugins("/usr/local/lib32/games/psemu/");
	ScanPlugins(DEF_PLUGIN_DIR);
	ScanPlugins(DEF_PLUGIN_DIR "/lib");
	ScanPlugins(DEF_PLUGIN_DIR "/lib64");
	ScanPlugins(DEF_PLUGIN_DIR "/lib32");
	ScanPlugins(DEF_PLUGIN_DIR "/config");

	// scan some default locations to find bioses
	ScanBios("/usr/lib/games/psemu");
	ScanBios("/usr/lib/games/psemu/bios");
	ScanBios("/usr/lib64/games/psemu");
	ScanBios("/usr/lib64/games/psemu/bios");
	ScanBios("/usr/lib32/games/psemu");
	ScanBios("/usr/lib32/games/psemu/bios");
	ScanBios("/usr/share/psemu");
	ScanBios("/usr/share/psemu/bios");
	ScanBios("/usr/share/pcsx");
	ScanBios("/usr/share/pcsx/bios");
	ScanBios("/usr/local/lib/games/psemu");
	ScanBios("/usr/local/lib/games/psemu/bios");
	ScanBios("/usr/local/lib64/games/psemu");
	ScanBios("/usr/local/lib64/games/psemu/bios");
	ScanBios("/usr/local/lib32/games/psemu");
	ScanBios("/usr/local/lib32/games/psemu/bios");
	ScanBios("/usr/local/share/psemu");
	ScanBios("/usr/local/share/psemu/bios");
	ScanBios("/usr/local/share/pcsx");
	ScanBios("/usr/local/share/pcsx/bios");
	ScanBios(PACKAGE_DATA_DIR);
	ScanBios(PSEMU_DATA_DIR);
	ScanBios(PACKAGE_DATA_DIR "/bios");
	ScanBios(PSEMU_DATA_DIR "/bios");

	currentdir = g_strconcat(getenv("HOME"), "/.psemu-plugins/", NULL);
	ScanPlugins(currentdir);
	g_free(currentdir);

	currentdir = g_strconcat(getenv("HOME"), "/.psemu/", NULL);
	ScanPlugins(currentdir);
	g_free(currentdir);

	// Check for bad links in ~/.pcsx/plugins/
	currentdir = g_build_filename(getenv("HOME"), PLUGINS_DIR, NULL);
	CheckSymlinksInPath(currentdir);
	g_free(currentdir);

	// Check for bad links in ~/.pcsx/plugins/cfg
	currentdir = g_build_filename(getenv("HOME"), PLUGINS_CFG_DIR, NULL);
	CheckSymlinksInPath(currentdir);
	g_free(currentdir);

	// Check for bad links in ~/.pcsx/bios
	currentdir = g_build_filename(getenv("HOME"), BIOS_DIR, NULL);
	CheckSymlinksInPath(currentdir);
	g_free(currentdir);
}

// Set the default plugin name
void set_default_plugin(char *plugin_name, char *conf_plugin_name) {
	if (strlen(plugin_name) != 0) {
		strcpy(conf_plugin_name, plugin_name);
		printf("Picking default plugin: %s\n", plugin_name);
	} else
		printf("No default plugin could be found for %s\n", conf_plugin_name);
}

int main(int argc, char *argv[]) {
	char file[MAXPATHLEN] = "";
	char path[MAXPATHLEN];
	int runcd = RUN;
	int loadst = 0;
	int i;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	// what is the name of the config file?
	// it may be redefined by -cfg on the command line
	strcpy(cfgfile_basename, "pcsx.cfg");

	// read command line options
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-runcd")) runcd = RUN_CD;
		else if (!strcmp(argv[i], "-nogui")) UseGui = FALSE;
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
			runcd = RUN_CD;
		}
		else if (!strcmp(argv[i], "-h") ||
			 !strcmp(argv[i], "-help") ||
			 !strcmp(argv[i], "--help")) {
			 printf(PACKAGE_STRING "\n");
			 printf("%s\n", _(
							" pcsx [options] [file]\n"
							"\toptions:\n"
							"\t-runcd\t\tRuns CD-ROM\n"
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

	if (UseGui) gtk_init(NULL, NULL);

	CheckSubDir();
	ScanAllPlugins();

	// try to load config
	// if the config file doesn't exist
	if (LoadConfig() == -1) {
		if (!UseGui) {
			printf(_("PCSX cannot be configured without using the GUI -- you should restart without -nogui.\n"));
			return 1;
		}

		// Uh oh, no config file found, use some defaults
		Config.PsxAuto = 1;

		gchar *str_bios_dir = g_strconcat(getenv("HOME"), BIOS_DIR, NULL);
		strcpy(Config.BiosDir, str_bios_dir);
		g_free(str_bios_dir);

		gchar *str_plugin_dir = g_strconcat(getenv("HOME"), PLUGINS_DIR, NULL);
		strcpy(Config.PluginsDir, str_plugin_dir);
		g_free(str_plugin_dir);

		gtk_init(NULL, NULL);

		// Update available plugins, but not GUI
		UpdatePluginsBIOS();

		// Pick some defaults, if they're available
		set_default_plugin(GpuConfS.plist[0], Config.Gpu);
		set_default_plugin(SpuConfS.plist[0], Config.Spu);
		set_default_plugin(CdrConfS.plist[0], Config.Cdr);
		set_default_plugin(Pad1ConfS.plist[0], Config.Pad1);
		set_default_plugin(Pad2ConfS.plist[0], Config.Pad2);
		set_default_plugin(BiosConfS.plist[0], Config.Bios);

		// create & load default memcards if they don't exist
		CreateMemcard("card1.mcd", Config.Mcd1);
		CreateMemcard("card2.mcd", Config.Mcd2);

		LoadMcds(Config.Mcd1, Config.Mcd2);

		SaveConfig();
	}

	gchar *str_patches_dir = g_strconcat(getenv("HOME"), PATCHES_DIR, NULL);
	strcpy(Config.PatchesDir,  str_patches_dir);
	g_free(str_patches_dir);

	// switch to plugin dotdir
	// this lets plugins work without modification!
	gchar *plugin_default_dir = g_build_filename(getenv("HOME"), PLUGINS_DIR, NULL);
	chdir(plugin_default_dir);
	g_free(plugin_default_dir);

	if (UseGui) SetIsoFile(NULL);

	if (SysInit() == -1) return 1;

	if (UseGui) {
		StartGui();
	} else {
		// the following only occurs if the gui isn't started
		if (LoadPlugins() == -1) {
			SysErrorMessage(_("Error"), _("Failed loading plugins!"));
			return 1;
		}

		if (OpenPlugins() == -1 || plugins_configured() == FALSE) {
			return 1;
		}

		SysReset();
		CheckCdrom();

		if (file[0] != '\0') {
			Load(file);
		} else {
			if (runcd == RUN_CD) {
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
			gchar *state_filename = get_state_filename(StatesC);
			LoadState(state_filename);
			g_free(state_filename);
		}

		psxCpu->Execute();
	}

	return 0;
}

int SysInit() {
#ifdef EMU_LOG
#ifndef LOG_STDOUT
	emuLog = fopen("emuLog.txt","wb");
#else
	emuLog = stdout;
#endif
	setvbuf(emuLog, NULL, _IONBF, 0);
#endif

	if (EmuInit() == -1) {
		printf(_("PSX emulator couldn't be initialized.\n"));
		return -1;
	}

	LoadMcds(Config.Mcd1, Config.Mcd2);	/* TODO Do we need to have this here, or in the calling main() function?? */

	if (Config.Debug) {
		StartDebugger();
	}

	return 0;
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

#ifdef EMU_LOG
#ifndef LOG_STDOUT
	fprintf(emuLog, "%s", msg);
#endif
#endif
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

static void SysDisableScreenSaver() {
	static time_t fake_key_timer = 0;
	static char first_time = 1, has_test_ext = 0, t = 1;
	Display *display;
	extern unsigned long gpuDisp;

	display = (Display *)gpuDisp;

	if (first_time) {
		// check if xtest is available
		int a, b, c, d;
		has_test_ext = XTestQueryExtension(display, &a, &b, &c, &d);

		first_time = 0;
	}

	if (has_test_ext && fake_key_timer < time(NULL)) {
		XTestFakeRelativeMotionEvent(display, t *= -1, 0, 0);
		fake_key_timer = time(NULL) + 55;
	}
}

void SysUpdate() {
	PADhandleKey(PAD1_keypressed());
	PADhandleKey(PAD2_keypressed());

	SysDisableScreenSaver();
}

/* ADB TODO Replace RunGui() with StartGui ()*/
void SysRunGui() {
	StartGui();
}
