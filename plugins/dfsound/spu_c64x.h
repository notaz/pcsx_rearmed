#define COMPONENT_NAME "pcsxr_spu"

enum {
 CCMD_INIT = 0x101,
 CCMD_DOIT = 0x102,
};

struct region_mem {
 unsigned char spu_ram[512 * 1024];
 int RVB[NSSIZE * 2];
 int SSumLR[NSSIZE * 2];
 int SB[SB_SIZE * 24];
 // careful not to lose ARM writes by DSP overwriting
 // with old data when it's writing out neighbor cachelines
 int _pad1[128/4 - ((NSSIZE * 4 + SB_SIZE * 24) & (128/4 - 1))];
 SPUCHAN s_chan[24 + 1];
 int _pad2[128/4 - ((sizeof(SPUCHAN) * 25 / 4) & (128/4 - 1))];
 struct spu_worker worker;
 SPUConfig spu_config;
 // init/debug
 int sizeof_region_mem;
 int offsetof_s_chan1;
 int offsetof_worker_ram;
};

