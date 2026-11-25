/*
 * Conformance tests for the SIO1 port model (libpcsxcore/sio1.c).
 * Each test is one documented behaviour from the psx-spx SIO page; the
 * T-numbers refer to the test matrix in the design notes.
 *
 * Two ports are joined by an ideal cable and stepped in emulated time,
 * no emulator and no host timing involved.
 *
 * build: see Makefile in this directory
 */

#include <stdio.h>
#include <string.h>
#include "../../libpcsxcore/sio1.h"

static struct sio1_port A, B;
static int irq_a, irq_b;
static int fails, checks;
static const char *cur_test;

#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { \
		fails++; \
		printf("  FAIL %s line %d: %s\n", cur_test, __LINE__, #cond); \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	unsigned long long a_ = (a), b_ = (b); \
	checks++; \
	if (a_ != b_) { \
		fails++; \
		printf("  FAIL %s line %d: %s = 0x%llx, expected 0x%llx\n", \
			cur_test, __LINE__, #a, a_, b_); \
	} \
} while (0)

#define TEST(name) cur_test = name; setup()

static void emit_a(void *ctx, const struct sio1_event *ev)
{
	(void)ctx;
	if (sio1p_deliver(&B, ev))
		printf("  B queue overflow\n");
}

static void emit_b(void *ctx, const struct sio1_event *ev)
{
	(void)ctx;
	if (sio1p_deliver(&A, ev))
		printf("  A queue overflow\n");
}

static void on_irq_a(void *ctx) { (void)ctx; irq_a++; }
static void on_irq_b(void *ctx) { (void)ctx; irq_b++; }

/* advance both ports to t in global time order */
static void run_until(uint64_t t)
{
	for (;;) {
		uint64_t na = sio1p_next_event(&A), nb = sio1p_next_event(&B);
		uint64_t n = na < nb ? na : nb;

		if (n > t)
			break;
		sio1p_advance(&A, n);
		sio1p_advance(&B, n);
	}
	sio1p_advance(&A, t);
	sio1p_advance(&B, t);
}

static uint32_t rd(struct sio1_port *p, uint64_t t, uint32_t ofs, int w)
{
	uint32_t v;

	run_until(t);
	v = sio1p_read(p, t, ofs, w);
	run_until(t);
	return v;
}

static void wr(struct sio1_port *p, uint64_t t, uint32_t ofs, uint32_t v, int w)
{
	run_until(t);
	sio1p_write(p, t, ofs, v, w);
	run_until(t);
}

#define DR   0x0
#define SR   0x4
#define MR   0x8
#define CR   0xa
#define MISC 0xc
#define BR   0xe

#define MODE_8N1	0x4e	/* x16, 8 bits, no parity, 1 stop */
#define BAUD_115K	0x12	/* 18 * 16 = 288 cycles per bit */
#define BIT		288u
#define FRAME		2880u
#define CTRL_ON		(SIO1_CR_TXEN | SIO1_CR_DTR | SIO1_CR_RXEN | SIO1_CR_RTS)

static void setup(void)
{
	sio1p_init(&A);
	sio1p_init(&B);
	A.emit = emit_a; A.irq = on_irq_a;
	B.emit = emit_b; B.irq = on_irq_b;
	irq_a = irq_b = 0;
}

/* both ends configured the way libsio would for 115200 8N1 */
static void link_up(uint64_t t)
{
	wr(&A, t, MR, MODE_8N1, 2); wr(&A, t, BR, BAUD_115K, 2);
	wr(&B, t, MR, MODE_8N1, 2); wr(&B, t, BR, BAUD_115K, 2);
	wr(&A, t, CR, CTRL_ON, 2);
	wr(&B, t, CR, CTRL_ON, 2);
}

static void t_registers(void)
{
	TEST("T01 reset");
	wr(&A, 10, MR, MODE_8N1, 2); wr(&A, 10, BR, 0x1234, 2);
	wr(&A, 10, CR, CTRL_ON, 2);
	wr(&A, 20, CR, SIO1_CR_INTRST, 2);
	CHECK_EQ(rd(&A, 20, MR, 2), 0);
	CHECK_EQ(rd(&A, 20, CR, 2), 0);
	CHECK_EQ(rd(&A, 20, BR, 2), 0);
	CHECK_EQ(rd(&A, 20, SR, 2) & 0x3ff, SIO1_SR_TXRDY | SIO1_SR_TXU);

	TEST("T03 SR is read-only");
	wr(&A, 10, SR, 0xffff, 2);
	wr(&A, 10, SR, 0xffffffff, 4);
	wr(&A, 10, SR, 0xff, 1);
	CHECK_EQ(rd(&A, 10, SR, 4), SIO1_SR_TXRDY | SIO1_SR_TXU);

	TEST("T04 0x1f80105c");
	wr(&A, 10, MR, MODE_8N1, 2);
	wr(&A, 10, MISC, 0x1234, 2);
	CHECK_EQ(rd(&A, 10, MISC, 2), 0x3412);
	wr(&A, 10, MR, 0x42, 2); /* 5 bits */
	wr(&A, 10, MISC, 0xffff, 2);
	CHECK_EQ(rd(&A, 10, MISC, 2), 0xdfdf);
	wr(&A, 10, MR, 0x46, 2); /* 6 bits */
	wr(&A, 10, MISC, 0xffff, 2);
	CHECK_EQ(rd(&A, 10, MISC, 2), 0xbfbf);
	wr(&A, 10, MR, 0x4a, 2); /* 7 bits */
	wr(&A, 10, MISC, 0xffff, 2);
	CHECK_EQ(rd(&A, 10, MISC, 2), 0x7f7f);

	TEST("T05 CR bit 7");
	wr(&A, 10, CR, 0x80, 2);
	CHECK_EQ(rd(&A, 10, CR, 2), 0);
	wr(&A, 10, MR, MODE_8N1, 2);
	wr(&A, 10, CR, 0x80, 2);
	CHECK_EQ(rd(&A, 10, CR, 2), 0x80);

	TEST("register access widths");
	wr(&A, 10, MR, 0x4e, 1);
	wr(&A, 10, CR, 0x27, 1);
	wr(&A, 10, CR + 1, 0x03, 1);
	wr(&A, 10, BR, 0x34, 1);
	wr(&A, 10, BR + 1, 0x12, 1);
	CHECK_EQ(rd(&A, 10, MR, 2), 0x4e);
	CHECK_EQ(rd(&A, 10, CR, 2), 0x0327);
	CHECK_EQ(rd(&A, 10, CR + 1, 1), 0x03);
	CHECK_EQ(rd(&A, 10, BR, 2), 0x1234);
	CHECK_EQ(rd(&A, 10, MR, 4), 0x0327004e);
	CHECK_EQ(rd(&A, 10, MISC, 4) >> 16, 0x1234);
	wr(&A, 20, MR, 0x00250042, 4);
	CHECK_EQ(rd(&A, 20, MR, 2), 0x42);
	CHECK_EQ(rd(&A, 20, CR, 2), 0x25);
	wr(&A, 20, MISC, 0x00880000, 4);
	CHECK_EQ(rd(&A, 20, BR, 2), 0x88);
	/* bits that do not exist */
	wr(&A, 30, MR, 0xffff, 2);
	CHECK_EQ(rd(&A, 30, MR, 2), 0xff);
	wr(&A, 30, CR, 0xffff & ~(SIO1_CR_INTRST), 2);
	CHECK_EQ(rd(&A, 30, CR, 2), 0x1fff & ~(SIO1_CR_ERRRST | SIO1_CR_INTRST));
}

static void t_baud(void)
{
	TEST("T06 baud formula");
	CHECK_EQ(sio1p_bit_cycles(0x01, 0), 1);
	CHECK_EQ(sio1p_bit_cycles(0x01, 3), 2);
	CHECK_EQ(sio1p_bit_cycles(0x01, 0x88), 0x88);
	CHECK_EQ(sio1p_bit_cycles(0x02, 0), 16);
	CHECK_EQ(sio1p_bit_cycles(0x02, 0x12), 288);
	CHECK_EQ(sio1p_bit_cycles(0x03, 1), 64);
	CHECK_EQ(sio1p_bit_cycles(0x03, 0xffff), (0xffffu * 64) & ~1u);
	CHECK_EQ(sio1p_bit_cycles(0x00, 0x12), 0);
	/* 33868800 / 288 = 117600 baud, what "115200" really is */
	CHECK_EQ(33868800 / sio1p_bit_cycles(0x02, 0x12), 117600);
	CHECK_EQ(sio1p_frame_cycles(0x4e, 288), 2880);          /* 8N1 */
	CHECK_EQ(sio1p_frame_cycles(0x0e, 288), 2880);          /* SB=0 is 1 */
	CHECK_EQ(sio1p_frame_cycles(0xce, 288), 288 * 11);      /* 8N2 */
	CHECK_EQ(sio1p_frame_cycles(0x8e, 288), 288 * 10 + 144); /* 8N1.5 */
	CHECK_EQ(sio1p_frame_cycles(0x5e, 288), 288 * 11);      /* 8O1 */
	CHECK_EQ(sio1p_frame_cycles(0x42, 288), 288 * 7);       /* 5N1 */

	TEST("T07 baud timer in SR");
	wr(&A, 100, MR, MODE_8N1, 2);
	wr(&A, 1000, BR, BAUD_115K, 2); /* reload = 18 * 16 / 2 = 144 */
	CHECK_EQ(rd(&A, 1000, SR, 4) >> 11, 144);
	CHECK_EQ(rd(&A, 1010, SR, 4) >> 11, 134);
	CHECK_EQ(rd(&A, 1143, SR, 4) >> 11, 1);
	CHECK_EQ(rd(&A, 1144, SR, 4) >> 11, 144);
	wr(&A, 1200, BR, BAUD_115K, 2); /* a write reloads */
	CHECK_EQ(rd(&A, 1200, SR, 4) >> 11, 144);

	TEST("T08 factor 0 stops the port");
	wr(&B, 10, MR, MODE_8N1, 2); wr(&B, 10, BR, BAUD_115K, 2);
	wr(&B, 10, CR, CTRL_ON, 2);
	wr(&A, 10, BR, BAUD_115K, 2);
	wr(&A, 10, CR, CTRL_ON, 2);
	wr(&A, 100, DR, 0x55, 1);
	CHECK_EQ(rd(&A, 100000, SR, 2) & (SIO1_SR_TXRDY | SIO1_SR_TXU), 0);
	CHECK_EQ(rd(&B, 100000, SR, 2) & SIO1_SR_RXRDY, 0);
	wr(&A, 200000, MR, MODE_8N1, 2); /* now it goes */
	CHECK_EQ(rd(&B, 200000 + FRAME - 1, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(rd(&B, 200000 + FRAME, SR, 2) & SIO1_SR_RXRDY, SIO1_SR_RXRDY);
	CHECK_EQ(rd(&B, 200000 + FRAME, DR, 1), 0x55);
}

static void t_tx(void)
{
	const uint32_t F = SIO1_SR_TXRDY | SIO1_SR_TXU;
	int i;

	TEST("T09 TX flags");
	link_up(10);
	CHECK_EQ(rd(&A, 999, SR, 2) & F, F);
	wr(&A, 1000, DR, 0xa5, 1);
	CHECK_EQ(rd(&A, 1000, SR, 2) & F, 0);
	CHECK_EQ(rd(&A, 1000 + BIT - 1, SR, 2) & F, 0);
	CHECK_EQ(rd(&A, 1000 + BIT, SR, 2) & F, SIO1_SR_TXRDY);
	CHECK_EQ(rd(&A, 1000 + FRAME - 1, SR, 2) & F, SIO1_SR_TXRDY);
	CHECK_EQ(rd(&A, 1000 + FRAME, SR, 2) & F, F);
	CHECK_EQ(rd(&B, 1000 + FRAME, DR, 1), 0xa5);

	TEST("T10 two characters back to back");
	link_up(10);
	wr(&A, 1000, DR, 0x11, 1);
	wr(&A, 1000 + BIT, DR, 0x22, 1);
	CHECK_EQ(rd(&A, 1000 + BIT, SR, 2) & F, 0);
	CHECK_EQ(rd(&A, 1000 + FRAME, SR, 2) & F, 0);
	CHECK_EQ(rd(&A, 1000 + FRAME + BIT, SR, 2) & F, SIO1_SR_TXRDY);
	CHECK_EQ(rd(&A, 1000 + 2 * FRAME - 1, SR, 2) & F, SIO1_SR_TXRDY);
	CHECK_EQ(rd(&A, 1000 + 2 * FRAME, SR, 2) & F, F);
	CHECK_EQ(B.s.rx_count, 2);
	CHECK_EQ(rd(&B, 1000 + 2 * FRAME, DR, 1), 0x11);
	CHECK_EQ(rd(&B, 1000 + 2 * FRAME, DR, 1), 0x22);
	CHECK_EQ(A.n_tx, 2);

	TEST("T11 DR write while busy overwrites");
	link_up(10);
	wr(&A, 1000, DR, 0x11, 1);
	wr(&A, 1001, DR, 0x22, 1);
	run_until(1000 + 4 * FRAME);
	CHECK_EQ(B.s.rx_count, 1);
	CHECK_EQ(rd(&B, 1000 + 4 * FRAME, DR, 1), 0x22);
	/* also when the first one could not even start */
	link_up(100000);
	wr(&B, 100000, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	wr(&A, 100100, DR, 0x33, 1);
	wr(&A, 100200, DR, 0x44, 1);
	wr(&B, 100300, CR, CTRL_ON, 2);
	run_until(100300 + 4 * FRAME);
	CHECK_EQ(B.s.rx_count, 1);
	CHECK_EQ(rd(&B, 100300 + 4 * FRAME, DR, 1), 0x44);

	TEST("T12 CTS gates transmission");
	link_up(10);
	wr(&B, 20, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	CHECK_EQ(rd(&A, 20, SR, 2) & SIO1_SR_CTS, 0);
	wr(&A, 1000, DR, 0x77, 1);
	CHECK_EQ(rd(&A, 50000, SR, 2) & F, 0);
	CHECK_EQ(rd(&B, 50000, SR, 2) & SIO1_SR_RXRDY, 0);
	wr(&B, 60000, CR, CTRL_ON, 2);
	CHECK_EQ(rd(&B, 60000 + FRAME - 1, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(rd(&B, 60000 + FRAME, DR, 1), 0x77);
	/* a character already on the wire is not cut short */
	wr(&A, 70000, DR, 0x78, 1);
	wr(&B, 70000 + BIT * 3, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	CHECK_EQ(rd(&B, 70000 + FRAME, DR, 1), 0x78);
	/* but the next one waits */
	wr(&B, 80000, CR, CTRL_ON, 2);
	wr(&A, 81000, DR, 0x79, 1);
	wr(&A, 81000 + BIT, DR, 0x7a, 1);
	wr(&B, 81000 + BIT * 2, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	run_until(81000 + 5 * FRAME);
	CHECK_EQ(B.s.rx_count, 1);
	CHECK_EQ(rd(&A, 81000 + 5 * FRAME, SR, 2) & F, 0);
	wr(&B, 100000, CR, CTRL_ON, 2);
	run_until(100000 + FRAME);
	CHECK_EQ(B.s.rx_count, 2);

	TEST("T13 TXEN is latched by the DR write");
	link_up(10);
	wr(&B, 20, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	wr(&A, 1000, DR, 0x42, 1);
	wr(&A, 1100, CR, CTRL_ON & ~SIO1_CR_TXEN, 2);
	wr(&B, 2000, CR, CTRL_ON, 2);
	CHECK_EQ(rd(&B, 2000 + FRAME, DR, 1), 0x42);
	/* latched off and still off: nothing goes out */
	wr(&A, 10000, DR, 0x43, 1);
	run_until(10000 + 4 * FRAME);
	CHECK_EQ(B.s.rx_count, 0);
	CHECK_EQ(rd(&A, 10000 + 4 * FRAME, SR, 2) & F, 0);
	/* until TXEN is set */
	wr(&A, 30000, CR, CTRL_ON, 2);
	CHECK_EQ(rd(&B, 30000 + FRAME, DR, 1), 0x43);

	TEST("T14 character length");
	for (i = 0; i < 3; i++) {
		static const uint8_t modes[3] = { 0x42, 0x46, 0x4a };
		static const uint8_t masks[3] = { 0x1f, 0x3f, 0x7f };
		uint64_t t = 1000 + i * 100000;

		link_up(t);
		wr(&A, t, MR, modes[i], 2);
		wr(&B, t, MR, modes[i], 2);
		wr(&A, t + 10, DR, 0xff, 1);
		run_until(t + 10 + FRAME);
		CHECK_EQ(rd(&B, t + 10 + FRAME, DR, 1), masks[i]);
		CHECK_EQ(rd(&B, t + 10 + FRAME, SR, 2)
			& (SIO1_SR_FE | SIO1_SR_PERROR), 0);
	}
}

static void send_bytes(uint64_t t, const uint8_t *d, int n)
{
	int i;

	for (i = 0; i < n; i++)
		wr(&A, t + (uint64_t)i * FRAME, DR, d[i], 1);
	run_until(t + (uint64_t)n * FRAME);
}

static void t_rx(void)
{
	static const uint8_t seq[9] = { 0x01, 0x02, 0x03, 0x04, 0x05,
					0x06, 0x07, 0x08, 0x09 };
	uint64_t t;
	int i;

	TEST("T15 RXRDY");
	link_up(10);
	CHECK_EQ(rd(&B, 100, SR, 2) & SIO1_SR_RXRDY, 0);
	send_bytes(1000, seq, 2);
	t = 1000 + 2 * FRAME;
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_RXRDY, SIO1_SR_RXRDY);
	rd(&B, t, DR, 1);
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_RXRDY, SIO1_SR_RXRDY);
	rd(&B, t, DR, 1);
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_RXRDY, 0);

	TEST("T16 T17 read widths and preview");
	link_up(10);
	send_bytes(1000, seq, 6);
	t = 1000 + 6 * FRAME;
	CHECK_EQ(rd(&B, t, DR + 1, 1), 0x02); /* previews do not remove */
	CHECK_EQ(rd(&B, t, DR + 2, 1), 0x03);
	CHECK_EQ(rd(&B, t, DR + 3, 1), 0x04);
	CHECK_EQ(rd(&B, t, DR + 2, 2), 0x0403);
	CHECK_EQ(B.s.rx_count, 6);
	CHECK_EQ(rd(&B, t, DR, 2), 0x0201);   /* two entries, one removed */
	CHECK_EQ(B.s.rx_count, 5);
	CHECK_EQ(rd(&B, t, DR, 4), 0x05040302); /* four removed */
	CHECK_EQ(B.s.rx_count, 1);
	CHECK_EQ(rd(&B, t, DR, 1), 0x06);
	CHECK_EQ(B.s.rx_count, 0);

	TEST("T18 overrun");
	link_up(10);
	send_bytes(1000, seq, 8);
	t = 1000 + 8 * FRAME;
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_OE, 0);
	send_bytes(t, seq + 8, 1);
	t += FRAME;
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_OE, SIO1_SR_OE);
	CHECK_EQ(B.s.rx_count, 8);
	/* "does NOT automatically disable RTS" */
	CHECK_EQ(rd(&B, t, CR, 2) & SIO1_CR_RTS, SIO1_CR_RTS);
	CHECK_EQ(rd(&A, t, SR, 2) & SIO1_SR_CTS, SIO1_SR_CTS);
	for (i = 0; i < 7; i++)
		CHECK_EQ(rd(&B, t, DR, 1), seq[i]);
	CHECK_EQ(rd(&B, t, DR, 1), 0x09); /* last entry was overwritten */

	cur_test = "T02 acknowledge clears error bits"; /* same ports */
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_OE, SIO1_SR_OE);
	wr(&B, t, CR, CTRL_ON | SIO1_CR_ERRRST, 2);
	CHECK_EQ(rd(&B, t, SR, 2)
		& (SIO1_SR_OE | SIO1_SR_FE | SIO1_SR_PERROR | SIO1_SR_IRQ), 0);
	CHECK_EQ(rd(&B, t, CR, 2), CTRL_ON); /* the strobe does not stick */

	TEST("T19 reading an empty FIFO");
	/* "if five entries are used, then the data gets stored thrice,
	 * after reading 6 bytes, the FIFO empty flag gets set, but
	 * nethertheless, the last byte can be read two more times, but
	 * doing further reads returns 00h" */
	link_up(10);
	send_bytes(1000, seq, 6);
	t = 1000 + 6 * FRAME;
	for (i = 0; i < 6; i++)
		CHECK_EQ(rd(&B, t, DR, 1), seq[i]);
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(rd(&B, t, DR, 1), 0x06);
	CHECK_EQ(rd(&B, t, DR, 1), 0x06);
	CHECK_EQ(rd(&B, t, DR, 1), 0x00);
	CHECK_EQ(rd(&B, t, DR, 1), 0x00);

	TEST("T20 clearing RXEN empties the FIFO");
	link_up(10);
	send_bytes(1000, seq, 3);
	t = 1000 + 3 * FRAME;
	CHECK_EQ(B.s.rx_count, 3);
	wr(&B, t, CR, CTRL_ON & ~SIO1_CR_RXEN, 2);
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(rd(&B, t, DR, 1), 0);
	send_bytes(t + 100, seq, 1); /* and nothing is received while off */
	CHECK_EQ(rd(&B, t + 100 + FRAME, SR, 2) & SIO1_SR_RXRDY, 0);

	TEST("T21 parity and framing errors still store the character");
	link_up(10);
	wr(&A, 20, MR, 0x7e, 2); /* 8E1 */
	wr(&B, 20, MR, 0x5e, 2); /* 8O1 */
	wr(&A, 1000, DR, 0x5a, 1);
	t = 1000 + 11 * BIT;
	CHECK_EQ(rd(&B, t, SR, 2) & (SIO1_SR_PERROR | SIO1_SR_FE),
		SIO1_SR_PERROR);
	CHECK_EQ(rd(&B, t, DR, 1), 0x5a);
	setup();
	link_up(100000);
	wr(&A, 100000, MR, 0x4a, 2); /* 7N1 into 8N1 */
	wr(&A, 101000, DR, 0x5a, 1);
	t = 101000 + FRAME;
	CHECK_EQ(rd(&B, t, SR, 2) & (SIO1_SR_PERROR | SIO1_SR_FE), SIO1_SR_FE);
	CHECK_EQ(B.s.rx_count, 1);
	/* break level */
	setup();
	link_up(200000);
	wr(&A, 200000, CR, CTRL_ON | SIO1_CR_BRK, 2);
	wr(&A, 201000, DR, 0x00, 1);
	t = 201000 + FRAME;
	CHECK_EQ(rd(&B, t, SR, 2) & (SIO1_SR_BRK | SIO1_SR_FE),
		SIO1_SR_BRK | SIO1_SR_FE);

	TEST("T29 baud rate tolerance");
	link_up(10);
	wr(&A, 20, MR, 0x4d, 2); wr(&A, 20, BR, 294, 2); /* x1: 2% off */
	wr(&A, 1000, DR, 0x33, 1);
	t = 1000 + 2940;
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_FE, 0);
	CHECK_EQ(rd(&B, t, DR, 1), 0x33);
	wr(&A, 10000, BR, 320, 2); /* 11% off */
	wr(&A, 20000, DR, 0x33, 1);
	t = 20000 + 3200;
	CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_FE, SIO1_SR_FE);
}

static void t_lines(void)
{
	TEST("T22 handshake lines are crossed");
	wr(&A, 10, MR, MODE_8N1, 2);
	wr(&B, 10, MR, MODE_8N1, 2);
	CHECK_EQ(rd(&B, 10, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), 0);
	wr(&A, 20, CR, SIO1_CR_DTR, 2);
	CHECK_EQ(rd(&B, 20, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), SIO1_SR_DSR);
	wr(&A, 30, CR, SIO1_CR_RTS, 2);
	CHECK_EQ(rd(&B, 30, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), SIO1_SR_CTS);
	CHECK_EQ(rd(&A, 30, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), 0);
	wr(&B, 40, CR, SIO1_CR_DTR | SIO1_CR_RTS, 2);
	CHECK_EQ(rd(&A, 40, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS),
		SIO1_SR_DSR | SIO1_SR_CTS);
	/* a console reset drops its outputs, the cable stays in */
	sio1p_reset(&B, 50);
	run_until(50);
	CHECK_EQ(rd(&A, 50, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), 0);
	CHECK_EQ(rd(&B, 50, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), SIO1_SR_CTS);
}

static void t_irq(void)
{
	static const uint8_t seq[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint64_t t;
	int bufsz, i;

	TEST("T23 RX interrupt thresholds");
	for (bufsz = 0; bufsz < 4; bufsz++) {
		int n = 1 << bufsz;

		setup();
		t = 1000;
		link_up(10);
		wr(&B, 20, CR, CTRL_ON | SIO1_CR_RXIEN | (bufsz << 8), 2);
		for (i = 1; i <= n; i++) {
			wr(&A, t, DR, seq[i - 1], 1);
			run_until(t + FRAME - 1);
			CHECK_EQ(irq_b, 0);
			run_until(t + FRAME);
			CHECK_EQ(irq_b, i == n ? 1 : 0);
			t += FRAME;
		}
		CHECK_EQ(rd(&B, t, SR, 2) & SIO1_SR_IRQ, SIO1_SR_IRQ);
	}

	TEST("T24 TX interrupt with an idle transmitter");
	/* observed on SCPH-1001 and SCPH-5501 (PCSX-Redux sio1-txirq test):
	 * raised without any byte written and with TXEN clear */
	wr(&A, 10, CR, SIO1_CR_TXIEN, 2);
	CHECK_EQ(irq_a, 1);
	CHECK_EQ(rd(&A, 10, SR, 2) & SIO1_SR_IRQ, SIO1_SR_IRQ);

	TEST("T25 acknowledge with the condition still true");
	wr(&A, 10, CR, SIO1_CR_TXIEN, 2);
	CHECK_EQ(irq_a, 1);
	wr(&A, 20, CR, SIO1_CR_TXIEN | SIO1_CR_ERRRST, 2);
	CHECK_EQ(irq_a, 2);
	CHECK_EQ(rd(&A, 20, SR, 2) & SIO1_SR_IRQ, SIO1_SR_IRQ);
	wr(&A, 30, CR, SIO1_CR_ERRRST, 2); /* enable dropped: it goes away */
	CHECK_EQ(irq_a, 2);
	CHECK_EQ(rd(&A, 30, SR, 2) & SIO1_SR_IRQ, 0);

	TEST("TX interrupt per character");
	link_up(10);
	wr(&A, 1000, DR, 0x10, 1);
	wr(&A, 1001, CR, CTRL_ON | SIO1_CR_TXIEN, 2);
	CHECK_EQ(irq_a, 0); /* busy: neither SR.0 nor SR.2 */
	run_until(1000 + BIT - 1);
	CHECK_EQ(irq_a, 0);
	run_until(1000 + BIT);
	CHECK_EQ(irq_a, 1); /* start bit out, DR free again */

	TEST("T26 DSR interrupt");
	link_up(10);
	wr(&A, 20, CR, CTRL_ON & ~SIO1_CR_DTR, 2);
	wr(&B, 30, CR, CTRL_ON | SIO1_CR_DSRIEN, 2);
	CHECK_EQ(irq_b, 0);
	wr(&A, 5000, CR, CTRL_ON, 2);
	CHECK_EQ(irq_b, 1);
	CHECK_EQ(rd(&B, 5000, SR, 2) & (SIO1_SR_IRQ | SIO1_SR_DSR),
		SIO1_SR_IRQ | SIO1_SR_DSR);

	TEST("T27 no interrupt is lost around an acknowledge");
	link_up(10);
	wr(&B, 20, CR, CTRL_ON | SIO1_CR_RXIEN, 2);
	wr(&A, 1000, DR, 0x01, 1);
	wr(&A, 1000 + FRAME, DR, 0x02, 1);
	run_until(1000 + FRAME);
	CHECK_EQ(irq_b, 1);
	/* handler acknowledges before draining: fires again at once */
	wr(&B, 1000 + FRAME + 50, CR, CTRL_ON | SIO1_CR_RXIEN | SIO1_CR_ERRRST, 2);
	CHECK_EQ(irq_b, 2);
	rd(&B, 1000 + FRAME + 60, DR, 1);
	wr(&B, 1000 + FRAME + 70, CR, CTRL_ON | SIO1_CR_RXIEN | SIO1_CR_ERRRST, 2);
	CHECK_EQ(irq_b, 2); /* drained: stays quiet */
	run_until(1000 + 2 * FRAME);
	CHECK_EQ(irq_b, 3); /* the next character is not missed */
}

static void t_timing(void)
{
	static const struct { uint8_t mode; uint16_t baud; } cfg[] = {
		{ 0x4e, 0x12 },   /* 117600 8N1 */
		{ 0x4e, 0x24 },   /* 58800 8N1 */
		{ 0x4d, 0x126 },  /* x1 */
		{ 0x4f, 0x02 },   /* x64 */
		{ 0xce, 0x12 },   /* 2 stop bits */
		{ 0x8e, 0x12 },   /* 1.5 stop bits */
		{ 0x7e, 0x12 },   /* even parity */
		{ 0x42, 0x12 },   /* 5 bits */
		{ 0x4e, 0x00 },   /* fastest x16 */
	};
	unsigned i;

	TEST("T28 arrival time");
	for (i = 0; i < sizeof(cfg) / sizeof(cfg[0]); i++) {
		uint32_t bit = sio1p_bit_cycles(cfg[i].mode, cfg[i].baud);
		uint32_t frame = sio1p_frame_cycles(cfg[i].mode, bit);
		uint64_t t = 100000 + i * 1000;

		setup();
		wr(&A, 10, MR, cfg[i].mode, 2); wr(&A, 10, BR, cfg[i].baud, 2);
		wr(&B, 10, MR, cfg[i].mode, 2); wr(&B, 10, BR, cfg[i].baud, 2);
		wr(&A, 10, CR, CTRL_ON, 2); wr(&B, 10, CR, CTRL_ON, 2);
		wr(&A, t, DR, 0x15, 1);
		CHECK_EQ(rd(&B, t + frame - 1, SR, 2) & SIO1_SR_RXRDY, 0);
		CHECK_EQ(rd(&B, t + frame, SR, 2) & SIO1_SR_RXRDY, SIO1_SR_RXRDY);
		CHECK_EQ(rd(&B, t + frame, SR, 2) & (SIO1_SR_FE | SIO1_SR_PERROR), 0);
		CHECK_EQ(rd(&B, t + frame, DR, 1), 0x15);
	}

	TEST("bound on when a waiting character can be put on the cable");
	link_up(10);
	run_until(5000);
	CHECK(sio1p_unsent_bound(&A, 5000) == SIO1_TIME_NEVER);
	wr(&A, 5000, DR, 0x01, 1);
	CHECK_EQ(sio1p_unsent_bound(&A, 5000), 5000 + BIT); /* in its start bit */
	run_until(5000 + BIT);
	CHECK(sio1p_unsent_bound(&A, 5000 + BIT) == SIO1_TIME_NEVER); /* out */
	wr(&A, 5000 + BIT, DR, 0x02, 1);
	/* waits for the first one to finish */
	CHECK_EQ(sio1p_unsent_bound(&A, 5000 + BIT), 5000 + FRAME + BIT);
	run_until(5000 + FRAME);
	CHECK_EQ(sio1p_unsent_bound(&A, 5000 + FRAME), 5000 + FRAME + BIT);
	run_until(5000 + 3 * FRAME);
	CHECK(sio1p_unsent_bound(&A, 5000 + 3 * FRAME) == SIO1_TIME_NEVER);
	/* waiting for CTS: one bit time after the peer raises RTS, which
	 * it has not done as far as it is known */
	wr(&B, 100000, CR, CTRL_ON & ~SIO1_CR_RTS, 2);
	wr(&A, 100100, DR, 0x03, 1);
	run_until(100500);
	CHECK_EQ(sio1p_unsent_bound(&A, 100500), 100500 + 1 + BIT);
	CHECK_EQ(sio1p_unsent_bound(&A, 100900), 100900 + 1 + BIT);
	{
		/* ... or does with a line change that is on its way */
		struct sio1_event ev;

		memset(&ev, 0, sizeof(ev));
		ev.type = SIO1_EV_LINES;
		ev.cycle = 100700;
		ev.lines = SIO1_LINE_DTR | SIO1_LINE_RTS;
		sio1p_deliver(&A, &ev);
		CHECK_EQ(sio1p_unsent_bound(&A, 100900), 100700 + BIT);
		CHECK_EQ(sio1p_unsent_bound(&A, 100600), 100600 + 1 + BIT);
		run_until(100700 + BIT - 1);
		CHECK_EQ(sio1p_unsent_bound(&A, 100900), 100700 + BIT);
		/* and it goes out then */
		i = B.s.rx_count;
		run_until(100700 + FRAME - 1);
		CHECK_EQ(B.s.rx_count, i);
		run_until(100700 + FRAME);
		CHECK_EQ(B.s.rx_count, i + 1);
	}
	/* waiting for TXEN: needs a register write, so no bound */
	setup();
	link_up(10);
	wr(&A, 300000, CR, CTRL_ON & ~SIO1_CR_TXEN, 2);
	wr(&A, 300100, DR, 0x04, 1);
	CHECK(sio1p_unsent_bound(&A, 300100) == SIO1_TIME_NEVER);
}

/* a loopback plug: TX to RX, DTR to DSR, RTS to CTS */
static void emit_loop(void *ctx, const struct sio1_event *ev)
{
	sio1p_deliver(ctx, ev);
}

static void t_loopback(void)
{
	TEST("loopback plug");
	A.emit = emit_loop; A.ctx = &A;
	wr(&A, 10, MR, MODE_8N1, 2); wr(&A, 10, BR, BAUD_115K, 2);
	CHECK_EQ(sio1p_read(&A, 10, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS), 0);
	sio1p_write(&A, 20, CR, CTRL_ON, 2);
	CHECK_EQ(sio1p_read(&A, 20, SR, 2) & (SIO1_SR_DSR | SIO1_SR_CTS),
		SIO1_SR_DSR | SIO1_SR_CTS);
	sio1p_write(&A, 1000, DR, 0xc3, 1);
	CHECK_EQ(sio1p_read(&A, 1000 + FRAME - 1, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(sio1p_read(&A, 1000 + FRAME, SR, 2) & SIO1_SR_RXRDY,
		SIO1_SR_RXRDY);
	CHECK_EQ(sio1p_read(&A, 1000 + FRAME, DR, 1), 0xc3);
}

static void t_queue(void)
{
	struct sio1_event ev;
	int i;

	TEST("events are processed in time order, not arrival order");
	link_up(10);
	memset(&ev, 0, sizeof(ev));
	/* a character due at 5000, then a line change due at 3000 */
	ev.type = SIO1_EV_BYTE; ev.cycle = 5000; ev.data = 0x99;
	ev.bit_cycles = BIT; ev.fmt = MODE_8N1;
	ev.lines = SIO1_LINE_DTR | SIO1_LINE_RTS;
	CHECK_EQ(sio1p_deliver(&A, &ev), 0);
	ev.type = SIO1_EV_LINES; ev.cycle = 3000; ev.lines = SIO1_LINE_RTS;
	CHECK_EQ(sio1p_deliver(&A, &ev), 0);
	CHECK_EQ(sio1p_next_event(&A), 3000);
	CHECK_EQ(sio1p_read(&A, 2999, SR, 2) & SIO1_SR_DSR, SIO1_SR_DSR);
	CHECK_EQ(sio1p_read(&A, 3000, SR, 2) & SIO1_SR_DSR, 0);
	CHECK_EQ(sio1p_read(&A, 4999, SR, 2) & SIO1_SR_RXRDY, 0);
	CHECK_EQ(sio1p_read(&A, 5000, DR, 1), 0x99);
	/* equal times keep their order; a full queue refuses */
	setup();
	memset(&ev, 0, sizeof(ev));
	ev.type = SIO1_EV_LINES; ev.cycle = 100;
	for (i = 0; i < SIO1_RXQ_SIZE; i++) {
		ev.lines = i & 1 ? SIO1_LINE_DTR : 0;
		CHECK_EQ(sio1p_deliver(&A, &ev), 0);
	}
	CHECK_EQ(sio1p_rxq_space(&A), 0);
	CHECK(sio1p_deliver(&A, &ev) != 0);
	CHECK_EQ(sio1p_read(&A, 100, SR, 2) & SIO1_SR_DSR, SIO1_SR_DSR);
	CHECK_EQ(sio1p_rxq_space(&A), SIO1_RXQ_SIZE);
}

int main(void)
{
	t_registers();
	t_baud();
	t_tx();
	t_rx();
	t_lines();
	t_irq();
	t_timing();
	t_loopback();
	t_queue();

	printf("%d checks, %d failed\n", checks, fails);
	return fails != 0;
}
