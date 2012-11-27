#ifndef __PSXMEM_MAP_H__
#define __PSXMEM_MAP_H__

#ifdef __cplusplus
extern "C" {
#endif

enum psxMapTag {
	MAP_TAG_OTHER = 0,
	MAP_TAG_RAM,
	MAP_TAG_VRAM,
	MAP_TAG_LUTS,
};

extern void *(*psxMapHook)(unsigned long addr, size_t size, int is_fixed,
	enum psxMapTag tag);
extern void (*psxUnmapHook)(void *ptr, size_t size, enum psxMapTag tag);

void *psxMap(unsigned long addr, size_t size, int is_fixed,
		enum psxMapTag tag);
void psxUnmap(void *ptr, size_t size, enum psxMapTag tag);

#ifdef __cplusplus
}
#endif
#endif
