/*
 * (C) Gražvydas "notaz" Ignotas, 2009-2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <linux/soundcard.h>

#include "common/input.h"
#include "gp2x/in_gp2x.h"
#include "common/menu.h"
#include "warm/warm.h"
#include "plugin_lib.h"
#include "pl_gun_ts.h"
#include "blit320.h"
#include "in_tsbutton.h"
#include "main.h"
#include "menu.h"
#include "plat.h"
#include "pcnt.h"
#include "../plugins/gpulib/cspace.h"

int gp2x_dev_id;

static int fbdev = -1, memdev = -1, battdev = -1, mixerdev = -1;
static volatile unsigned short *memregs;
static volatile unsigned int   *memregl;
static void *fb_vaddrs[2];
static unsigned int fb_paddrs[2];
static int fb_work_buf;
static int cpu_clock_allowed, have_warm;
static unsigned int saved_video_regs[2][6];
#define FB_VRAM_SIZE (320*240*2*2*2) // 2 buffers with space for 24bpp mode

static unsigned short *psx_vram;
static unsigned int psx_vram_padds[512];
static int psx_step, psx_width, psx_height, psx_bpp;
static int psx_offset_x, psx_offset_y;
static int fb_offset_x, fb_offset_y;

// TODO: get rid of this
struct vout_fbdev;
struct vout_fbdev *layer_fb;
int g_layer_x, g_layer_y, g_layer_w, g_layer_h;

int omap_enable_layer(int enabled)
{
	return 0;
}

static void caanoo_init(void);


static void *fb_flip(void)
{
	memregl[0x406C>>2] = memregl[0x446C>>2] = fb_paddrs[fb_work_buf];
	memregl[0x4058>>2] |= 0x10;
	memregl[0x4458>>2] |= 0x10;
	fb_work_buf ^= 1;
	return fb_vaddrs[fb_work_buf];
}

static void pollux_changemode(int bpp, int is_bgr)
{
	int code = 0, bytes = 2;
	unsigned int r;

	printf("changemode: %dbpp %s\n", bpp, is_bgr ? "bgr" : "rgb");

	memregl[0x4004>>2] = 0x00ef013f;
	memregl[0x4000>>2] |= 1 << 3;

	switch (bpp)
	{
		case 8:
			code = 0x443a;
			bytes = 1;
			break;
		case 16:
			code = is_bgr ? 0xc342 : 0x4432;
			bytes = 2;
			break;
		case 24:
			code = is_bgr ? 0xc653 : 0x4653;
			bytes = 3;
			break;
		default:
			printf("unhandled bpp request: %d\n", bpp);
			return;
	}

	// program both MLCs so that TV-out works
	memregl[0x405c>>2] = memregl[0x445c>>2] = bytes;
	memregl[0x4060>>2] = memregl[0x4460>>2] = 320 * bytes;

	r = memregl[0x4058>>2];
	r = (r & 0xffff) | (code << 16) | 0x10;
	memregl[0x4058>>2] = r;

	r = memregl[0x4458>>2];
	r = (r & 0xffff) | (code << 16) | 0x10;
	memregl[0x4458>>2] = r;
}

/* note: both PLLs are programmed the same way,
 * the databook incorrectly states that PLL1 differs */
static int decode_pll(unsigned int reg)
{
	long long v;
	int p, m, s;

	p = (reg >> 18) & 0x3f;
	m = (reg >> 8) & 0x3ff;
	s = reg & 0xff;

	if (p == 0)
		p = 1;

	v = 27000000; // master clock
	v = v * m / (p << s);
	return v;
}

int plat_cpu_clock_get(void)
{
	return decode_pll(memregl[0xf004>>2]) / 1000000;
}

int plat_cpu_clock_apply(int mhz)
{
	int adiv, mdiv, pdiv, sdiv = 0;
	int i, vf000, vf004;

	if (!cpu_clock_allowed)
		return -1;
	if (mhz == plat_cpu_clock_get())
		return 0;

	// m = MDIV, p = PDIV, s = SDIV
	#define SYS_CLK_FREQ 27
	pdiv = 9;
	mdiv = (mhz * pdiv) / SYS_CLK_FREQ;
	if (mdiv & ~0x3ff)
		return -1;
	vf004 = (pdiv<<18) | (mdiv<<8) | sdiv;

	// attempt to keep the AHB divider close to 250, but not higher
	for (adiv = 1; mhz / adiv > 250; adiv++)
		;

	vf000 = memregl[0xf000>>2];
	vf000 = (vf000 & ~0x3c0) | ((adiv - 1) << 6);
	memregl[0xf000>>2] = vf000;
	memregl[0xf004>>2] = vf004;
	memregl[0xf07c>>2] |= 0x8000;
	for (i = 0; (memregl[0xf07c>>2] & 0x8000) && i < 0x100000; i++)
		;

	printf("clock set to %dMHz, AHB set to %dMHz\n", mhz, mhz / adiv);

	// stupid pll share hack - must restart audio
	extern long SPUopen(void);
	extern long SPUclose(void);
	SPUclose();
	SPUopen();

	return 0;
}

int plat_get_bat_capacity(void)
{
	unsigned short magic_val = 0;

	if (battdev < 0)
		return -1;
	if (read(battdev, &magic_val, sizeof(magic_val)) != sizeof(magic_val))
		return -1;
	switch (magic_val) {
	default:
	case 1:	return 100;
	case 2: return 66;
	case 3: return 40;
	case 4: return 0;
	}
}

#define TIMER_BASE3 0x1980
#define TIMER_REG(x) memregl[(TIMER_BASE3 + x) >> 2]

static __attribute__((unused)) unsigned int timer_get(void)
{
	TIMER_REG(0x08) |= 0x48;  /* run timer, latch value */
	return TIMER_REG(0);
}

static void timer_cleanup(void)
{
	TIMER_REG(0x40) = 0x0c;	/* be sure clocks are on */
	TIMER_REG(0x08) = 0x23;	/* stop the timer, clear irq in case it's pending */
	TIMER_REG(0x00) = 0;	/* clear counter */
	TIMER_REG(0x40) = 0;	/* clocks off */
	TIMER_REG(0x44) = 0;	/* dividers back to default */
}

void plat_video_menu_enter(int is_rom_loaded)
{
	if (pl_vout_buf != NULL) {
		if (psx_bpp == 16)
			// have to do rgb conversion for menu bg
			bgr555_to_rgb565(pl_vout_buf, pl_vout_buf, 320*240*2);
		else
			memset(pl_vout_buf, 0, 320*240*2);
	}

	pollux_changemode(16, 0);
}

void plat_video_menu_begin(void)
{
}

void plat_video_menu_end(void)
{
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_leave(void)
{
	if (psx_vram == NULL) {
		fprintf(stderr, "GPU plugin did not provide vram\n");
		exit(1);
	}

	if (gp2x_dev_id == GP2X_DEV_CAANOO)
		in_set_config_int(in_name_to_id("evdev:pollux-analog"),
			IN_CFG_ABS_DEAD_ZONE, analog_deadzone);

	memset(g_menuscreen_ptr, 0, 320*240 * psx_bpp/8);
	g_menuscreen_ptr = fb_flip();
	memset(g_menuscreen_ptr, 0, 320*240 * psx_bpp/8);

	pollux_changemode(psx_bpp, 1);
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	bgr555_to_rgb565(pl_vout_buf, pl_vout_buf, 320*240*2);
	*w = 320;
	*h = 240;
	*bpp = psx_bpp;
	return pl_vout_buf;
}

void plat_minimize(void)
{
}

static void pl_vout_set_raw_vram(void *vram)
{
	int i;

	psx_vram = vram;

	if (vram == NULL)
		return;

	if ((long)psx_vram & 0x7ff)
		fprintf(stderr, "GPU plugin did not align vram\n");

	for (i = 0; i < 512; i++) {
		psx_vram[i * 1024] = 0; // touch
		psx_vram_padds[i] = warm_virt2phys(&psx_vram[i * 1024]);
	}
}

static void spend_cycles(int loops)
{
	asm volatile (
		"   mov  r0,%0    ;\n"
		"0: subs r0,r0,#1 ;\n"
		"   bgt  0b"
		:: "r" (loops) : "cc", "r0");
}

#define DMA_BASE6 0x0300
#define DMA_REG(x) memregl[(DMA_BASE6 + x) >> 2]

/* this takes ~1.5ms, while ldm/stm ~1.95ms */
static void raw_flip_dma(int x, int y)
{
	unsigned int dst = fb_paddrs[fb_work_buf] +
			(fb_offset_y * 320 + fb_offset_x) * psx_bpp / 8;
	int spsx_line = y + psx_offset_y;
	int spsx_offset = (x + psx_offset_x) & 0x3f8;
	int dst_stride = 320 * psx_bpp / 8;
	int len = psx_width * psx_bpp / 8;
	int i;

	warm_cache_op_all(WOP_D_CLEAN);
	pcnt_start(PCNT_BLIT);

	dst &= ~7;
	len &= ~7;

	if (DMA_REG(0x0c) & 0x90000) {
		printf("already runnig DMA?\n");
		DMA_REG(0x0c) = 0x100000;
	}
	if ((DMA_REG(0x2c) & 0x0f) < 5) {
		printf("DMA queue busy?\n");
		DMA_REG(0x24) = 1;
	}

	for (i = psx_height; i > 0; i--, spsx_line += psx_step, dst += dst_stride) {
		while ((DMA_REG(0x2c) & 0x0f) < 4)
			spend_cycles(10);

		// XXX: it seems we must always set all regs, what is autoincrement there for?
		DMA_REG(0x20) = 1;		// queue wait cmd
		DMA_REG(0x10) = psx_vram_padds[spsx_line & 511] + spsx_offset * 2; // DMA src
		DMA_REG(0x14) = dst;		// DMA dst
		DMA_REG(0x18) = len - 1;	// len
		DMA_REG(0x1c) = 0x80000;	// go
	}

	if (psx_bpp == 16) {
		pl_vout_buf = g_menuscreen_ptr;
		pl_print_hud(320, fb_offset_y + psx_height, fb_offset_x);
	}

	g_menuscreen_ptr = fb_flip();
	pl_rearmed_cbs.flip_cnt++;

	pcnt_end(PCNT_BLIT);
}

#define make_flip_func(name, blitfunc)                                                  \
static void name(int x, int y)                                                          \
{                                                                                       \
        unsigned short *vram = psx_vram;                                                \
        unsigned char *dst = (unsigned char *)g_menuscreen_ptr +                        \
                        (fb_offset_y * 320 + fb_offset_x) * psx_bpp / 8;                \
        unsigned int src = (y + psx_offset_y) * 1024 + x + psx_offset_x;                \
        int dst_stride = 320 * psx_bpp / 8;                                             \
        int len = psx_width * psx_bpp / 8;                                              \
        int i;                                                                          \
                                                                                        \
        pcnt_start(PCNT_BLIT);                                                          \
                                                                                        \
        for (i = psx_height; i > 0; i--, src += psx_step * 1024, dst += dst_stride) {   \
                src &= 1024*512-1;                                                      \
                blitfunc(dst, vram + src, len);                                         \
        }                                                                               \
                                                                                        \
        if (psx_bpp == 16) {                                                            \
                pl_vout_buf = g_menuscreen_ptr;                                         \
                pl_print_hud(320, fb_offset_y + psx_height, fb_offset_x);               \
        }                                                                               \
                                                                                        \
        g_menuscreen_ptr = fb_flip();                                                   \
        pl_rearmed_cbs.flip_cnt++;                                                      \
                                                                                        \
        pcnt_end(PCNT_BLIT);                                                            \
}

make_flip_func(raw_flip_soft, memcpy)
make_flip_func(raw_flip_soft_368, blit320_368)
make_flip_func(raw_flip_soft_512, blit320_512)
make_flip_func(raw_flip_soft_640, blit320_640)

static void *pl_vout_set_mode(int w, int h, int bpp)
{
	static int old_w, old_h, old_bpp;
	int poff_w, poff_h, w_max;

	if (!w || !h || !bpp || (w == old_w && h == old_h && bpp == old_bpp))
		return NULL;

	printf("psx mode: %dx%d@%d\n", w, h, bpp);

	switch (w + (bpp != 16)) {
	case 640:
		pl_rearmed_cbs.pl_vout_raw_flip = raw_flip_soft_640;
		w_max = 640;
		break;
	case 512:
		pl_rearmed_cbs.pl_vout_raw_flip = raw_flip_soft_512;
		w_max = 512;
		break;
	case 384:
	case 368:
		pl_rearmed_cbs.pl_vout_raw_flip = raw_flip_soft_368;
		w_max = 368;
		break;
	default:
		pl_rearmed_cbs.pl_vout_raw_flip = have_warm ? raw_flip_dma : raw_flip_soft;
		w_max = 320;
		break;
	}

	psx_step = 1;
	if (h > 256) {
		psx_step = 2;
		h /= 2;
	}

	poff_w = poff_h = 0;
	if (w > w_max) {
		poff_w = w / 2 - w_max / 2;
		w = w_max;
	}
	fb_offset_x = 0;
	if (w < 320)
		fb_offset_x = 320/2 - w / 2;
	if (h > 240) {
		poff_h = h / 2 - 240/2;
		h = 240;
	}
	fb_offset_y = 240/2 - h / 2;

	psx_offset_x = poff_w;
	psx_offset_y = poff_h;
	psx_width = w;
	psx_height = h;
	psx_bpp = bpp;

	if (fb_offset_x || fb_offset_y) {
		// not fullscreen, must clear borders
		memset(g_menuscreen_ptr, 0, 320*240 * psx_bpp/8);
		g_menuscreen_ptr = fb_flip();
		memset(g_menuscreen_ptr, 0, 320*240 * psx_bpp/8);
	}

	pollux_changemode(bpp, 1);

	pl_set_gun_rect(fb_offset_x, fb_offset_y, w > 320 ? 320 : w, h);

	return NULL;
}

static void *pl_vout_flip(void)
{
	return NULL;
}

static void save_multiple_regs(unsigned int *dest, int base, int count)
{
	const volatile unsigned int *regs = memregl + base / 4;
	int i;

	for (i = 0; i < count; i++)
		dest[i] = regs[i];
}

static void restore_multiple_regs(int base, const unsigned int *src, int count)
{
	volatile unsigned int *regs = memregl + base / 4;
	int i;

	for (i = 0; i < count; i++)
		regs[i] = src[i];
}

void plat_init(void)
{
	const char *main_fb_name = "/dev/fb0";
	struct fb_fix_screeninfo fbfix;
	int rate, timer_div, timer_div2;
	int fbdev, ret, warm_ret;
	FILE *f;

	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1) {
		perror("open(/dev/mem) failed");
		exit(1);
	}

	memregs	= mmap(0, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	if (memregs == MAP_FAILED) {
		perror("mmap(memregs) failed");
		exit(1);
	}
	memregl = (volatile void *)memregs;

	// save video regs of both MLCs
	save_multiple_regs(saved_video_regs[0], 0x4058, ARRAY_SIZE(saved_video_regs[0]));
	save_multiple_regs(saved_video_regs[1], 0x4458, ARRAY_SIZE(saved_video_regs[1]));

	fbdev = open(main_fb_name, O_RDWR);
	if (fbdev == -1) {
		fprintf(stderr, "%s: ", main_fb_name);
		perror("open");
		exit(1);
	}

	ret = ioctl(fbdev, FBIOGET_FSCREENINFO, &fbfix);
	if (ret == -1) {
		perror("ioctl(fbdev) failed");
		exit(1);
	}
	printf("framebuffer: \"%s\" @ %08lx\n", fbfix.id, fbfix.smem_start);
	fb_paddrs[0] = fbfix.smem_start;
	fb_paddrs[1] = fb_paddrs[0] + 320*240*4; // leave space for 24bpp

	fb_vaddrs[0] = mmap(0, FB_VRAM_SIZE, PROT_READ|PROT_WRITE,
				MAP_SHARED, memdev, fb_paddrs[0]);
	if (fb_vaddrs[0] == MAP_FAILED) {
		perror("mmap(fb_vaddrs) failed");
		exit(1);
	}
	fb_vaddrs[1] = (char *)fb_vaddrs[0] + 320*240*4;

	pollux_changemode(16, 0);
	g_menuscreen_w = 320;
	g_menuscreen_h = 240;
	g_menuscreen_ptr = fb_flip();

	g_menubg_ptr = calloc(320*240*2, 1);
	if (g_menubg_ptr == NULL) {
		fprintf(stderr, "OOM\n");
		exit(1);
	}

	warm_ret = warm_init();
	have_warm = warm_ret == 0;
	warm_change_cb_upper(WCB_B_BIT, 1);

	/* some firmwares have sys clk on PLL0, we can't adjust CPU clock
	 * by reprogramming the PLL0 then, as it overclocks system bus */
	if ((memregl[0xf000>>2] & 0x03000030) == 0x01000000)
		cpu_clock_allowed = 1;
	else {
		cpu_clock_allowed = 0;
		fprintf(stderr, "unexpected PLL config (%08x), overclocking disabled\n",
			memregl[0xf000>>2]);
	}

	/* find what PLL1 runs at, for the timer */
	rate = decode_pll(memregl[0xf008>>2]);
	printf("PLL1 @ %dHz\n", rate);

	/* setup timer */
	timer_div = (rate + 500000) / 1000000;
	timer_div2 = 0;
	while (timer_div > 256) {
		timer_div /= 2;
		timer_div2++;
	}
	if (1 <= timer_div && timer_div <= 256 && timer_div2 < 4) {
		int timer_rate = (rate >> timer_div2) / timer_div;
		if (TIMER_REG(0x08) & 8) {
			fprintf(stderr, "warning: timer in use, overriding!\n");
			timer_cleanup();
		}
		if (timer_rate != 1000000)
			fprintf(stderr, "warning: timer drift %d us\n", timer_rate - 1000000);

		timer_div2 = (timer_div2 + 3) & 3;
		TIMER_REG(0x44) = ((timer_div - 1) << 4) | 2;	/* using PLL1 */
		TIMER_REG(0x40) = 0x0c;				/* clocks on */
		TIMER_REG(0x08) = 0x68 | timer_div2;		/* run timer, clear irq, latch value */
	}
	else
		fprintf(stderr, "warning: could not make use of timer\n");

	/* setup DMA */
	DMA_REG(0x0c) = 0x20000; // pending IRQ clear

	battdev = open("/dev/pollux_batt", O_RDONLY);
	if (battdev < 0)
		perror("Warning: could't open pollux_batt");

	f = fopen("/dev/accel", "rb");
	if (f) {
		printf("detected Caanoo\n");
		gp2x_dev_id = GP2X_DEV_CAANOO;
		fclose(f);
	}
	else {
		printf("detected Wiz\n");
		gp2x_dev_id = GP2X_DEV_WIZ;
		in_gp2x_init();
	}

	in_tsbutton_init();
	in_probe();
	if (gp2x_dev_id == GP2X_DEV_CAANOO)
		caanoo_init();

	mixerdev = open("/dev/mixer", O_RDWR);
	if (mixerdev == -1)
		perror("open(/dev/mixer)");

	pl_rearmed_cbs.pl_vout_flip = pl_vout_flip;
	pl_rearmed_cbs.pl_vout_raw_flip = have_warm ? raw_flip_dma : raw_flip_soft;
	pl_rearmed_cbs.pl_vout_set_mode = pl_vout_set_mode;
	pl_rearmed_cbs.pl_vout_set_raw_vram = pl_vout_set_raw_vram;

	psx_width = 320;
	psx_height = 240;
	psx_bpp = 16;

	pl_rearmed_cbs.screen_w = 320;
	pl_rearmed_cbs.screen_h = 240;
}

void plat_finish(void)
{
	warm_finish();
	timer_cleanup();

	memset(fb_vaddrs[0], 0, FB_VRAM_SIZE);
	restore_multiple_regs(0x4058, saved_video_regs[0], ARRAY_SIZE(saved_video_regs[0]));
	restore_multiple_regs(0x4458, saved_video_regs[1], ARRAY_SIZE(saved_video_regs[1]));
	memregl[0x4058>>2] |= 0x10;
	memregl[0x4458>>2] |= 0x10;
	munmap(fb_vaddrs[0], FB_VRAM_SIZE);
	close(fbdev);

	if (battdev >= 0)
		close(battdev);
	if (mixerdev >= 0)
		close(mixerdev);
	munmap((void *)memregs, 0x20000);
	close(memdev);
}

/* Caanoo stuff, perhaps move later */
#include <linux/input.h>

struct in_default_bind in_evdev_defbinds[] = {
	{ KEY_UP,	IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ KEY_DOWN,	IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ KEY_LEFT,	IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ KEY_RIGHT,	IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	{ BTN_TOP,	IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ BTN_THUMB,	IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ BTN_THUMB2,	IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ BTN_TRIGGER,	IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	{ BTN_BASE3,	IN_BINDTYPE_PLAYER12, DKEY_START },
	{ BTN_BASE4,	IN_BINDTYPE_PLAYER12, DKEY_SELECT },
	{ BTN_TOP2,	IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ BTN_PINKIE,	IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ BTN_BASE,	IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
	{ 0, 0, 0 },
};

static const char * const caanoo_keys[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_UP]        = "Up",
	[KEY_LEFT]      = "Left",
	[KEY_RIGHT]     = "Right",
	[KEY_DOWN]      = "Down",
	[BTN_TRIGGER]   = "A",
	[BTN_THUMB]     = "X",
	[BTN_THUMB2]    = "B",
	[BTN_TOP]       = "Y",
	[BTN_TOP2]      = "L",
	[BTN_PINKIE]    = "R",
	[BTN_BASE]      = "Home",
	[BTN_BASE2]     = "Lock",
	[BTN_BASE3]     = "I",
	[BTN_BASE4]     = "II",
	[BTN_BASE5]     = "Push",
};

struct haptic_data {
	int count;
	struct {
		short time, strength;
	} actions[120];
};

#define HAPTIC_IOCTL_MAGIC	'I'
#define HAPTIC_PLAY_PATTERN	_IOW(HAPTIC_IOCTL_MAGIC, 4, struct haptic_data)
#define HAPTIC_INDIVIDUAL_MODE	_IOW(HAPTIC_IOCTL_MAGIC, 5, unsigned int)
#define HAPTIC_SET_VIB_LEVEL	_IOW(HAPTIC_IOCTL_MAGIC, 9, unsigned int)

static int hapticdev = -1;
static struct haptic_data haptic_seq;

static int haptic_init(void)
{
	int i, ret, v1, v2;
	char buf[128], *p;
	FILE *f;

	f = fopen("haptic.txt", "r");
	if (f == NULL) {
		perror("fopen(haptic.txt)");
		return -1;
	}

	for (i = 0; i < sizeof(haptic_seq.actions) / sizeof(haptic_seq.actions[0]); ) {
		p = fgets(buf, sizeof(buf), f);
		if (p == NULL)
			break;
		while (*p != 0 && *p == ' ')
			p++;
		if (*p == 0 || *p == ';' || *p == '#')
			continue;

		ret = sscanf(buf, "%d %d", &v1, &v2);
		if (ret != 2) {
			fprintf(stderr, "can't parse: %s", buf);
			continue;
		}

		haptic_seq.actions[i].time = v1;
		haptic_seq.actions[i].strength = v2;
		i++;
	}
	fclose(f);

	if (i == 0) {
		fprintf(stderr, "bad haptic.txt\n");
		return -1;
	}
	haptic_seq.count = i;

	hapticdev = open("/dev/isa1200", O_RDWR | O_NONBLOCK);
	if (hapticdev == -1) {
		perror("open(/dev/isa1200)");
		return -1;
	}

	i = 0;
	ret  = ioctl(hapticdev, HAPTIC_INDIVIDUAL_MODE, &i);	/* use 2 of them */
	i = 3;
	ret |= ioctl(hapticdev, HAPTIC_SET_VIB_LEVEL, &i);	/* max */
	if (ret != 0) {
		fprintf(stderr, "haptic ioctls failed\n");
		close(hapticdev);
		hapticdev = -1;
		return -1;
	}

	return 0;
}

void plat_trigger_vibrate(void)
{
	int ret;

	if (hapticdev == -2)
		return; // it's broken
	if (hapticdev < 0) {
		ret = haptic_init();
		if (ret < 0) {
			hapticdev = -2;
			return;
		}
	}

	ioctl(hapticdev, HAPTIC_PLAY_PATTERN, &haptic_seq);
}

/* Wiz stuff */
struct in_default_bind in_gp2x_defbinds[] =
{
	/* MXYZ SACB RLDU */
	{ GP2X_BTN_UP,		IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ GP2X_BTN_DOWN,	IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ GP2X_BTN_LEFT,	IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ GP2X_BTN_RIGHT,	IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	{ GP2X_BTN_X,		IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ GP2X_BTN_B,		IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ GP2X_BTN_A,		IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	{ GP2X_BTN_Y,		IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ GP2X_BTN_L,		IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ GP2X_BTN_R,		IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ GP2X_BTN_START,	IN_BINDTYPE_PLAYER12, DKEY_START },
	{ GP2X_BTN_SELECT,	IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
	{ GP2X_BTN_VOL_UP,	IN_BINDTYPE_EMU, SACTION_VOLUME_UP },
	{ GP2X_BTN_VOL_DOWN,	IN_BINDTYPE_EMU, SACTION_VOLUME_DOWN },
	{ 0, 0, 0 },
};

void plat_step_volume(int is_up)
{
	static int volume = 50;
	int ret, val;

	if (mixerdev < 0)
		return;

	if (is_up) {
		volume += 5;
		if (volume > 255) volume = 255;
	}
	else {
		volume -= 5;
		if (volume < 0) volume = 0;
	}
	val = volume;
	val |= val << 8;

 	ret = ioctl(mixerdev, SOUND_MIXER_WRITE_PCM, &val);
	if (ret == -1)
		perror("WRITE_PCM");
}

// unused dummy for in_gp2x
volatile unsigned short *gp2x_memregs;

static void caanoo_init(void)
{
	in_set_config(in_name_to_id("evdev:pollux-analog"), IN_CFG_KEY_NAMES,
		      caanoo_keys, sizeof(caanoo_keys));
}
