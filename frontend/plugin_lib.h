
enum {
	DKEY_SELECT = 0,
	DKEY_L3,
	DKEY_R3,
	DKEY_START,
	DKEY_UP,
	DKEY_RIGHT,
	DKEY_DOWN,
	DKEY_LEFT,
	DKEY_L2,
	DKEY_R2,
	DKEY_L1,
	DKEY_R1,
	DKEY_TRIANGLE,
	DKEY_CIRCLE,
	DKEY_CROSS,
	DKEY_SQUARE,
};
extern int in_type1, in_type2;
extern int in_keystate, in_state_gun, in_a1[2], in_a2[2];
extern int in_adev[2], in_adev_axis[2][2];
extern int in_enable_vibration;

extern void *pl_vout_buf;

void  pl_text_out16(int x, int y, const char *texto, ...);
void  pl_start_watchdog(void);
void *pl_prepare_screenshot(int *w, int *h, int *bpp);
void  pl_init(void);
void  pl_print_hud(int w, int h, int xborder);

void  pl_timing_prepare(int is_pal);
void  pl_frame_limit(void);

void  pl_update_gun(int *xn, int *xres, int *y, int *in);

struct rearmed_cbs {
	void  (*pl_get_layer_pos)(int *x, int *y, int *w, int *h);
	int   (*pl_vout_open)(void);
	void *(*pl_vout_set_mode)(int w, int h, int bpp);
	void *(*pl_vout_flip)(void);
	void  (*pl_vout_close)(void);
	// these are only used by some frontends
	void  (*pl_vout_raw_flip)(int x, int y);
	void  (*pl_vout_set_raw_vram)(void *vram);
	// some stats, for display by some plugins
	int flips_per_sec, cpu_usage;
	float vsps_cur; // currect vsync/s
	// gpu options
	int   frameskip;
	int   fskip_advice;
	unsigned int *gpu_frame_count;
	unsigned int *gpu_hcnt;
	unsigned int flip_cnt; // increment manually if not using pl_vout_flip
	unsigned int screen_w, screen_h; // gles plugin wants this
	struct {
		int   allow_interlace; // 0 off, 1 on, 2 guess
	} gpu_neon;
	struct {
		int   iUseDither;
		int   dwActFixes;
		float fFrameRateHz;
		int   dwFrameRateTicks;
	} gpu_peops;
	struct {
		int   abe_hack;
		int   no_light, no_blend;
		int   lineskip;
	} gpu_unai;
	struct {
		int   dwActFixes;
		int   bDrawDither, iFilterType, iFrameTexType;
		int   iUseMask, bOpaquePass, bAdvancedBlend, bUseFastMdec;
		int   iVRamSize, iTexGarbageCollection;
	} gpu_peopsgl;
};

extern struct rearmed_cbs pl_rearmed_cbs;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif
