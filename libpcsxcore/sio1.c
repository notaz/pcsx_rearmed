/*
 * SIO1 (serial port / link cable) emulation.
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * This file is the port model only: it has no dependencies on the rest of
 * the emulator (see sio1.h, sio1_emu.c binds it to psxRegs). Register
 * behaviour follows the psx-spx "Serial Interfaces (SIO)" page; where that
 * page is silent the choice made here is marked with "assumption".
 */

#include <string.h>
#include "sio1.h"

static const uint8_t factor_tab[4] = { 0, 1, 16, 64 };

/* cycles per bit, 0 if the baud rate generator is stopped */
uint32_t sio1p_bit_cycles(uint32_t mode, uint32_t baud)
{
	uint32_t factor = factor_tab[mode & 3];
	uint32_t v;

	if (factor == 0)
		return 0;
	v = ((baud & 0xffff) * factor) & ~1u;
	return v > factor ? v : factor;
}

/* start bit + character + parity + stop bit(s) */
uint32_t sio1p_frame_cycles(uint32_t mode, uint32_t bit_cycles)
{
	uint32_t bits = 1 + 5 + ((mode >> 2) & 3) + ((mode >> 4) & 1);
	uint32_t sb = (mode >> 6) & 3;
	uint32_t stop_halves = sb == 3 ? 4 : sb == 2 ? 3 : 2;

	return bit_cycles * bits + bit_cycles * stop_halves / 2;
}

static uint32_t char_mask(uint32_t mode)
{
	return 0xffu >> (3 - ((mode >> 2) & 3));
}

static int tx_ready(const struct sio1_state *s)
{
	return !s->tx_hold_valid;
}

static int tx_idle(const struct sio1_state *s)
{
	return !s->tx_hold_valid && s->tx_state == 0;
}

static void update_irq(struct sio1_port *p)
{
	struct sio1_state *s = &p->s;
	int cond = 0;

	if ((s->ctrl & SIO1_CR_TXIEN) && (tx_ready(s) || tx_idle(s)))
		cond = 1;
	if ((s->ctrl & SIO1_CR_RXIEN)
	    && s->rx_count >= (1u << ((s->ctrl >> 8) & 3)))
		cond = 1;
	if ((s->ctrl & SIO1_CR_DSRIEN) && (s->peer_lines & SIO1_LINE_DTR))
		cond = 1;
	if (cond && !(s->sticky & SIO1_SR_IRQ)) {
		s->sticky |= SIO1_SR_IRQ;
		if (p->irq)
			p->irq(p->ctx);
	}
}

static void emit(struct sio1_port *p, const struct sio1_event *ev)
{
	if (p->emit)
		p->emit(p->ctx, ev);
}

/* "Writing to this register starts a transfer (if, or as soon as, TXEN=1
 * and CTS=on and SR.2=Ready)", TXEN being the current or the latched one */
static void tx_try_start(struct sio1_port *p, uint64_t t)
{
	struct sio1_state *s = &p->s;
	uint32_t bit;

	if (s->tx_state != 0 || !s->tx_hold_valid)
		return;
	if (!(s->ctrl & SIO1_CR_TXEN) && !s->tx_hold_txen)
		return;
	if (!(s->peer_lines & SIO1_LINE_RTS))
		return;
	bit = sio1p_bit_cycles(s->mode, s->baud);
	if (bit == 0)
		return;

	s->tx_state = 1;
	s->tx_fmt = s->mode;
	s->tx_bit_cycles = bit;
	s->tx_start = t;
	/* SR.0 "gets set after sending the start bit", and a DR write before
	 * that still replaces the character, so DR is sampled at this point */
	s->tx_latch = t + bit;
	s->tx_end = t + sio1p_frame_cycles(s->mode, bit);
}

static void tx_internal(struct sio1_port *p)
{
	struct sio1_state *s = &p->s;

	if (s->tx_state == 1) {
		struct sio1_event ev;

		p->t_event = s->tx_latch;
		memset(&ev, 0, sizeof(ev));
		ev.type = SIO1_EV_BYTE;
		ev.cycle = s->tx_end;
		ev.bit_cycles = s->tx_bit_cycles;
		ev.data = s->tx_hold & char_mask(s->tx_fmt);
		ev.lines = s->out_lines;
		ev.fmt = s->tx_fmt;

		s->tx_hold_valid = 0;
		s->tx_hold_txen = 0;
		s->tx_state = 2;
		p->n_tx++;
		emit(p, &ev);
	}
	else {
		uint64_t t = s->tx_end;

		p->t_event = t;
		s->tx_state = 0;
		tx_try_start(p, t);
	}
	update_irq(p);
}

static void rx_clear(struct sio1_state *s)
{
	memset(s->rx_fifo, 0, sizeof(s->rx_fifo));
	s->rx_count = 0;
}

/* "the hardware stores incoming data in ALL unused FIFO entries" */
static void rx_push(struct sio1_port *p, uint8_t data)
{
	struct sio1_state *s = &p->s;

	if (s->rx_count >= 8) {
		s->rx_fifo[7] = data;
		s->sticky |= SIO1_SR_OE;
		p->n_overrun++;
		return;
	}
	memset(s->rx_fifo + s->rx_count, data, 8 - s->rx_count);
	s->rx_count++;
}

static void rx_pop(struct sio1_state *s, int n)
{
	for (; n > 0; n--) {
		memmove(s->rx_fifo, s->rx_fifo + 1, 7);
		s->rx_fifo[7] = 0;
		if (s->rx_count > 0)
			s->rx_count--;
	}
}

/* assumption: a UART tolerates a few percent of bit rate mismatch
 * (Wipeout 2097 depends on some tolerance); beyond 3% the stop bit is
 * sampled in the wrong place */
static int rate_matches(uint32_t theirs, uint32_t ours)
{
	uint64_t d = theirs > ours ? theirs - ours : ours - theirs;

	return d * 100 <= (uint64_t)ours * 3;
}

static void rx_byte(struct sio1_port *p, const struct sio1_event *ev)
{
	struct sio1_state *s = &p->s;
	uint32_t bit = sio1p_bit_cycles(s->mode, s->baud);
	uint32_t diff = ev->fmt ^ s->mode;

	if (!(s->ctrl & SIO1_CR_RXEN) || bit == 0) {
		p->n_dropped++;
		return;
	}
	/* "The hardware does always store incoming data in RX FIFO (even
	 * when Parity or Stop bits are invalid)" */
	if (!rate_matches(ev->bit_cycles, bit) || (diff & 0x1c))
		s->sticky |= SIO1_SR_FE; /* length or parity bit count differs */
	else if ((s->mode & 0x10) && (diff & 0x20))
		s->sticky |= SIO1_SR_PERROR;
	/* SR.6: RX level "only AFTER receiving Stop Bit" */
	if (ev->lines & SIO1_LINE_BRK)
		s->sticky |= SIO1_SR_BRK | SIO1_SR_FE;
	else
		s->sticky &= ~SIO1_SR_BRK;

	rx_push(p, ev->data & char_mask(s->mode));
	p->n_rx++;
}

static void rx_event(struct sio1_port *p, const struct sio1_event *ev)
{
	struct sio1_state *s = &p->s;

	p->t_event = ev->cycle;
	switch (ev->type) {
	case SIO1_EV_BYTE:
		rx_byte(p, ev);
		break;
	case SIO1_EV_LINES:
		s->peer_lines = ev->lines;
		tx_try_start(p, ev->cycle);
		break;
	}
	update_irq(p);
}

static uint64_t next_internal(const struct sio1_state *s)
{
	if (s->tx_state == 1)
		return s->tx_latch;
	if (s->tx_state == 2)
		return s->tx_end;
	return SIO1_TIME_NEVER;
}

uint64_t sio1p_next_event(const struct sio1_port *p)
{
	const struct sio1_state *s = &p->s;
	uint64_t t = next_internal(s);

	if (s->rxq_r != s->rxq_w) {
		uint64_t ti = s->rxq[s->rxq_r % SIO1_RXQ_SIZE].cycle;
		if (ti < t)
			t = ti;
	}
	return t;
}

int sio1p_rxq_space(const struct sio1_port *p)
{
	return SIO1_RXQ_SIZE - (int)(p->s.rxq_w - p->s.rxq_r);
}

/* The queue is kept sorted by time. A character is put on the cable long
 * before it arrives, so a line change sent after it can be due first. */
int sio1p_deliver(struct sio1_port *p, const struct sio1_event *ev)
{
	struct sio1_state *s = &p->s;
	uint32_t i;

	if (s->rxq_w - s->rxq_r >= SIO1_RXQ_SIZE)
		return -1;
	for (i = s->rxq_w; i != s->rxq_r; i--) {
		struct sio1_event *prev = &s->rxq[(i - 1) % SIO1_RXQ_SIZE];

		if (prev->cycle <= ev->cycle)
			break;
		s->rxq[i % SIO1_RXQ_SIZE] = *prev;
	}
	s->rxq[i % SIO1_RXQ_SIZE] = *ev;
	s->rxq_w++;
	return 0;
}

void sio1p_advance(struct sio1_port *p, uint64_t now)
{
	struct sio1_state *s = &p->s;

	for (;;) {
		uint64_t t_int = next_internal(s);
		uint64_t t_in = SIO1_TIME_NEVER;

		if (s->rxq_r != s->rxq_w)
			t_in = s->rxq[s->rxq_r % SIO1_RXQ_SIZE].cycle;
		if (t_int > now && t_in > now)
			break;
		if (t_int <= t_in)
			tx_internal(p);
		else {
			struct sio1_event ev = s->rxq[s->rxq_r % SIO1_RXQ_SIZE];
			s->rxq_r++;
			rx_event(p, &ev);
		}
	}
	if (now > s->now)
		s->now = now;
}

static void set_out_lines(struct sio1_port *p, uint64_t t)
{
	struct sio1_state *s = &p->s;
	struct sio1_event ev;
	uint8_t lines = 0;

	if (s->ctrl & SIO1_CR_DTR)
		lines |= SIO1_LINE_DTR;
	if (s->ctrl & SIO1_CR_RTS)
		lines |= SIO1_LINE_RTS;
	if (s->ctrl & SIO1_CR_BRK)
		lines |= SIO1_LINE_BRK;
	if (lines == s->out_lines)
		return;
	s->out_lines = lines;

	memset(&ev, 0, sizeof(ev));
	ev.type = SIO1_EV_LINES;
	ev.cycle = t;
	ev.lines = lines;
	emit(p, &ev);
}

static void write_data(struct sio1_port *p, uint64_t t, uint32_t v)
{
	struct sio1_state *s = &p->s;

	/* overwrites a character that is still waiting ("while SR.0=Busy") */
	s->tx_hold = v;
	s->tx_hold_valid = 1;
	s->tx_hold_txen = !!(s->ctrl & SIO1_CR_TXEN);
	tx_try_start(p, t);
}

static void write_mode(struct sio1_port *p, uint64_t t, uint32_t v)
{
	struct sio1_state *s = &p->s;

	s->mode = v & 0xff; /* "bit 8 on SIO1 [is] always zero" */
	if (!(s->mode & 3))
		s->ctrl &= ~SIO1_CR_UNK7;
	tx_try_start(p, t);
}

static void write_baud(struct sio1_port *p, uint64_t t, uint32_t v)
{
	struct sio1_state *s = &p->s;

	s->baud = v;
	s->baud_t0 = t;
}

static void write_ctrl(struct sio1_port *p, uint64_t t, uint32_t v)
{
	struct sio1_state *s = &p->s;

	v &= 0x1fff;
	if (!(s->mode & 3))
		v &= ~SIO1_CR_UNK7; /* "read/write-able when FACTOR non-zero" */
	if (v & SIO1_CR_ERRRST)
		s->sticky &= ~(SIO1_SR_PERROR | SIO1_SR_OE | SIO1_SR_FE
				| SIO1_SR_IRQ);
	if (v & SIO1_CR_INTRST) {
		/* "Reset most registers to zero" */
		s->mode = s->baud = 0;
		s->sticky = 0;
		s->tx_hold_valid = s->tx_hold_txen = 0;
		s->tx_state = 0;
		v = 0;
	}
	s->ctrl = v & ~(SIO1_CR_ERRRST | SIO1_CR_INTRST);
	if (!(s->ctrl & SIO1_CR_RXEN))
		rx_clear(s); /* "Disable also clears RXFIFO" */

	set_out_lines(p, t);
	tx_try_start(p, t);
	update_irq(p);
}

/* "After writing a value X to this register, reading returns X ROR 8
 * eventually ANDed with 1F1Fh and ORed with C0C0h or 8080h (depending on
 * the character length in MR)"; per-length masks as in PCSX-Redux */
static void write_misc(struct sio1_state *s, uint32_t v)
{
	uint32_t m = char_mask(s->mode);
	uint32_t o = m == 0x1f ? 0xc0 : m == 0x3f ? 0x80 : 0;
	uint32_t lo = (v & m) | o, hi = ((v >> 8) & m) | o;

	s->misc = (lo << 8) | hi;
}

static uint32_t read_stat(const struct sio1_state *s, uint64_t t)
{
	uint32_t factor = factor_tab[s->mode & 3];
	uint32_t reload = ((uint32_t)s->baud * factor / 2) & 0x1fffff;
	uint32_t v = s->sticky;

	if (tx_ready(s))
		v |= SIO1_SR_TXRDY;
	if (s->rx_count)
		v |= SIO1_SR_RXRDY;
	if (tx_idle(s))
		v |= SIO1_SR_TXU;
	if (s->peer_lines & SIO1_LINE_DTR)
		v |= SIO1_SR_DSR;
	if (s->peer_lines & SIO1_LINE_RTS)
		v |= SIO1_SR_CTS;
	/* assumption: a 21 bit down counter reloaded with BR*factor/2 */
	if (reload)
		v |= (reload - (uint32_t)((t - s->baud_t0) % reload)) << 11;
	return v;
}

uint32_t sio1p_peek_stat(const struct sio1_port *p)
{
	return read_stat(&p->s, p->s.now);
}

static uint32_t rx_peek32(const struct sio1_state *s)
{
	return s->rx_fifo[0] | (s->rx_fifo[1] << 8) | (s->rx_fifo[2] << 16)
		| ((uint32_t)s->rx_fifo[3] << 24);
}

static uint32_t reg32(const struct sio1_state *s, uint64_t t, uint32_t base)
{
	switch (base) {
	case 0x0: return rx_peek32(s);
	case 0x4: return read_stat(s, t);
	case 0x8: return s->mode | ((uint32_t)s->ctrl << 16);
	default:  return s->misc | ((uint32_t)s->baud << 16);
	}
}

uint32_t sio1p_read(struct sio1_port *p, uint64_t now, uint32_t ofs, int width)
{
	struct sio1_state *s = &p->s;
	uint32_t v;

	sio1p_advance(p, now);
	p->t_event = now;
	ofs &= 0xf;
	v = reg32(s, now, ofs & ~3u) >> ((ofs & 3) * 8);

	/* "A 16bit read allows to read two FIFO entries at once;
	 * nethertheless, it removes only ONE entry from the FIFO. On the
	 * contrary, a 32bit read DOES remove FOUR entries" */
	if (ofs == 0)
		rx_pop(s, width == 4 ? 4 : 1);

	if (width == 1)
		v &= 0xff;
	else if (width == 2)
		v &= 0xffff;
	return v;
}

void sio1p_write(struct sio1_port *p, uint64_t now, uint32_t ofs,
	uint32_t value, int width)
{
	struct sio1_state *s = &p->s;

	sio1p_advance(p, now);
	p->t_event = now;
	ofs &= 0xf;

	if (width == 1) {
		uint32_t sh = (ofs & 1) * 8;
		uint32_t m = 0xffu << sh;

		value = (value & 0xff) << sh;
		switch (ofs & ~1u) {
		case 0x0: if (ofs == 0) write_data(p, now, value); break;
		case 0x8: write_mode(p, now, (s->mode & ~m) | value); break;
		case 0xa: write_ctrl(p, now, (s->ctrl & ~m) | value); break;
		case 0xc: if (ofs == 0xc) write_misc(s, value); break;
		case 0xe: write_baud(p, now, (s->baud & ~m) | value); break;
		}
	}
	else if (width == 2) {
		value &= 0xffff;
		switch (ofs & ~1u) {
		case 0x0: write_data(p, now, value & 0xff); break;
		case 0x8: write_mode(p, now, value); break;
		case 0xa: write_ctrl(p, now, value); break;
		case 0xc: write_misc(s, value); break;
		case 0xe: write_baud(p, now, value); break;
		}
	}
	else {
		switch (ofs & ~3u) {
		case 0x0:
			write_data(p, now, value & 0xff);
			break;
		case 0x8:
			write_mode(p, now, value & 0xffff);
			write_ctrl(p, now, value >> 16);
			break;
		case 0xc:
			write_misc(s, value & 0xffff);
			write_baud(p, now, value >> 16);
			break;
		}
	}
	/* SR (4..7) is read-only */

	/* a loopback plug may have queued something for right now */
	sio1p_advance(p, now);
}

uint64_t sio1p_unsent_bound(const struct sio1_port *p, uint64_t peer_known)
{
	const struct sio1_state *s = &p->s;
	uint32_t bit = sio1p_bit_cycles(s->mode, s->baud);
	uint32_t i;

	if (s->tx_state == 1)
		return s->tx_latch;
	if (!s->tx_hold_valid)
		return SIO1_TIME_NEVER;
	if (s->tx_state == 2)
		return s->tx_end + bit; /* starts when the shifter is free */
	if (!bit || !((s->ctrl & SIO1_CR_TXEN) || s->tx_hold_txen)
	    || (s->peer_lines & SIO1_LINE_RTS))
		return SIO1_TIME_NEVER; /* needs a register write first */

	/* It waits for CTS and starts the cycle the peer raises RTS: with
	 * a line change that is queued already, or with one to come */
	for (i = s->rxq_r; i != s->rxq_w; i++) {
		const struct sio1_event *ev = &s->rxq[i % SIO1_RXQ_SIZE];

		if (ev->type == SIO1_EV_LINES && ev->cycle <= peer_known)
			return ev->cycle + bit;
	}
	return peer_known + 1 + bit;
}

void sio1p_init(struct sio1_port *p)
{
	memset(p, 0, sizeof(*p));
}

/* console reset: the cable stays plugged in, so the peer's levels and
 * anything it already sent are kept */
void sio1p_reset(struct sio1_port *p, uint64_t now)
{
	struct sio1_state *s = &p->s;

	sio1p_advance(p, now);
	p->t_event = now;
	s->mode = s->ctrl = s->baud = s->misc = 0;
	s->sticky = 0;
	s->tx_hold_valid = s->tx_hold_txen = 0;
	s->tx_state = 0;
	rx_clear(s);
	set_out_lines(p, now);
}
