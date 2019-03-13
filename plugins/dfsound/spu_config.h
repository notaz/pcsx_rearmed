// user settings

typedef struct
{
 int        iVolume;
 int        iXAPitch;
 int        iUseReverb;
 int        iUseInterpolation;
 int        iTempo;
 int        idiablofix;
 int        iUseThread;
 int        iUseFixedUpdates;  // output fixed number of samples/frame

 // status
 int        iThreadAvail;
} SPUConfig;

extern SPUConfig spu_config;
