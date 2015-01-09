// user settings

typedef struct
{
 int        iVolume;
 int        iXAPitch;
 int        iUseReverb;
 int        iUseInterpolation;
 int        iTempo;
 int        iUseThread;

 // status
 int        iThreadAvail;
} SPUConfig;

extern SPUConfig spu_config;
