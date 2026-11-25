/*
 * Test of the two process SIO1 link (libpcsxcore/sio1_link.c).
 *
 * Two scripted "consoles" hammer their ports with pseudo-random register
 * accesses that depend on what they read back. Each keeps a hash of every
 * access (cycle, register, value) and of every interrupt it sees.
 *
 *   test_link ref <seed> <cycles> <line delay>
 *       both consoles in one process, joined by an ideal cable that is
 *       stepped in global emulated time order: the reference result
 *   test_link A|B <link name> <seed> <cycles> <line delay> [jitter seed]
 *       one console in this process, linked to the other one. With a
 *       jitter seed the process sleeps at random to shake host timing.
 *
 * The link is correct if both linked processes print the lines the
 * reference prints, whatever the host timing was. See run_link_test.sh.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../../libpcsxcore/sio1.h"
#include "../../libpcsxcore/sio1_link.h"

#ifndef SIO1_LINK_GRID
#define SIO1_LINK_GRID 256
#endif

#define DR   0x0
#define SR   0x4
#define MR   0x8
#define CR   0xa
#define BR   0xe

struct rng { uint64_t s; };

static uint32_t rnd(struct rng *r)
{
	/* splitmix64 */
	uint64_t z = (r->s += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return (uint32_t)((z ^ (z >> 31)) >> 16);
}

struct console {
	int side;
	struct sio1_port port;
	struct rng script, step;
	uint64_t hash;
	uint64_t next_access;	/* link time */
	uint64_t next_stop;	/* link time, where the cpu really stops */
	uint64_t stop_grid;	/* link time, the grid point it stops for */
	uint64_t end;
	uint16_t ctrl;
	uint64_t irq_times[256]; /* local times of interrupts not seen yet */
	int irq_pending;
	uint32_t n_access, n_irq, n_rx, n_tx, n_err;
	int configured;
};

static FILE *trace[2];

static void hash_mix(struct console *c, uint64_t v)
{
	int i;

	if (trace[c->side])
		fprintf(trace[c->side], "%llx\n", (unsigned long long)v);
	for (i = 0; i < 8; i++) {
		c->hash ^= (v >> (i * 8)) & 0xff;
		c->hash *= 0x100000001b3ull;
	}
}

static void on_irq(void *ctx)
{
	struct console *c = ctx;

	if (c->irq_pending < 256)
		c->irq_times[c->irq_pending++] = c->port.t_event;
}

/* --- what a console does with its port; all in link time --- */

struct ops {
	uint32_t (*read)(struct console *c, uint64_t tl, uint32_t ofs, int w);
	void (*write)(struct console *c, uint64_t tl, uint32_t ofs, uint32_t v, int w);
};

/* every access is its own instruction, a few cycles after the last */
#define OP_CYCLES 3

static uint32_t traced_read(struct console *c, const struct ops *o,
	uint64_t *ptl, uint32_t ofs, int w)
{
	uint64_t tl = *ptl;
	uint32_t v = o->read(c, tl, ofs, w);

	*ptl += OP_CYCLES;
	if (tl <= c->end) {
		hash_mix(c, tl);
		hash_mix(c, 0x100 | ofs | (w << 4));
		hash_mix(c, v);
		c->n_access++;
	}
	return v;
}

static void traced_write(struct console *c, const struct ops *o,
	uint64_t *ptl, uint32_t ofs, uint32_t v, int w)
{
	uint64_t tl = *ptl;

	o->write(c, tl, ofs, v, w);
	*ptl += OP_CYCLES;	if (tl <= c->end) {
		hash_mix(c, tl);
		hash_mix(c, 0x200 | ofs | (w << 4));
		hash_mix(c, v);
		c->n_access++;
	}
}

/* Interrupts are recorded with the time of the stop or access that brought
 * the port up to their cause. upto: local time the port was brought to. */
static void note_irqs(struct console *c, uint64_t tl, uint64_t upto)
{
	int i, n = 0;

	for (i = 0; i < c->irq_pending; i++) {
		if (c->irq_times[i] <= upto)
			n++;
		else
			c->irq_times[i - n] = c->irq_times[i];
	}
	c->irq_pending -= n;
	if (n && tl <= c->end) {
		hash_mix(c, tl);
		hash_mix(c, 0x300 + n);
		c->n_irq += n;
	}
}

/* returns the time of the last access made */
static uint64_t script_access(struct console *c, const struct ops *o,
	uint64_t tl_)
{
	uint64_t t_ = tl_, *tl = &t_;
	static const uint8_t modes[] = { 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e,
					 0x4d, 0x4f, 0xce, 0x7e, 0x5e, 0x4a };
	static const uint16_t bauds[] = { 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
					  0x24, 0x08, 0x02, 0x01, 0x48 };
	uint32_t r = rnd(&c->script), sr, gap;

	if (!c->configured) {
		traced_write(c, o, tl, MR, 0x4e, 2);
		traced_write(c, o, tl, BR, 0x12, 2);
		c->ctrl = SIO1_CR_TXEN | SIO1_CR_DTR | SIO1_CR_RXEN | SIO1_CR_RTS;
		traced_write(c, o, tl, CR, c->ctrl, 2);
		c->configured = 1;
	}
	else switch (r % 64) {
	case 0: /* another format or rate, the peer may or may not follow */
		if (rnd(&c->script) % 4 == 0) {
			traced_write(c, o, tl, MR, modes[rnd(&c->script) % sizeof(modes)], 2);
			traced_write(c, o, tl, BR, bauds[rnd(&c->script)
				% (sizeof(bauds) / sizeof(bauds[0]))], 2);
		}
		break;
	case 1: /* flow control */
	case 2:
		c->ctrl ^= SIO1_CR_RTS;
		traced_write(c, o, tl, CR, c->ctrl, 2);
		break;
	case 3:
		c->ctrl ^= SIO1_CR_DTR;
		traced_write(c, o, tl, CR, c->ctrl, 2);
		break;
	case 4: /* interrupt enables and receive threshold */
	case 5:
		c->ctrl &= ~(SIO1_CR_TXIEN | SIO1_CR_RXIEN | SIO1_CR_DSRIEN
				| SIO1_CR_BUFSZ);
		c->ctrl |= (rnd(&c->script) & 7) << 10;
		c->ctrl |= (rnd(&c->script) & 3) << 8;
		traced_write(c, o, tl, CR, c->ctrl, 2);
		break;
	case 6:
		c->ctrl ^= SIO1_CR_TXEN;
		traced_write(c, o, tl, CR, c->ctrl, 2);
		break;
	case 7:
		if (rnd(&c->script) % 8 == 0) {
			c->ctrl ^= SIO1_CR_RXEN;
			traced_write(c, o, tl, CR, c->ctrl, 2);
		}
		break;
	case 8:
		if (rnd(&c->script) % 16 == 0) {
			traced_write(c, o, tl, CR, SIO1_CR_INTRST, 2);
			c->configured = 0;
		}
		break;
	case 9: /* odd access widths */
		traced_read(c, o, tl, SR, 4);
		traced_read(c, o, tl, DR, rnd(&c->script) & 1 ? 2 : 4);
		break;
	default:
		sr = traced_read(c, o, tl, SR, 2);
		if ((sr & (SIO1_SR_PERROR | SIO1_SR_OE | SIO1_SR_FE))
		    && t_ <= c->end)
			c->n_err++;
		if (sr & (SIO1_SR_IRQ | SIO1_SR_PERROR | SIO1_SR_OE | SIO1_SR_FE))
			traced_write(c, o, tl, CR, c->ctrl | SIO1_CR_ERRRST, 2);
		if ((sr & SIO1_SR_RXRDY) && (r & 0x100)) {
			if (t_ <= c->end)
				c->n_rx++;
			traced_read(c, o, tl, DR, 1);
		}
		if ((sr & SIO1_SR_TXRDY) && (r & 0x600)) {
			if (t_ <= c->end)
				c->n_tx++;
			traced_write(c, o, tl, DR, r >> 16, 1);
		}
		break;
	}

	/* mostly tight polling, sometimes away for a while */
	gap = rnd(&c->script);
	if (gap % 16 == 0)
		gap = 2000 + (gap >> 8) % 60000;
	else
		gap = 20 + (gap >> 8) % 900;
	c->next_access = t_ + gap;
	/* time does not go back: a stop that was due during these
	 * instructions is taken after them */
	if (c->next_stop < t_)
		c->next_stop = t_;
	return t_ - OP_CYCLES;
}

/* a stop lands a little after the time asked for, like a cpu that only
 * checks at the end of a block; accesses before that still come first */
static uint64_t overshoot(struct console *c)
{
	return rnd(&c->step) % 48;
}

static void step_init(struct console *c, uint32_t seed)
{
	c->step.s = seed * 40503u + c->side * 1234567u + 99;
	c->stop_grid = SIO1_LINK_GRID;
	c->next_stop = c->stop_grid + overshoot(c);
}

static void console_init(struct console *c, int side, uint32_t seed,
	uint64_t end)
{
	memset(c, 0, sizeof(*c));
	c->side = side;
	sio1p_init(&c->port);
	c->port.irq = on_irq;
	c->port.ctx = c;
	c->script.s = seed * 2654435761u + side * 77777u;
	c->hash = 0xcbf29ce484222325ull;
	c->end = end;
	c->next_access = 1 + 300 + side * 1000;
	step_init(c, seed);
}

static void console_report(const struct console *c)
{
	printf("%c hash %016llx accesses %u irqs %u tx %u rx %u errors %u\n",
		'A' + c->side, (unsigned long long)c->hash, c->n_access,
		c->n_irq, c->n_tx, c->n_rx, c->n_err);
}

/* --- reference: both consoles in this process --- */

static struct console ref_con[2];
/* the consoles' local clocks differ, as two emulators' would */
static const uint64_t ref_epoch[2] = { 1000 - 1, 5000000 - 1 };
static uint32_t line_delay;

/* what the cable does to an event, as sio1_link.c has it */
static void ref_cable(struct console *from, struct sio1_event *e)
{
	if (e->type == SIO1_EV_LINES)
		e->cycle += line_delay;
	else if (e->cycle < from->port.t_event + line_delay)
		e->cycle = from->port.t_event + line_delay;
}

static void ref_emit_a(void *ctx, const struct sio1_event *ev)
{
	struct sio1_event e = *ev;

	(void)ctx;
	ref_cable(&ref_con[0], &e);
	e.cycle = e.cycle - ref_epoch[0] + ref_epoch[1];
	sio1p_deliver(&ref_con[1].port, &e);
}

static void ref_emit_b(void *ctx, const struct sio1_event *ev)
{
	struct sio1_event e = *ev;

	(void)ctx;
	ref_cable(&ref_con[1], &e);
	e.cycle = e.cycle - ref_epoch[1] + ref_epoch[0] + 1; /* A before B */
	sio1p_deliver(&ref_con[0].port, &e);
}

/* both port models up to link time tl, in global time order */
static void ref_run_until(uint64_t tl)
{
	for (;;) {
		uint64_t na = sio1p_next_event(&ref_con[0].port);
		uint64_t nb = sio1p_next_event(&ref_con[1].port);
		uint64_t n;

		if (na != SIO1_TIME_NEVER) na -= ref_epoch[0];
		if (nb != SIO1_TIME_NEVER) nb -= ref_epoch[1];
		n = na < nb ? na : nb;
		if (n > tl)
			break;
		sio1p_advance(&ref_con[0].port, n + ref_epoch[0]);
		sio1p_advance(&ref_con[1].port, n + ref_epoch[1]);
	}
	sio1p_advance(&ref_con[0].port, tl + ref_epoch[0]);
	sio1p_advance(&ref_con[1].port, tl + ref_epoch[1]);
}

/* Each console is a thread, and only the one whose next step is earliest
 * in emulated time runs (A first on a tie). A plain, obviously ordered
 * way to get what the link has to get with two free running processes. */
static pthread_mutex_t ref_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ref_cond = PTHREAD_COND_INITIALIZER;
static uint64_t ref_key[2];
static int ref_waiting[2], ref_done[2];

static void ref_turn(struct console *c, uint64_t key)
{
	int me = c->side, other = !me;

	pthread_mutex_lock(&ref_lock);
	if (key < ref_key[me])
		key = ref_key[me];
	ref_key[me] = key;
	ref_waiting[me] = 1;
	pthread_cond_broadcast(&ref_cond);
	while (!ref_done[other]
	       && !(ref_waiting[other]
		    && (key < ref_key[other]
			|| (key == ref_key[other] && me == 0))))
		pthread_cond_wait(&ref_cond, &ref_lock);
	ref_waiting[me] = 0;
	pthread_mutex_unlock(&ref_lock);
}

static uint32_t ref_read(struct console *c, uint64_t tl, uint32_t ofs, int w)
{
	ref_turn(c, tl);
	ref_run_until(tl);
	return sio1p_read(&c->port, tl + ref_epoch[c->side], ofs, w);
}

static void ref_write(struct console *c, uint64_t tl, uint32_t ofs,
	uint32_t v, int w)
{
	ref_turn(c, tl);
	ref_run_until(tl);
	sio1p_write(&c->port, tl + ref_epoch[c->side], ofs, v, w);
}

static const struct ops ref_ops = { ref_read, ref_write };

static void *ref_thread(void *arg)
{
	struct console *c = arg;

	for (;;) {
		int access = c->next_access <= c->next_stop;
		uint64_t tl = access ? c->next_access : c->next_stop;

		if (tl > c->end)
			break;
		if (access) {
			uint64_t last = script_access(c, &ref_ops, tl);

			/* some steps touch nothing, and then see nothing */
			if (last >= tl) {
				ref_turn(c, last);
				ref_run_until(last);
				note_irqs(c, tl, last + ref_epoch[c->side]);
			}
		}
		else {
			if (trace[c->side] && getenv("SIO1_TEST_STOPS"))
				fprintf(trace[c->side], "S %llx %llx\n",
					(unsigned long long)c->stop_grid,
					(unsigned long long)tl);
			/* the port is only brought up to the grid point */
			ref_turn(c, c->stop_grid);
			ref_run_until(c->stop_grid);
			note_irqs(c, tl, c->stop_grid + ref_epoch[c->side]);
			c->stop_grid = (tl / SIO1_LINK_GRID + 1) * SIO1_LINK_GRID;
			c->next_stop = c->stop_grid + overshoot(c);
		}
	}
	pthread_mutex_lock(&ref_lock);
	ref_done[c->side] = 1;
	pthread_cond_broadcast(&ref_cond);
	pthread_mutex_unlock(&ref_lock);
	return NULL;
}

static int run_ref(uint32_t seed, uint64_t end)
{
	struct sio1_event ev;
	pthread_t th[2];
	int i;

	for (i = 0; i < 2; i++) {
		console_init(&ref_con[i], i, seed, end);
		ref_con[i].port.emit = i ? ref_emit_b : ref_emit_a;
		sio1p_advance(&ref_con[i].port, ref_epoch[i] + 1);
	}
	for (i = 0; i < 2; i++) {
		/* as when the cable is plugged in */
		memset(&ev, 0, sizeof(ev));
		ev.type = SIO1_EV_LINES;
		ev.cycle = ref_epoch[i] + 1;
		ref_con[i].port.emit(NULL, &ev);
	}
	for (i = 0; i < 2; i++)
		pthread_create(&th[i], NULL, ref_thread, &ref_con[i]);
	for (i = 0; i < 2; i++)
		pthread_join(th[i], NULL);

	console_report(&ref_con[0]);
	console_report(&ref_con[1]);
	return 0;
}

/* --- one console linked to another process --- */

static struct sio1_link *lnk;
static uint64_t lnk_epoch;
static struct rng jitter;
static int jitter_on;

static void maybe_sleep(void)
{
	uint32_t r;

	if (!jitter_on)
		return;
	r = rnd(&jitter);
	if (r % 4000 == 0)
		usleep(3000 + r % 5000);
	else if (r % 200 == 0)
		usleep(r % 300);
}

static void lnk_emit(void *ctx, const struct sio1_event *ev)
{
	(void)ctx;
	sio1_link_send(lnk, ev);
}

static uint32_t lnk_read(struct console *c, uint64_t tl, uint32_t ofs, int w)
{
	uint64_t t = tl + lnk_epoch;
	uint32_t v;

	t = sio1_link_access_begin(lnk, t);
	v = sio1p_read(&c->port, t, ofs, w);
	sio1_link_access_end(lnk, t);
	return v;
}

static void lnk_write(struct console *c, uint64_t tl, uint32_t ofs,
	uint32_t v, int w)
{
	uint64_t t = tl + lnk_epoch;

	t = sio1_link_access_begin(lnk, t);
	sio1p_write(&c->port, t, ofs, v, w);
	sio1_link_access_end(lnk, t);
}

static const struct ops lnk_ops = { lnk_read, lnk_write };

static int run_side(int side, const char *name, uint32_t seed, uint64_t end)
{
	struct console con, *c = &con;
	const struct sio1_link_stats *st;
	char err[200];
	uint64_t t = side ? 5000000 : 1000; /* local time */
	struct timespec t0, t1;
	int tries = 0;

	console_init(c, side, seed, end);
	c->port.emit = lnk_emit;
	for (;;) {
		lnk = sio1_link_open(name, side, &c->port, 0x1234, line_delay,
			err, sizeof(err));
		if (lnk != NULL)
			break;
		if (++tries > 100) {
			fprintf(stderr, "%c: %s\n", 'A' + side, err);
			return 2;
		}
		usleep(20000);
	}

	/* wait for the other console, as a running emulator would: stopping
	 * now and then while its own time goes on */
	while (sio1_link_status(lnk) == SIO1_LS_WAITING) {
		sio1_link_stop(lnk, t);
		if (sio1_link_status(lnk) != SIO1_LS_WAITING)
			break;
		t += 100000 + rnd(&c->step) % 1000;
		usleep(1000);
		if (++tries > 20000) {
			fprintf(stderr, "%c: nobody came\n", 'A' + side);
			return 2;
		}
	}
	/* that stop joined the link; link time 1 is local time t.
	 * The step generator starts over so that the wait does not count. */
	lnk_epoch = t - 1;
	step_init(c, seed);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	/* a little past the end so that the other one gets there too */
	while (sio1_link_status(lnk) == SIO1_LS_LINKED) {
		int access = c->next_access <= c->next_stop;
		uint64_t tl = access ? c->next_access : c->next_stop;

		if (tl > end + 64 * SIO1_LINK_GRID)
			break;
		maybe_sleep();
		if (access)
			script_access(c, &lnk_ops, tl);
		else {
			if (trace[c->side] && getenv("SIO1_TEST_STOPS"))
				fprintf(trace[c->side], "S %llx %llx\n",
					(unsigned long long)c->stop_grid,
					(unsigned long long)tl);
			c->stop_grid = sio1_link_stop(lnk, tl + lnk_epoch)
				- lnk_epoch;
			c->next_stop = c->stop_grid + overshoot(c);
		}
		note_irqs(c, tl, ~(uint64_t)0);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	console_report(c);
	st = sio1_link_get_stats(lnk);
	fprintf(stderr, "%c: %s; %.2fs host for %.2fs emulated, %llu stops, "
		"%llu accesses, %llu waits (%llu looks, %.0f ms asleep), "
		"sent %llu got %llu\n",
		'A' + side, sio1_link_message(lnk),
		(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9,
		end / 33868800.0,
		(unsigned long long)st->stops, (unsigned long long)st->accesses,
		(unsigned long long)st->waits, (unsigned long long)st->spins,
		st->wait_ns / 1e6,
		(unsigned long long)st->sent, (unsigned long long)st->received);
	sio1_link_close(lnk);
	return 0;
}

int main(int argc, char *argv[])
{
	/* SIO1_TEST_TRACE=prefix writes everything hashed to prefix.A/.B */
	if (getenv("SIO1_TEST_TRACE") && argc > 1) {
		char name[256];
		int i;

		for (i = 0; i < 2; i++) {
			if (argv[1][0] != 'r' && argv[1][0] != 'A' + i)
				continue;
			snprintf(name, sizeof(name), "%s.%c",
				getenv("SIO1_TEST_TRACE"), 'A' + i);
			trace[i] = fopen(name, "w");
		}
	}
	if (argc == 5 && strcmp(argv[1], "ref") == 0) {
		line_delay = atoi(argv[4]);
		return run_ref(atoi(argv[2]), strtoull(argv[3], NULL, 0));
	}
	if (argc >= 6 && (argv[1][0] == 'A' || argv[1][0] == 'B')) {
		line_delay = atoi(argv[5]);
		if (argc > 6) {
			jitter_on = 1;
			jitter.s = strtoull(argv[6], NULL, 0) * 31 + argv[1][0];
		}
		return run_side(argv[1][0] - 'A', argv[2], atoi(argv[3]),
			strtoull(argv[4], NULL, 0));
	}
	fprintf(stderr, "usage: %s ref <seed> <cycles> <line delay>\n"
		"       %s A|B <name> <seed> <cycles> <line delay> [jitter seed]\n",
		argv[0], argv[0]);
	return 1;
}
