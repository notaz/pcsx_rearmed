/*
 * SIO1 (serial port / link cable) emulation: binding to the emulator.
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * Nothing here is active unless a link mode other than SIO1_LINK_OFF is
 * set, in which case 0x1f801050..0x1f80105f are served by the port model
 * in sio1.c instead of plain i/o memory.
 */

#include <stdio.h>
#include <string.h>
#include "psxcommon.h"
#include "psxhw.h"
#include "psxevents.h"
#include "r3000a.h"
#include "sio1.h"
#include "sio1_link.h"

#define HEARTBEAT	(PSXCLK / 60)

int sio1_active;

static struct {
	enum sio1_link_mode mode;
	struct sio1_port port;
	struct sio1_link *link;
	/* the model wants a clock that does not wrap every two minutes */
	u64 time;
	u32 cycle;	/* psxRegs.cycle that .time belongs to */
	u32 line_delay;
	int in_handler;
	int last_status;
} sio1;

static u64 now64(void)
{
	s32 d = psxRegs.cycle - sio1.cycle;

	if (d > 0)
		sio1.time += d;
	sio1.cycle = psxRegs.cycle;
	return sio1.time;
}

static void schedule(u64 when)
{
	u64 now = now64();
	u32 d = 1;

	if (when > now)
		d = when - now > HEARTBEAT ? HEARTBEAT : (u32)(when - now);
	set_event(PSXINT_SIO1, d);
}

/* without a peer the model can be followed exactly */
static void schedule_alone(void)
{
	u64 next = sio1p_next_event(&sio1.port);
	u64 beat = now64() + HEARTBEAT;

	schedule(next < beat ? next : beat);
}

static int linked(void)
{
	return sio1.link != NULL
		&& sio1_link_status(sio1.link) == SIO1_LS_LINKED;
}

static void report_link(void)
{
	int status;

	if (sio1.link == NULL)
		return;
	status = sio1_link_status(sio1.link);
	if (status == sio1.last_status)
		return;
	sio1.last_status = status;
	SysPrintf("sio1: %s\n", sio1_link_message(sio1.link));
}

static void port_emit(void *ctx, const struct sio1_event *ev)
{
	if (sio1.mode == SIO1_LINK_LOOPBACK)
		sio1p_deliver(&sio1.port, ev);
	else if (sio1.link != NULL)
		sio1_link_send(sio1.link, ev);
}

static void port_irq(void *ctx)
{
	psxHu32ref(0x1070) |= SWAPu32(0x100);
	/* raised by a register write: have the cpu look right away */
	if (!sio1.in_handler)
		set_event(PSXINT_NEWDRC_CHECK, 1);
}

static u64 access_begin(void)
{
	u64 t = now64();

	if (sio1.link != NULL)
		t = sio1_link_access_begin(sio1.link, t);
	return t;
}

static void access_end(u64 t)
{
	int was_linked = linked();

	if (sio1.link != NULL) {
		sio1_link_access_end(sio1.link, t);
		report_link();
	}
	/* linked, the next stop is on the link's grid and stays there */
	if (!was_linked || !linked())
		schedule_alone();
}

unsigned int sio1HwRead8(unsigned int add)
{
	u64 t = access_begin();
	u32 v = sio1p_read(&sio1.port, t, add, 1);

	access_end(t);
	return v;
}

unsigned int sio1HwRead16(unsigned int add)
{
	u64 t = access_begin();
	u32 v = sio1p_read(&sio1.port, t, add, 2);

	access_end(t);
	return v;
}

unsigned int sio1HwRead32(unsigned int add)
{
	u64 t = access_begin();
	u32 v = sio1p_read(&sio1.port, t, add, 4);

	access_end(t);
	return v;
}

void sio1HwWrite8(unsigned int add, unsigned int value)
{
	u64 t = access_begin();

	sio1p_write(&sio1.port, t, add, value, 1);
	access_end(t);
}

void sio1HwWrite16(unsigned int add, unsigned int value)
{
	u64 t = access_begin();

	sio1p_write(&sio1.port, t, add, value, 2);
	access_end(t);
}

void sio1HwWrite32(unsigned int add, unsigned int value)
{
	u64 t = access_begin();

	sio1p_write(&sio1.port, t, add, value, 4);
	access_end(t);
}

void sio1Interrupt(void)
{
	u64 t, next;

	if (!sio1_active)
		return;
	t = now64();
	sio1.in_handler = 1;
	if (sio1.link != NULL) {
		next = sio1_link_stop(sio1.link, t);
		report_link();
		sio1.in_handler = 0;
		schedule(next);
		return;
	}
	sio1p_advance(&sio1.port, t);
	sio1.in_handler = 0;
	schedule_alone();
}

/* what the port's inputs see when nothing drives them from outside */
static void set_inputs(u64 t, int lines)
{
	struct sio1_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = SIO1_EV_LINES;
	ev.cycle = t;
	ev.lines = lines;
	sio1.port.s.rxq_r = sio1.port.s.rxq_w;
	sio1p_deliver(&sio1.port, &ev);
	sio1p_advance(&sio1.port, t);
}

void sio1SetLinkTiming(unsigned int line_delay)
{
	sio1.line_delay = line_delay;
}

enum sio1_link_mode sio1GetLinkMode(void)
{
	return sio1.mode;
}

int sio1SetLinkMode(enum sio1_link_mode mode, const char *name)
{
	char err[200];
	int ret = 0;
	u64 t;

	if (sio1.link != NULL) {
		sio1_link_close(sio1.link);
		sio1.link = NULL;
	}
	if (mode == SIO1_LINK_OFF) {
		sio1.mode = mode;
		sio1_active = 0;
		psxRegs.interrupt &= ~(1u << PSXINT_SIO1);
		return 0;
	}
	if (!sio1_active) {
		sio1p_init(&sio1.port);
		sio1.port.emit = port_emit;
		sio1.port.irq = port_irq;
		sio1.cycle = psxRegs.cycle;
	}
	sio1_active = 1;
	t = now64();
	sio1p_advance(&sio1.port, t);

	if (mode == SIO1_LINK_CONSOLE_A || mode == SIO1_LINK_CONSOLE_B) {
		/* both consoles have to count time the same way */
		u32 config = Config.cycle_multiplier;

		sio1.mode = SIO1_LINK_UNPLUGGED;
		set_inputs(t, 0);
		sio1.link = sio1_link_open(name, mode == SIO1_LINK_CONSOLE_B,
				&sio1.port, config, sio1.line_delay,
				err, sizeof(err));
		if (sio1.link == NULL) {
			SysPrintf("sio1: %s\n", err);
			ret = -1;
		}
		else {
			sio1.mode = mode;
			sio1.last_status = -1;
			report_link();
		}
	}
	else {
		sio1.mode = mode;
		set_inputs(t, mode == SIO1_LINK_LOOPBACK
				? sio1.port.s.out_lines : 0);
	}
	schedule(t + 1);
	return ret;
}

void sio1Reset(void)
{
	u64 t;

	if (!sio1_active)
		return;
	/* psxRegs.cycle starts over, the port's clock goes on */
	sio1.cycle = psxRegs.cycle;
	t = access_begin();
	sio1p_reset(&sio1.port, t);
	sio1p_advance(&sio1.port, t);
	access_end(t);
	schedule(t + 1);
}

void sio1Shutdown(void)
{
	sio1SetLinkMode(SIO1_LINK_OFF, NULL);
}

/* Savestates: a block of its own at the very end of the file, so that
 * states made before it existed still load (they just end earlier). */
struct sio1_save_hdr {
	char tag[8];
	u32 size;
	u32 active;
	u64 time;
};

static const char save_tag[8] = "SIO1prt";

int sio1Freeze(void *f, int Mode)
{
	struct sio1_save_hdr hdr;

	if (Mode == 1) {
		memset(&hdr, 0, sizeof(hdr));
		memcpy(hdr.tag, save_tag, sizeof(hdr.tag));
		hdr.size = sizeof(sio1.port.s);
		hdr.active = sio1_active;
		hdr.time = sio1_active ? now64() : 0;
		SaveFuncs.write(f, &hdr, sizeof(hdr));
		SaveFuncs.write(f, &sio1.port.s, sizeof(sio1.port.s));
		return 0;
	}

	memset(&hdr, 0, sizeof(hdr));
	SaveFuncs.read(f, &hdr, sizeof(hdr));
	if (!sio1_active)
		return 0;

	/* this console went somewhere else in time, the other did not */
	if (sio1.link != NULL) {
		sio1_link_break(sio1.link, sio1.time, "a savestate was loaded");
		report_link();
	}
	sio1.cycle = psxRegs.cycle;
	if (memcmp(hdr.tag, save_tag, sizeof(hdr.tag)) == 0
	    && hdr.size == sizeof(sio1.port.s) && hdr.active) {
		int lines = sio1.mode == SIO1_LINK_LOOPBACK;

		SaveFuncs.read(f, &sio1.port.s, sizeof(sio1.port.s));
		sio1.time = hdr.time;
		if (lines)
			lines = sio1.port.s.out_lines;
		if (sio1.mode != SIO1_LINK_LOOPBACK)
			set_inputs(sio1.time, lines);
	}
	else {
		/* a state without the port: as after a reset */
		sio1p_reset(&sio1.port, sio1.time);
	}
	schedule_alone();
	return 0;
}
