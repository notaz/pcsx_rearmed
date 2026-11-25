/*
 * SIO1 link between two emulator processes on one machine.
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * Each process runs one console. What crosses the cable (characters, line
 * changes) is stamped in emulated cycles and passed through a shared memory
 * ring, and each side publishes how far it has run. The rules below make
 * the outcome a function of emulated time only, never of host timing:
 *
 *  1. The port model is only advanced to times for which everything the
 *     peer did is known. A side publishes a clock: nothing it does later
 *     reaches the other end before that clock plus the line delay.
 *  2. A register access at cycle t waits until the peer is known up to t.
 *  3. In between, both sides stop on a fixed grid of SIO1_LINK_GRID cycles
 *     and bring the model up to that grid point. The model, and with it
 *     the interrupt request, is never updated at any other time, so an
 *     interrupt is raised up to one grid step after its cause, always at
 *     the same cycle.
 *
 * The line delay is the time a level change (DTR, RTS) or the end of a
 * start bit takes to be seen at the other end. With 0 an access sees the
 * other console's lines as of the very same cycle, which makes the two
 * processes take turns at every access. A delay of at least one grid step
 * lets them run side by side.
 *
 * Cycles that are equal on both sides are ordered A before B: console B's
 * events are seen by A one cycle later.
 */

#include "sio1_link.h"

/* needs a shared file mapping and flock() */
#if !defined(SIO1_LINK_SHM) && !defined(NO_SIO1_LINK) \
    && (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
        || defined(__OpenBSD__) || defined(__NetBSD__)) \
    && !defined(__ANDROID__)
#define SIO1_LINK_SHM
#endif

#ifdef SIO1_LINK_SHM

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define LINK_MAGIC		0x314f4953u	/* "SIO1" */
#define LINK_VERSION		3
#define RING_SIZE		SIO1_RXQ_SIZE
#define HEARTBEAT_ALONE		(33868800u / 60)
#define LOST_TIMEOUT_NS		3000000000ull

/* 256 cycles are 7.6us, less than one bit time at 115200 baud */
#ifndef SIO1_LINK_GRID
#define SIO1_LINK_GRID		256
#endif

enum { ST_ABSENT = 0, ST_PRESENT, ST_LINKED, ST_LEFT };

struct link_side {
	uint32_t state;
	uint32_t pid;
	uint32_t config;
	uint32_t delay;
	uint32_t seq;		/* odd while clock is being written */
	uint32_t pad0;
	/* link time (cycles since the two were joined, starting at 1):
	 * nothing stamped below this plus the line delay is still to
	 * come from this side */
	uint64_t clock;
	uint32_t ring_w;	/* events this side has put into ring[] */
	uint32_t ring_ack;	/* events of the peer's ring it has taken */
	struct sio1_event ring[RING_SIZE];
	uint8_t  pad[64];
};

struct link_shm {
	uint32_t magic;
	uint32_t version;
	uint32_t session;
	uint32_t pad;
	struct link_side side[2];
};

struct sio1_link {
	struct link_shm *shm;
	struct link_side *me, *peer;
	struct sio1_port *port;
	int fd, side;
	enum sio1_link_status status;
	uint64_t epoch;		/* local time = link time + epoch */
	uint64_t peer_clock;	/* last value read */
	uint64_t last_clock;	/* last value published */
	uint64_t min_time;	/* local; own events are stamped from here on */
	uint64_t grid;		/* local time of the grid point being run to */
	uint32_t delay;
	uint32_t ring_r;
	struct sio1_link_stats stats;
	char msg[160];
};

/* Nothing here depends on a store being seen at once by the other side:
 * whoever has to wait keeps looking. */
#define aload(p)	__atomic_load_n(p, __ATOMIC_ACQUIRE)
#define astore(p, v)	__atomic_store_n(p, v, __ATOMIC_RELEASE)

static void cpu_relax(void)
{
#if defined(__i386__) || defined(__x86_64__)
	__builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
	__asm__ __volatile__("yield");
#endif
}

static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

/* a seqlock, as 64 bit atomics are not everywhere */
static void clock_write(struct link_side *s, uint64_t v)
{
	uint32_t q = aload(&s->seq);

	astore(&s->seq, q + 1);
	s->clock = v;
	astore(&s->seq, q + 2);
}

static uint64_t clock_read(struct link_side *s)
{
	for (;;) {
		uint32_t q = aload(&s->seq);

		if (!(q & 1)) {
			uint64_t v = s->clock;

			if (aload(&s->seq) == q)
				return v;
		}
		cpu_relax();
	}
}

static int pid_alive(uint32_t pid)
{
	return pid != 0 && (kill((pid_t)pid, 0) == 0 || errno != ESRCH);
}

static int side_in_use(const struct link_side *s)
{
	return (s->state == ST_PRESENT || s->state == ST_LINKED)
		&& pid_alive(s->pid);
}

static void link_path(char *buf, size_t size, const char *name)
{
	const char *dir = getenv("PCSX_SIO1_LINK_DIR");
	char clean[64];
	size_t i;

	if (dir == NULL && access("/dev/shm", W_OK) == 0)
		dir = "/dev/shm";
	if (dir == NULL)
		dir = getenv("TMPDIR");
	if (dir == NULL)
		dir = "/tmp";
	if (name == NULL || name[0] == 0)
		name = "default";
	for (i = 0; name[i] && i < sizeof(clean) - 1; i++) {
		char c = name[i];
		int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_';
		clean[i] = ok ? c : '_';
	}
	clean[i] = 0;
	snprintf(buf, size, "%s/pcsx_rearmed_sio1_%s", dir, clean);
}

struct sio1_link *sio1_link_open(const char *name, int side,
	struct sio1_port *port, uint32_t config, uint32_t line_delay,
	char *err, size_t err_len)
{
	struct sio1_link *l;
	struct link_shm *shm;
	char path[256];
	struct stat st;
	int fd;

	link_path(path, sizeof(path), name);
	if (name == NULL || name[0] == 0)
		name = "default";
	fd = open(path, O_RDWR | O_CREAT, 0600);
	if (fd < 0) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return NULL;
	}
	if (flock(fd, LOCK_EX) != 0 || fstat(fd, &st) != 0) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		close(fd);
		return NULL;
	}
	if ((size_t)st.st_size != sizeof(*shm)
	    && (ftruncate(fd, 0) != 0 || ftruncate(fd, sizeof(*shm)) != 0)) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		close(fd);
		return NULL;
	}
	shm = mmap(NULL, sizeof(*shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shm == MAP_FAILED) {
		snprintf(err, err_len, "mmap %s: %s", path, strerror(errno));
		close(fd);
		return NULL;
	}

	if (shm->magic != LINK_MAGIC || shm->version != LINK_VERSION
	    || (!side_in_use(&shm->side[0]) && !side_in_use(&shm->side[1]))) {
		/* nobody there (or leftovers of a crash): start over */
		uint32_t session = shm->magic == LINK_MAGIC ? shm->session : 0;

		memset(shm, 0, sizeof(*shm));
		shm->version = LINK_VERSION;
		shm->session = session + 1;
		astore(&shm->magic, LINK_MAGIC);
	}
	else if (side_in_use(&shm->side[side])) {
		snprintf(err, err_len, "console %c of link '%s' is already "
			"running (pid %u)", 'A' + side, name,
			shm->side[side].pid);
		goto fail;
	}
	else if (shm->side[!side].state != ST_PRESENT) {
		/* the other side is in the middle of a session with a
		 * console that is gone; it will notice and leave */
		snprintf(err, err_len, "the other console of link '%s' has not "
			"left its previous session yet, try again", name);
		goto fail;
	}

	l = calloc(1, sizeof(*l));
	if (l == NULL) {
		snprintf(err, err_len, "out of memory");
		goto fail;
	}
	l->shm = shm;
	l->fd = fd;
	l->side = side;
	l->me = &shm->side[side];
	l->peer = &shm->side[!side];
	l->port = port;
	l->status = SIO1_LS_WAITING;
	l->delay = line_delay;
	snprintf(l->msg, sizeof(l->msg), "console %c of link '%s' is waiting "
		"for console %c", 'A' + side, name, 'B' - side);

	memset(l->me, 0, sizeof(*l->me));
	l->me->pid = getpid();
	l->me->config = config;
	l->me->delay = line_delay;
	astore(&l->me->state, ST_PRESENT);

	flock(fd, LOCK_UN);
	return l;

fail:
	munmap(shm, sizeof(*shm));
	flock(fd, LOCK_UN);
	close(fd);
	return NULL;
}

void sio1_link_close(struct sio1_link *l)
{
	if (l == NULL)
		return;
	if (l->status != SIO1_LS_LOST)
		astore(&l->me->state, ST_LEFT);
	munmap(l->shm, sizeof(*l->shm));
	close(l->fd);
	free(l);
}

enum sio1_link_status sio1_link_status(const struct sio1_link *l)
{
	return l->status;
}

const struct sio1_link_stats *sio1_link_get_stats(const struct sio1_link *l)
{
	return &l->stats;
}

const char *sio1_link_message(const struct sio1_link *l)
{
	return l->msg;
}

/* the cable is gone: the port sees its inputs drop and runs on alone */
static void lost(struct sio1_link *l, uint64_t t, const char *why)
{
	struct sio1_event ev;

	if (l->status == SIO1_LS_LOST)
		return;
	if (l->status == SIO1_LS_LINKED) {
		memset(&ev, 0, sizeof(ev));
		ev.type = SIO1_EV_LINES;
		ev.cycle = t > l->port->s.now ? t : l->port->s.now;
		/* whatever was on its way is not coming */
		l->port->s.rxq_r = l->port->s.rxq_w;
		sio1p_deliver(l->port, &ev);
	}
	l->status = SIO1_LS_LOST;
	astore(&l->me->state, ST_LEFT);
	snprintf(l->msg, sizeof(l->msg), "link lost: %s", why);
}

void sio1_link_break(struct sio1_link *l, uint64_t t, const char *why)
{
	lost(l, t, why);
}

/* up to which link time everything the peer did is known */
static uint64_t knowledge(const struct sio1_link *l)
{
	if (l->peer_clock == 0)
		return 0; /* it has not joined yet */
	return l->peer_clock + l->delay - (l->side == 0 ? 0 : 1);
}

/* read the peer's clock, then take what it has sent. In this order,
 * as it publishes its clock only after queueing what happened before it. */
static int refresh(struct sio1_link *l, uint64_t t)
{
	uint32_t w, state = aload(&l->peer->state);

	l->peer_clock = clock_read(l->peer);
	w = aload(&l->peer->ring_w);
	if ((uint32_t)(w - l->ring_r) > (uint32_t)sio1p_rxq_space(l->port)) {
		lost(l, t, "event queue overflow");
		return -1;
	}
	while (l->ring_r != w) {
		struct sio1_event ev = l->peer->ring[l->ring_r % RING_SIZE];

		l->ring_r++;
		ev.cycle += l->epoch + (l->side == 0 ? 1 : 0);
		sio1p_deliver(l->port, &ev);
		l->stats.received++;
	}
	if (l->me->ring_ack != l->ring_r)
		astore(&l->me->ring_ack, l->ring_r);

	if (state != ST_PRESENT && state != ST_LINKED) {
		lost(l, t, "the other console left");
		return -1;
	}
	return 0;
}

/* t: local time from which on this side's register accesses will be */
static void publish(struct sio1_link *l, uint64_t t)
{
	uint64_t clock = t - l->epoch;
	uint64_t bound = sio1p_unsent_bound(l->port,
				l->epoch + knowledge(l));

	/* a character that could not be put into the ring yet (the model
	 * is behind) has to stay in the peer's future */
	if (bound != SIO1_TIME_NEVER && bound - l->epoch < clock)
		clock = bound - l->epoch;
	if (clock <= l->last_clock)
		return;
	l->last_clock = clock;
	clock_write(l->me, clock);
}

static void advance_model(struct sio1_link *l, uint64_t t)
{
	uint64_t target = knowledge(l);

	if (target > t - l->epoch)
		target = t - l->epoch;
	target += l->epoch;
	if (target > l->port->s.now)
		sio1p_advance(l->port, target);
}

struct waiter {
	uint32_t seq, ring_w;	/* what the peer showed at the last look */
	uint64_t since;		/* host time it stopped moving */
	int counted;
};

/* Returns when the peer has published something new, 0, or when it has
 * not moved for too long, nonzero. The peer is usually a microsecond or
 * so away, so look hard before giving up the cpu. No pause instruction
 * at first: where it is slow (some virtual machines) it costs more than
 * the wait itself. */
static int wait_for_peer(struct sio1_link *l, struct waiter *w)
{
	unsigned spins;

	if (!w->counted) {
		w->counted = 1;
		l->stats.waits++;
	}
	for (spins = 0; ; spins++) {
		uint32_t seq = aload(&l->peer->seq);
		uint32_t ring_w = aload(&l->peer->ring_w);
		uint32_t state = aload(&l->peer->state);

		if (seq != w->seq || ring_w != w->ring_w
		    || (state != ST_PRESENT && state != ST_LINKED)) {
			w->seq = seq;
			w->ring_w = ring_w;
			w->since = 0;
			return 0;
		}
		l->stats.spins++;
		if (spins < 1000)
			;
		else if (spins < 3000)
			cpu_relax();
		else if (spins < 3200)
			sched_yield();
		else {
			struct timespec ts = { 0, 50000 };
			uint64_t t0 = now_ns(), t1;

			if (w->since == 0)
				w->since = t0;
			nanosleep(&ts, NULL);
			t1 = now_ns();
			l->stats.wait_ns += t1 - t0;
			if (t1 - w->since > LOST_TIMEOUT_NS)
				return 1;
		}
	}
}

/* Rule 1: returns with the port model at t. claim is the time from which
 * on this side can still do something, as publish() takes it. */
static void sync_to(struct sio1_link *l, uint64_t t, uint64_t claim)
{
	struct waiter w;

	memset(&w, 0, sizeof(w));
	for (;;) {
		/* before looking, to not miss what comes right after */
		w.seq = aload(&l->peer->seq);
		w.ring_w = aload(&l->peer->ring_w);
		if (refresh(l, t) < 0)
			break;
		advance_model(l, t);
		publish(l, claim);
		if (knowledge(l) >= t - l->epoch)
			return;
		if (wait_for_peer(l, &w)) {
			lost(l, t, "the other console stopped responding");
			break;
		}
	}
	sio1p_advance(l->port, t);
}

static void push_event(struct sio1_link *l, const struct sio1_event *ev)
{
	uint32_t w = l->me->ring_w;

	l->me->ring[w % RING_SIZE] = *ev;
	astore(&l->me->ring_w, w + 1);
	l->stats.sent++;
}

static void try_attach(struct sio1_link *l, uint64_t t)
{
	uint32_t state = aload(&l->peer->state);
	struct sio1_event ev;

	if (state != ST_PRESENT && state != ST_LINKED)
		return;
	if (aload(&l->peer->config) != l->me->config) {
		lost(l, t, "the two consoles use different emulation settings");
		return;
	}
	if (aload(&l->peer->delay) != l->delay) {
		lost(l, t, "the two consoles use different link timing settings");
		return;
	}
	sio1p_advance(l->port, t);
	/* link time starts at 1 on both sides, wherever each of them is */
	l->epoch = t - 1;
	l->grid = t;
	l->min_time = t;
	l->status = SIO1_LS_LINKED;
	snprintf(l->msg, sizeof(l->msg), "linked as console %c", 'A' + l->side);

	/* the cable has just been plugged in: show our levels */
	memset(&ev, 0, sizeof(ev));
	ev.type = SIO1_EV_LINES;
	ev.cycle = 1 + l->delay;
	ev.lines = l->port->s.out_lines;
	push_event(l, &ev);
	publish(l, t);
	astore(&l->me->state, ST_LINKED);
}

void sio1_link_send(struct sio1_link *l, const struct sio1_event *ev)
{
	struct sio1_event e = *ev;

	if (l->status != SIO1_LS_LINKED)
		return;
	if ((uint32_t)(l->me->ring_w - aload(&l->peer->ring_ack)) >= RING_SIZE) {
		lost(l, ev->cycle, "event ring overflow");
		return;
	}
	if (e.type == SIO1_EV_LINES)
		e.cycle += l->delay;
	else if (e.cycle < l->port->t_event + l->delay)
		/* a character is emitted when its start bit ends */
		e.cycle = l->port->t_event + l->delay;
	e.cycle -= l->epoch;
	push_event(l, &e);
}

/* Time only moves forward here: an access that claims to be earlier than
 * something already told to the peer is taken to happen right after it.
 * A cpu core that reports the cycle of each access does not get there. */
static uint64_t clamp_time(struct sio1_link *l, uint64_t t)
{
	return t < l->min_time ? l->min_time : t;
}

uint64_t sio1_link_access_begin(struct sio1_link *l, uint64_t t)
{
	l->stats.accesses++;
	if (l->status != SIO1_LS_LINKED) {
		sio1p_advance(l->port, t);
		return t;
	}
	t = clamp_time(l, t);
	if (knowledge(l) >= t - l->epoch) {
		/* What the peer did up to here was taken at the last look
		 * already (refresh() takes everything its clock covers), so
		 * there is no need to look again. This is the usual case
		 * with a line delay, and keeps shared memory out of a
		 * game's polling loops. */
		if (t > l->port->s.now)
			sio1p_advance(l->port, t);
		return t;
	}
	/* rule 2. The access itself may still change lines at t */
	sync_to(l, t, t);
	return t;
}

void sio1_link_access_end(struct sio1_link *l, uint64_t t)
{
	if (l->status != SIO1_LS_LINKED)
		return;
	l->min_time = t + 1;
	/* Without a line delay the peer may be waiting for exactly this.
	 * With one it can go on for that long past our last clock, which
	 * the grid stops keep fresh enough. */
	if (t + 1 - l->epoch >= l->last_clock + l->delay / 2)
		publish(l, t + 1);
}

uint64_t sio1_link_stop(struct sio1_link *l, uint64_t t)
{
	uint64_t next;

	l->stats.stops++;
	if (l->status == SIO1_LS_WAITING)
		try_attach(l, t);
	if (l->status == SIO1_LS_LINKED) {
		/* rule 3. The cpu may be a little past the grid point (it
		 * only gets here between blocks), whatever it accessed up
		 * to now is done, so say so before waiting for anything */
		t = clamp_time(l, t);
		l->min_time = t + 1;
		if (l->grid > t)
			l->grid = t; /* called early: no harm */
		sync_to(l, l->grid, t + 1);
	}
	if (l->status == SIO1_LS_LINKED) {
		uint64_t tl = t - l->epoch;

		l->grid = l->epoch + (tl / SIO1_LINK_GRID + 1) * SIO1_LINK_GRID;
		return l->grid;
	}

	/* alone: the model can be kept exact */
	sio1p_advance(l->port, t);
	next = sio1p_next_event(l->port);
	if (next > t + HEARTBEAT_ALONE)
		next = t + HEARTBEAT_ALONE;
	if (next <= t)
		next = t + 1;
	return next;
}

#else /* !SIO1_LINK_SHM */

struct sio1_link *sio1_link_open(const char *name, int side,
	struct sio1_port *port, uint32_t config, uint32_t line_delay,
	char *err, size_t err_len)
{
	static const char msg[] =
		"linking two consoles is not supported in this build";
	size_t i;

	(void)name; (void)side; (void)port; (void)config; (void)line_delay;
	for (i = 0; i < sizeof(msg) && i + 1 < err_len; i++)
		err[i] = msg[i];
	if (err_len)
		err[i < err_len ? i : err_len - 1] = 0;
	return NULL;
}

void sio1_link_close(struct sio1_link *l) { (void)l; }
enum sio1_link_status sio1_link_status(const struct sio1_link *l)
{
	(void)l;
	return SIO1_LS_LOST;
}
const struct sio1_link_stats *sio1_link_get_stats(const struct sio1_link *l)
{
	(void)l;
	return NULL;
}
const char *sio1_link_message(const struct sio1_link *l)
{
	(void)l;
	return "";
}
void sio1_link_send(struct sio1_link *l, const struct sio1_event *ev)
{
	(void)l; (void)ev;
}
uint64_t sio1_link_access_begin(struct sio1_link *l, uint64_t t) { (void)l; return t; }
void sio1_link_access_end(struct sio1_link *l, uint64_t t) { (void)l; (void)t; }
uint64_t sio1_link_stop(struct sio1_link *l, uint64_t t) { (void)l; return t + 1; }
void sio1_link_break(struct sio1_link *l, uint64_t t, const char *why)
{
	(void)l; (void)t; (void)why;
}

#endif /* SIO1_LINK_SHM */
