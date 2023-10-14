#include <stdio.h>
#include "r3000a.h"
#include "cdrom.h"
#include "psxdma.h"
#include "mdec.h"
#include "psxevents.h"

extern int pending_exception;

//#define evprintf printf
#define evprintf(...)

u32 event_cycles[PSXINT_COUNT];

u32 schedule_timeslice(void)
{
	u32 i, c = psxRegs.cycle;
	u32 irqs = psxRegs.interrupt;
	s32 min, dif;

	min = PSXCLK;
	for (i = 0; irqs != 0; i++, irqs >>= 1) {
		if (!(irqs & 1))
			continue;
		dif = event_cycles[i] - c;
		//evprintf("  ev %d\n", dif);
		if (0 < dif && dif < min)
			min = dif;
	}
	next_interupt = c + min;
	return next_interupt;
}

static void irqNoOp() {
}

typedef void (irq_func)();

static irq_func * const irq_funcs[] = {
	[PSXINT_SIO]	= sioInterrupt,
	[PSXINT_CDR]	= cdrInterrupt,
	[PSXINT_CDREAD]	= cdrPlayReadInterrupt,
	[PSXINT_GPUDMA]	= gpuInterrupt,
	[PSXINT_MDECOUTDMA] = mdec1Interrupt,
	[PSXINT_SPUDMA]	= spuInterrupt,
	[PSXINT_MDECINDMA] = mdec0Interrupt,
	[PSXINT_GPUOTCDMA] = gpuotcInterrupt,
	[PSXINT_CDRDMA] = cdrDmaInterrupt,
	[PSXINT_NEWDRC_CHECK] = irqNoOp,
	[PSXINT_CDRLID] = cdrLidSeekInterrupt,
	[PSXINT_IRQ10] = irq10Interrupt,
	[PSXINT_SPU_UPDATE] = spuUpdate,
	[PSXINT_SPU_IRQ] = spuDelayedIrq,
	[PSXINT_RCNT] = psxRcntUpdate,
};

/* local dupe of psxBranchTest, using event_cycles */
void irq_test(psxCP0Regs *cp0)
{
	u32 cycle = psxRegs.cycle;
	u32 irq, irq_bits;

	for (irq = 0, irq_bits = psxRegs.interrupt; irq_bits != 0; irq++, irq_bits >>= 1) {
		if (!(irq_bits & 1))
			continue;
		if ((s32)(cycle - event_cycles[irq]) >= 0) {
			// note: irq_funcs() also modify psxRegs.interrupt
			psxRegs.interrupt &= ~(1u << irq);
			irq_funcs[irq]();
		}
	}

	cp0->n.Cause &= ~0x400;
	if (psxHu32(0x1070) & psxHu32(0x1074))
		cp0->n.Cause |= 0x400;
	if (((cp0->n.Cause | 1) & cp0->n.SR & 0x401) == 0x401) {
		psxException(0, 0, cp0);
		pending_exception = 1;
	}
}

void gen_interupt(psxCP0Regs *cp0)
{
	evprintf("  +ge %08x, %u->%u (%d)\n", psxRegs.pc, psxRegs.cycle,
		next_interupt, next_interupt - psxRegs.cycle);

	irq_test(cp0);
	//pending_exception = 1;

	schedule_timeslice();

	evprintf("  -ge %08x, %u->%u (%d)\n", psxRegs.pc, psxRegs.cycle,
		next_interupt, next_interupt - psxRegs.cycle);
}

void events_restore(void)
{
	int i;
	for (i = 0; i < PSXINT_COUNT; i++)
		event_cycles[i] = psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle;

	event_cycles[PSXINT_RCNT] = psxNextsCounter + psxNextCounter;
	psxRegs.interrupt |=  1 << PSXINT_RCNT;
	psxRegs.interrupt &= (1 << PSXINT_COUNT) - 1;
}
