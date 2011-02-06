/*
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "main.h"
#include "plugin.h"
#include "../libpcsxcore/misc.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"

// from softgpu plugin
extern int iUseDither;
extern int UseFrameSkip;
extern int UseFrameLimit;
extern uint32_t dwActFixes;
extern float fFrameRateHz;
extern int dwFrameRateTicks;

// sound plugin
extern int iUseReverb;
extern int iUseInterpolation;
extern int iXAPitch;
extern int iSPUIRQWait;
extern int iUseTimer;

enum sched_action emu_action;
void do_emu_action(void);

static void ChangeWorkingDirectory(char *exe)
{
	char exepath[1024];
	char *s;
	snprintf(exepath, sizeof(exepath), "%s", exe);
	s = strrchr(exepath, '/');
	if (s != NULL) {
		*s = '\0';
		chdir(exepath);
	}
}

int maemo_main(int argc, char **argv)
{
	ChangeWorkingDirectory("c");
	char file[MAXPATHLEN] = "";
	char path[MAXPATHLEN];
	const char *cdfile = NULL;
	int loadst = 0;
	int i;

	strcpy(Config.BiosDir, "/home/user/MyDocs");
	strcpy(Config.PluginsDir, "/opt/maemo/usr/games/plugins");
	snprintf(Config.PatchesDir, sizeof(Config.PatchesDir), "/opt/maemo/usr/games" PATCHES_DIR);
	Config.PsxAuto = 1;
	
	// read command line options
	for (i = 1; i < argc; i++) {
		     if (!strcmp(argv[i], "-psxout")) Config.PsxOut = 1;
		else if (!strcmp(argv[i], "-load")) loadst = atol(argv[++i]);
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
		else if (!strcmp(argv[i],"-frameskip")){
		
		int tv_reg=atol(argv[++i]);
		if (tv_reg>0){
		UseFrameSkip=1;
		fFrameRateHz = (tv_reg==1)?50.0f:  59.94f;
		dwFrameRateTicks = (100000*100 / (unsigned long)(fFrameRateHz*100));
		}
		}
		else if (!strcmp(argv[i],"-sputhreaded")){
			iUseTimer=1;
		}
		else if (!strcmp(argv[i],"-nosound")){
				strcpy(Config.Spu, "spunull.so");
		}
		else if(!strcmp(argv[i], "-bdir"))	sprintf(Config.BiosDir, "%s", argv[++i]);
		else if(!strcmp(argv[i], "-bios"))	sprintf(Config.Bios, "%s", argv[++i]);
		else if (!strcmp(argv[i],"-gles")){
		strcpy(Config.Gpu, "gpuGLES.so");
		}
		else if (!strcmp(argv[i], "-cdda"))		Config.Cdda = 1;
		else if (!strcmp(argv[i], "-xa"))		Config.Xa = 1;
		else if (!strcmp(argv[i], "-rcnt"))		Config.RCntFix = 1 ;
		else if (!strcmp(argv[i], "-sio"))		Config.Sio = 1;
		else if (!strcmp(argv[i], "-spuirq"))	Config.SpuIrq = 1;
		else if (!strcmp(argv[i], "-vsync"))	Config.VSyncWA = 1;
		else if (!strcmp(argv[i], "-h") ||
			 !strcmp(argv[i], "-help") ||
			 !strcmp(argv[i], "--help")) {
			 printf(PACKAGE_NAME " " PACKAGE_VERSION "\n");
			 printf("%s\n", _(
							" pcsx [options] [file]\n"
							"\toptions:\n"
							"\t-cdfile FILE\tRuns a CD image file\n"
							"\t-psxout\t\tEnable PSX output\n"
							"\t-nosound\t\tDisable sound using spunull plugin\n"
							"\t-sputhreaded\t\tMove sound to separate thread\n"
							"\t-frameskip\t\tEnable frameskip\n"
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

	hildon_init(&argc, &argv);
	
	if (cdfile)
		set_cd_image(cdfile);

	if (SysInit() == -1)
		return 1;

	if (LoadPlugins() == -1) {
		SysMessage("Failed loading plugins!");
		return 1;
	}

	if (OpenPlugins() == -1) {
		return 1;
	}
	plugin_call_rearmed_cbs();

	CheckCdrom();
	SysReset();

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
		char state_filename[MAXPATHLEN];
		int ret = get_state_filename(state_filename, sizeof(state_filename), loadst - 1);
		if (ret == 0)
			ret = LoadState(state_filename);
		printf("%s state %s\n", ret ? "failed to load" : "loaded", state_filename);
	}

	if (ready_to_go)
		maemo_init();
	else
	{
		printf ("somethings goes wrong, maybe you forgot -cdfile ? \n");
		return 0;
	}

	while (1)
	{
		stop = 0;
		emu_action = SACTION_NONE;

		psxCpu->Execute();
		if (emu_action != SACTION_NONE)
			do_emu_action();
	}

	return 0;
}

