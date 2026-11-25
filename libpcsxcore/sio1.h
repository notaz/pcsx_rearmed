/*
 * SIO1 (serial port / link cable) emulation.
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * There are three layers:
 *  - sio1.c: a self-contained port model (struct sio1_port, sio1p_*) that
 *    knows nothing about the rest of the emulator and works on a 64 bit
 *    cycle count supplied by the caller, so that it can be unit tested and
 *    so that two ports can be joined with exact, host independent timing;
 *  - sio1_link.c: joins the ports of two emulator processes on one machine
 *    through shared memory, keeping them in step in emulated time;
 *  - sio1_emu.c: binds one port to psxRegs, the event scheduler and the
 *    savestates (sio1* functions, not declared with SIO1_MODEL_ONLY).
 *
 * Hardware behaviour follows the psx-spx "Serial Interfaces (SIO)" page.
 */

#ifndef __SIO1_H__
#define __SIO1_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIO1_TIME_NEVER		(~(uint64_t)0)
#define SIO1_RXQ_SIZE		64	/* events queued inside the model */

/* SR bits */
#define SIO1_SR_TXRDY		0x0001
#define SIO1_SR_RXRDY		0x0002
#define SIO1_SR_TXU		0x0004
#define SIO1_SR_PERROR		0x0008
#define SIO1_SR_OE		0x0010
#define SIO1_SR_FE		0x0020
#define SIO1_SR_BRK		0x0040
#define SIO1_SR_DSR		0x0080
#define SIO1_SR_CTS		0x0100
#define SIO1_SR_IRQ		0x0200

/* CR bits */
#define SIO1_CR_TXEN		0x0001
#define SIO1_CR_DTR		0x0002
#define SIO1_CR_RXEN		0x0004
#define SIO1_CR_BRK		0x0008
#define SIO1_CR_ERRRST		0x0010
#define SIO1_CR_RTS		0x0020
#define SIO1_CR_INTRST		0x0040
#define SIO1_CR_UNK7		0x0080
#define SIO1_CR_BUFSZ		0x0300
#define SIO1_CR_TXIEN		0x0400
#define SIO1_CR_RXIEN		0x0800
#define SIO1_CR_DSRIEN		0x1000

enum sio1_ev_type {
	SIO1_EV_BYTE = 1,	/* a character; .cycle = end of its stop bit(s) */
	SIO1_EV_LINES,		/* output levels changed at .cycle */
};

/* output line levels carried in sio1_event.lines */
#define SIO1_LINE_DTR		1	/* -> peer's DSR */
#define SIO1_LINE_RTS		2	/* -> peer's CTS */
#define SIO1_LINE_BRK		4	/* TX level inverted */

/* What travels over the cable. Kept at 16 bytes, it is also the layout
 * used by the shared memory link and by trace files. */
struct sio1_event {
	uint64_t cycle;
	uint32_t bit_cycles;	/* BYTE: sender's bit time in cycles */
	uint8_t  type;
	uint8_t  data;		/* BYTE: the character, masked to its length */
	uint8_t  lines;		/* sender's output levels */
	uint8_t  fmt;		/* BYTE: sender's MR low byte */
};

/* Plain data, saved as is in savestates. */
struct sio1_state {
	uint16_t mode, ctrl, baud, misc;
	uint16_t sticky;	/* SR bits 3,4,5,6,9 */
	uint8_t  tx_hold;
	uint8_t  tx_hold_valid;
	uint8_t  tx_hold_txen;	/* TXEN latched by the DR write */
	uint8_t  tx_state;	/* 0 idle, 1 start bit, 2 shifting */
	uint8_t  tx_fmt;
	uint8_t  rx_count;
	uint8_t  rx_fifo[8];
	uint8_t  peer_lines;	/* peer's output levels as seen at .now */
	uint8_t  out_lines;	/* levels we last put on the cable */
	uint8_t  pad[2];
	uint32_t tx_bit_cycles;
	uint32_t rxq_r, rxq_w;
	uint32_t pad2;
	uint64_t tx_start, tx_latch, tx_end;
	uint64_t now;		/* time the model was last advanced to */
	uint64_t baud_t0;	/* baud timer reload time */
	struct sio1_event rxq[SIO1_RXQ_SIZE];
};

struct sio1_port {
	struct sio1_state s;
	/* character or line change leaving the port */
	void (*emit)(void *ctx, const struct sio1_event *ev);
	/* rising edge of SR.9, to be forwarded to I_STAT bit 8 */
	void (*irq)(void *ctx);
	void *ctx;
	/* time of what is being processed, valid inside the callbacks */
	uint64_t t_event;
	/* statistics, not saved */
	uint32_t n_tx, n_rx, n_overrun, n_dropped;
};

void     sio1p_init(struct sio1_port *p);
void     sio1p_reset(struct sio1_port *p, uint64_t now);
/* process everything (internal and queued incoming) stamped <= now */
void     sio1p_advance(struct sio1_port *p, uint64_t now);
/* ofs is the register offset from 0x1f801050 (0..15), width is 1, 2 or 4 */
uint32_t sio1p_read(struct sio1_port *p, uint64_t now, uint32_t ofs, int width);
void     sio1p_write(struct sio1_port *p, uint64_t now, uint32_t ofs,
		uint32_t value, int width);
/* queue an event from the peer, in any order of time. 0 if queued */
int      sio1p_deliver(struct sio1_port *p, const struct sio1_event *ev);
int      sio1p_rxq_space(const struct sio1_port *p);
/* time of the next thing the model has to do, SIO1_TIME_NEVER if nothing */
uint64_t sio1p_next_event(const struct sio1_port *p);
/* No character that is not emitted yet will be put on the cable (which is
 * when its start bit ends) before this time, unless a register is written.
 * SIO1_TIME_NEVER if none is waiting. For a link that has to tell its peer
 * how far this side's output is complete. peer_known: everything from the
 * peer that is stamped up to here has been delivered. */
uint64_t sio1p_unsent_bound(const struct sio1_port *p, uint64_t peer_known);
/* SR as it would read at s.now, without side effects (debug, tests) */
uint32_t sio1p_peek_stat(const struct sio1_port *p);
uint32_t sio1p_bit_cycles(uint32_t mode, uint32_t baud);
uint32_t sio1p_frame_cycles(uint32_t mode, uint32_t bit_cycles);

#ifndef SIO1_MODEL_ONLY

enum sio1_link_mode {
	SIO1_LINK_OFF = 0,	/* port not emulated (old behaviour) */
	SIO1_LINK_UNPLUGGED,	/* port emulated, nothing connected */
	SIO1_LINK_LOOPBACK,	/* TX wired to own RX, for tests */
	SIO1_LINK_CONSOLE_A,	/* linked to another instance on this machine */
	SIO1_LINK_CONSOLE_B,
};

/* called by the frontend, takes effect at once; name selects the link
 * when more than one pair runs on the same machine (may be NULL).
 * Returns 0, or -1 if the link could not be set up (the port is then
 * left unplugged and the reason is logged). */
int  sio1SetLinkMode(enum sio1_link_mode mode, const char *name);
enum sio1_link_mode sio1GetLinkMode(void);
/* Cycles a line change takes to be seen by the other console, for the
 * next link that is set up. 0 is what a cable does, something above the
 * link's grid step lets the two emulators run in parallel instead of
 * taking turns. See sio1_link.c. */
void sio1SetLinkTiming(unsigned int line_delay);
/* nonzero when 0x1f801050..0x1f80105f belong to the model */
extern int sio1_active;

void sio1Reset(void);
void sio1Shutdown(void);
void sio1Interrupt(void);
int  sio1Freeze(void *f, int Mode);
/* for psxhw.c / pcsxmem.c; add is the full address or its low 16 bits */
unsigned int sio1HwRead8(unsigned int add);
unsigned int sio1HwRead16(unsigned int add);
unsigned int sio1HwRead32(unsigned int add);
void sio1HwWrite8(unsigned int add, unsigned int value);
void sio1HwWrite16(unsigned int add, unsigned int value);
void sio1HwWrite32(unsigned int add, unsigned int value);

#endif /* !SIO1_MODEL_ONLY */

#ifdef __cplusplus
}
#endif
#endif /* __SIO1_H__ */
