
#include "../psxcommon.h"
#include "reguse.h"

#include "../r3000a.h"

//#define SAME_CYCLE_MODE

static const int useBSC[64] = {
	/*recSPECIAL*/ REGUSE_SUB | REGUSE_SPECIAL,
        /*recREGIMM*/ REGUSE_SUB | REGUSE_REGIMM,
        /*recJ*/     REGUSE_JUMP, 
        /*recJAL*/   REGUSE_JUMP | REGUSE_R31_W, 
        /*recBEQ*/   REGUSE_BRANCH | REGUSE_RS_R | REGUSE_RT_R, 
        /*recBNE*/   REGUSE_BRANCH | REGUSE_RS_R | REGUSE_RT_R, 
        /*recBLEZ*/  REGUSE_BRANCH | REGUSE_RS_R, 
        /*recBGTZ*/  REGUSE_BRANCH | REGUSE_RS_R,
	/*recADDI*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recADDIU*/ REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recSLTI*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recSLTIU*/ REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recANDI*/  REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recORI*/   REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recXORI*/  REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLUI*/   REGUSE_ACC | REGUSE_RT_W,
	/*recCOP0*/  REGUSE_SUB | REGUSE_COP0, 
        REGUSE_NONE, 
        /*recCOP2*/  REGUSE_SUB | REGUSE_COP2, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	/*recLB*/    REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLH*/    REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLWL*/   REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT, 
        /*recLW*/    REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLBU*/   REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLHU*/   REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT_W, 
        /*recLWR*/   REGUSE_MEM_R | REGUSE_RS_R | REGUSE_RT, 
        REGUSE_NONE,
	/*recSB*/    REGUSE_MEM_W | REGUSE_RS_R | REGUSE_RT_R, 
        /*recSH*/    REGUSE_MEM_W | REGUSE_RS_R | REGUSE_RT_R, 
        /*recSWL*/   REGUSE_MEM | REGUSE_RS_R | REGUSE_RT_R, 
        /*recSW*/    REGUSE_MEM_W | REGUSE_RS_R | REGUSE_RT_R, 
        REGUSE_NONE, REGUSE_NONE, 
        /*recSWR*/   REGUSE_MEM | REGUSE_RS_R | REGUSE_RT_R, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        /*recLWC2*/  REGUSE_MEM_R | REGUSE_RS_R | REGUSE_COP2_RT_W, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	REGUSE_NONE, REGUSE_NONE, 
        /*recSWC2*/  REGUSE_MEM_W | REGUSE_RS_R | REGUSE_COP2_RT_R, 
        /*recHLE*/   REGUSE_UNKNOWN, // TODO: can this be done in a better way
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE
};

static const int useSPC[64] = {
	/*recSLL*/   REGUSE_ACC | REGUSE_RT_R | REGUSE_RD_W, 
        REGUSE_NONE, 
        /*recSRL*/   REGUSE_ACC | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSRA*/   REGUSE_ACC | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSLLV*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        REGUSE_NONE, 
        /*recSRLV*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSRAV*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W,
	/*recJR*/    REGUSE_JUMPR | REGUSE_RS_R, 
        /*recJALR*/  REGUSE_JUMPR | REGUSE_RS_R | REGUSE_RD_W, 
        REGUSE_NONE, REGUSE_NONE, 
        /*rSYSCALL*/ REGUSE_SYS | REGUSE_PC | REGUSE_COP0_STATUS | REGUSE_EXCEPTION,
        /*recBREAK*/ REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE,
	/*recMFHI*/  REGUSE_LOGIC | REGUSE_RD_W | REGUSE_HI_R, 
        /*recMTHI*/  REGUSE_LOGIC | REGUSE_RS_R | REGUSE_HI_W, 
        /*recMFLO*/  REGUSE_LOGIC | REGUSE_RD_W | REGUSE_LO_R, 
        /*recMTLO*/  REGUSE_LOGIC | REGUSE_RS_R | REGUSE_LO_W, 
        REGUSE_NONE, REGUSE_NONE , REGUSE_NONE, REGUSE_NONE,
	/*recMULT*/  REGUSE_MULT | REGUSE_RS_R | REGUSE_RT_R | REGUSE_LO_W | REGUSE_HI_W, 
        /*recMULTU*/ REGUSE_MULT | REGUSE_RS_R | REGUSE_RT_R | REGUSE_LO_W | REGUSE_HI_W, 
        /*recDIV*/   REGUSE_MULT | REGUSE_RS_R | REGUSE_RT_R | REGUSE_LO_W | REGUSE_HI_W, 
        /*recDIVU*/  REGUSE_MULT | REGUSE_RS_R | REGUSE_RT_R | REGUSE_LO_W | REGUSE_HI_W, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	/*recADD*/   REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recADDU*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSUB*/   REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSUBU*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recAND*/   REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recOR*/    REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recXOR*/   REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recNOR*/   REGUSE_LOGIC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W,
	REGUSE_NONE, REGUSE_NONE, 
        /*recSLT*/   REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        /*recSLTU*/  REGUSE_ACC | REGUSE_RS_R | REGUSE_RT_R | REGUSE_RD_W, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
	REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE
};

static const int useREGIMM[32] = {
	/*recBLTZ*/  REGUSE_BRANCH | REGUSE_RS_R, 
        /*recBGEZ*/  REGUSE_BRANCH | REGUSE_RS_R, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE,
	/*recBLTZAL*/REGUSE_BRANCH | REGUSE_RS_R | REGUSE_R31_W, 
        /*recBGEZAL*/REGUSE_BRANCH | REGUSE_RS_R | REGUSE_R31_W, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE
};

static const int useCP0[32] = {
	/*recMFC0*/  REGUSE_LOGIC | REGUSE_RT_W | REGUSE_COP0_RD_R, 
        REGUSE_NONE, 
        /*recCFC0*/  REGUSE_LOGIC | REGUSE_RT_W | REGUSE_COP0_RD_R, 
        REGUSE_NONE, 
        /*recMTC0*/  REGUSE_LOGIC | REGUSE_RT_R | REGUSE_COP0_RD_W, 
        REGUSE_NONE, 
        /*recCTC0*/  REGUSE_LOGIC | REGUSE_RT_R | REGUSE_COP0_RD_W, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	/*recRFE*/   REGUSE_LOGIC | REGUSE_COP0_STATUS, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE
};

// TODO: make more explicit
static const int useCP2[64] = {
	/*recBASIC*/ REGUSE_SUB | REGUSE_BASIC, 
        /*recRTPS*/  REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        /*recNCLIP*/ REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        /*recOP*/    REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE,
	/*recDPCS*/  REGUSE_GTE, 
        /*recINTPL*/ REGUSE_GTE, 
        /*recMVMVA*/ REGUSE_GTE, 
        /*recNCDS*/  REGUSE_GTE, 
        /*recCDP*/   REGUSE_GTE, 
        REGUSE_NONE, 
        /*recNCDT*/  REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        /*recNCCS*/  REGUSE_GTE, 
        /*recCC*/    REGUSE_GTE, 
        REGUSE_NONE, 
        /*recNCS*/   REGUSE_GTE, 
        REGUSE_NONE,
	/*recNCT*/   REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE,
	/*recSQR*/   REGUSE_GTE, 
        /*recDCPL*/  REGUSE_GTE, 
        /*recDPCT*/  REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, 
        /*recAVSZ3*/ REGUSE_GTE, 
        /*recAVSZ4*/ REGUSE_GTE, 
        REGUSE_NONE,
	/*recRTPT*/  REGUSE_GTE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, REGUSE_NONE, 
        /*recGPF*/   REGUSE_GTE, 
        /*recGPL*/   REGUSE_GTE, 
        /*recNCCT*/  REGUSE_GTE
};

static const int useCP2BSC[32] = {
	/*recMFC2*/  REGUSE_LOGIC | REGUSE_RT_W | REGUSE_COP2_RD_R, 
        REGUSE_NONE, 
        /*recCFC2*/  REGUSE_LOGIC | REGUSE_RT_W | REGUSE_COP2_RD_R, 
        REGUSE_NONE, 
        /*recMTC2*/  REGUSE_LOGIC | REGUSE_RT_R | REGUSE_COP2_RD_W, 
        REGUSE_NONE, 
        /*recCTC2*/  REGUSE_LOGIC | REGUSE_RT_R | REGUSE_COP2_RD_W, 
        REGUSE_NONE,
	REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE,
	REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE,
	REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE, 
        REGUSE_NONE
};

static int getRegUse(u32 code) __attribute__ ((__pure__));
static int getRegUse(u32 code)
{
    int use = useBSC[code>>26];
    
    switch (use & REGUSE_SUBMASK) {
		  case REGUSE_NONE:
				break;
        case REGUSE_SPECIAL:
            use = useSPC[_fFunct_(code)];
            break;
        case REGUSE_REGIMM:
            use = useREGIMM[_fRt_(code)];
            break;
        case REGUSE_COP0:
            use = useCP0[_fRs_(code)];
            break;
        case REGUSE_COP2:
            use = useCP2[_fFunct_(code)];
            if ((use & REGUSE_SUBMASK) == REGUSE_BASIC)
                use = useCP2BSC[_fRs_(code)];
            break;
		  default:
				use = REGUSE_UNKNOWN;
				break;
    }
    
    if ((use & REGUSE_COP0_RD_W)) {
        if (_fRd_(code) == 12 || _fRd_(code) == 13) {
            use = REGUSE_UNKNOWN;
        }
    }
    
    return use;
}

/* returns how psxreg is used in the code instruction */
int useOfPsxReg(u32 code, int use, int psxreg)
{
    int retval = REGUSE_NONE;
    
	 // get use if it wasn't supplied
    if (-1 == use) use = getRegUse(code);
    
	 // if we don't know what the usage is, assume it's read from
    if (REGUSE_UNKNOWN == use) return REGUSE_READ;
    
    if (psxreg < 32) {
        // check for 3 standard types
		  if ((use & REGUSE_RT) && _fRt_(code) == (u32)psxreg) {
            retval |= ((use & REGUSE_RT_R) ? REGUSE_READ:0) | ((use & REGUSE_RT_W) ? REGUSE_WRITE:0);
        }
        if ((use & REGUSE_RS) && _fRs_(code) == (u32)psxreg) {
            retval |= ((use & REGUSE_RS_R) ? REGUSE_READ:0) | ((use & REGUSE_RS_W) ? REGUSE_WRITE:0);
        }
        if ((use & REGUSE_RD) && _fRd_(code) == (u32)psxreg) {
            retval |= ((use & REGUSE_RD_R) ? REGUSE_READ:0) | ((use & REGUSE_RD_W) ? REGUSE_WRITE:0);
        }
		  // some instructions explicitly writes to r31
        if ((use & REGUSE_R31_W) && 31 == psxreg) {
            retval |= REGUSE_WRITE;
        }
    } else if (psxreg == 32) { // Special register LO
        retval |= ((use & REGUSE_LO_R) ? REGUSE_READ:0) | ((use & REGUSE_LO_W) ? REGUSE_WRITE:0);
    } else if (psxreg == 33) { // Special register HI
        retval |= ((use & REGUSE_HI_R) ? REGUSE_READ:0) | ((use & REGUSE_HI_W) ? REGUSE_WRITE:0);
    }
    
    return retval;
}

//#define NOREGUSE_FOLLOW

static int _nextPsxRegUse(u32 pc, int psxreg, int numInstr) __attribute__ ((__pure__, __unused__));
static int _nextPsxRegUse(u32 pc, int psxreg, int numInstr)
{
    u32 *ptr, code, bPC = 0;
    int i, use, reguse = 0;

    for (i=0; i<numInstr; ) {
        // load current instruction
		  ptr = PSXM(pc);
		  if (ptr==NULL) {
				// going nowhere... might as well assume a write, since we will hopefully never reach here
				reguse = REGUSE_WRITE;
				break;
		  }
		  code = SWAP32(*ptr);
		  // get usage patterns for instruction
		  use = getRegUse(code);
		  // find the use of psxreg in the instruction
        reguse = useOfPsxReg(code, use, psxreg);
        
		  // return if we have found a use
        if (reguse != REGUSE_NONE)
				break;
        
		  // goto next instruction
        pc += 4;
		  i++;
		  
		  // check for code branches/jumps
		  if (i != numInstr) {
			  if ((use & REGUSE_TYPEM) == REGUSE_BRANCH) {
#ifndef NOREGUSE_FOLLOW
					// check delay slot
					reguse = _nextPsxRegUse(pc, psxreg, 1);
					if (reguse != REGUSE_NONE) break;
					
					bPC = _fImm_(code) * 4 + pc;
					reguse = _nextPsxRegUse(pc+4, psxreg, (numInstr-i-1)/2);
					if (reguse != REGUSE_NONE) {
						int reguse2 = _nextPsxRegUse(bPC, psxreg, (numInstr-i-1)/2);
						if (reguse2 != REGUSE_NONE)
							reguse |= reguse2;
						else
							reguse = REGUSE_NONE;
					}
#endif
					break;
			  } else if ((use & REGUSE_TYPEM) == REGUSE_JUMP) {
#ifndef NOREGUSE_FOLLOW
					// check delay slot
					reguse = _nextPsxRegUse(pc, psxreg, 1);
					if (reguse != REGUSE_NONE) break;
					
					bPC = _fTarget_(code) * 4 + (pc & 0xf0000000);
					reguse = _nextPsxRegUse(bPC, psxreg, numInstr-i-1);
#endif
					break;
			  } else if ((use & REGUSE_TYPEM) == REGUSE_JUMPR) {
#ifndef NOREGUSE_FOLLOW
					// jump to unknown location - bail after checking delay slot
					reguse = _nextPsxRegUse(pc, psxreg, 1);
#endif
					break;
			  } else if ((use & REGUSE_TYPEM) == REGUSE_SYS) {
					break;
			  }
		  }
    }
    
    return reguse;
}


int nextPsxRegUse(u32 pc, int psxreg)
{
#if 1
	if (psxreg == 0)
		return REGUSE_WRITE; // pretend we are writing to it to fool compiler

#ifdef SAME_CYCLE_MODE
	return REGUSE_READ;
#else
	return _nextPsxRegUse(pc, psxreg, 80);
#endif
#else
    u32 code, bPC = 0;
    int use, reguse = 0, reguse1 = 0, b = 0, i, index = 0;

retry:
    for (i=index; i<80; i++) {
        code = PSXMu32(pc);
    	use = getRegUse(code);
        reguse = useOfPsxReg(code, use, psxreg);
        
        if (reguse != REGUSE_NONE) break;
        
        pc += 4;
        if ((use & REGUSE_TYPEM) == REGUSE_BRANCH) {
            if (b == 0) {
                bPC = _fImm_(code) * 4 + pc;
                index = i+1;
            }
            b += 1; // TODO: follow branches
            continue;
        } else if ((use & REGUSE_TYPEM) == REGUSE_JUMP) {
            if (b == 0) {
                bPC = _fTarget_(code) * 4 + (pc & 0xf0000000);
            }
            b = 2;
            continue;       
        } else if ((use & REGUSE_TYPEM) == REGUSE_JUMPR || 
                   (use & REGUSE_TYPEM) == REGUSE_SYS) {
            b = 2;
            continue;       
        }
        
        if (b == 2 && bPC && index == 0) {
            pc = bPC; bPC = 0;
            b = 1;
        }
        if (b >= 2) break; // only follow 1 branch
    }
    if (reguse == REGUSE_NONE) return reguse;
    
    if (bPC) {
        reguse1 = reguse;
        pc = bPC; bPC = 0;
        b = 1;
        goto retry;
    }
    
    return reguse1 | reguse;
#endif
}

int isPsxRegUsed(u32 pc, int psxreg)
{
    int use = nextPsxRegUse(pc, psxreg);
    
    if (use == REGUSE_NONE)
        return 2; // unknown use - assume it is used
    else if (use & REGUSE_READ)
        return 1; // the next use is a read
    else
        return 0; // the next use is a write, i.e. current value is not important
}
