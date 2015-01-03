
struct region_mem {
 unsigned char spu_ram[512 * 1024];
 int RVB[NSSIZE * 2];
 int SSumLR[NSSIZE * 2];
 SPUCHAN s_chan[24 + 1];
 struct spu_worker worker;
};

