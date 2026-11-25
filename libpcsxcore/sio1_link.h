/*
 * SIO1 link between two emulator processes on one machine.
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef __SIO1_LINK_H__
#define __SIO1_LINK_H__

#include <stddef.h>
#include <stdint.h>
#include "sio1.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sio1_link;

enum sio1_link_status {
	SIO1_LS_WAITING = 0,	/* the other console is not there yet */
	SIO1_LS_LINKED,
	SIO1_LS_LOST,		/* it left or stopped responding */
};

struct sio1_link_stats {
	uint64_t stops;		/* scheduler stops */
	uint64_t accesses;	/* register accesses */
	uint64_t waits;		/* times this side had to wait for the peer */
	uint64_t spins;		/* looks at the peer while waiting */
	uint64_t wait_ns;	/* host time spent waiting (slept part only) */
	uint64_t sent, received;
};

/* side: 0 = console A, 1 = console B. config is compared with the peer's
 * and must match (emulated clock settings), it is opaque here.
 * line_delay: cycles a line change takes to reach the other end, see
 * sio1_link.c; has to be the same on both sides.
 * Returns NULL and a message in err on failure. */
struct sio1_link *sio1_link_open(const char *name, int side,
	struct sio1_port *port, uint32_t config, uint32_t line_delay,
	char *err, size_t err_len);
void sio1_link_close(struct sio1_link *l);
enum sio1_link_status sio1_link_status(const struct sio1_link *l);
const struct sio1_link_stats *sio1_link_get_stats(const struct sio1_link *l);
/* one line on the last status change, for the log */
const char *sio1_link_message(const struct sio1_link *l);

/* port->emit handler; times are the port's (local) times */
void sio1_link_send(struct sio1_link *l, const struct sio1_event *ev);

/* Around every register access at local time t. _begin returns once
 * everything the peer did that matters at t is known and the port model
 * has been advanced there. It returns the time to use for the access
 * itself and for _end, which is t unless time appeared to go backwards. */
uint64_t sio1_link_access_begin(struct sio1_link *l, uint64_t t);
void sio1_link_access_end(struct sio1_link *l, uint64_t t);

/* At a scheduler stop at local time t, which should be at or shortly after
 * the time the previous call returned. Brings the port model up to that
 * time, waiting for the peer if it is behind, and returns the local time
 * of the next stop. */
uint64_t sio1_link_stop(struct sio1_link *l, uint64_t t);

/* the local clock jumped (savestate load): the link can not continue */
void sio1_link_break(struct sio1_link *l, uint64_t t, const char *why);

#ifdef __cplusplus
}
#endif
#endif /* __SIO1_LINK_H__ */
