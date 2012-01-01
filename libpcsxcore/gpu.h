
/*
 * q: Why bother with GPU stuff in a plugin-based emu core?
 * a: mostly because of busy bits, we have all the needed timing info
 *    that GPU plugin doesn't.
 */

#define PSXGPU_LCF     (1<<31)
#define PSXGPU_nBUSY   (1<<26)
#define PSXGPU_ILACE   (1<<22)
#define PSXGPU_DHEIGHT (1<<19)

// both must be set for interlace to work
#define PSXGPU_ILACE_BITS (PSXGPU_ILACE | PSXGPU_DHEIGHT)

#define HW_GPU_STATUS psxHu32ref(0x1814)

// TODO: handle com too
#define PSXGPU_TIMING_BITS (PSXGPU_LCF | PSXGPU_nBUSY)

#define gpuSyncPluginSR() { \
	HW_GPU_STATUS &= PSXGPU_TIMING_BITS; \
	HW_GPU_STATUS |= GPU_readStatus() & ~PSXGPU_TIMING_BITS; \
}
