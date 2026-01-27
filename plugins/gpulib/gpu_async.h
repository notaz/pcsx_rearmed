#ifndef __GPULIB_GPU_ASYNC_H__
#define __GPULIB_GPU_ASYNC_H__

#include <stdint.h>

struct psx_gpu;
struct psx_gpu_async;

#define AGPU_DMA_MAX 4096 // words

#ifdef USE_ASYNC_GPU

#define gpu_async_enabled(gpu) ((gpu)->async)

int gpu_async_do_cmd_list(struct psx_gpu *gpu, const uint32_t *list, int list_len,
      int *cycles_sum_out, int *cycles_last, int *last_cmd, int *vram_dirty);
int gpu_async_try_dma(struct psx_gpu *gpu, const uint32_t *data, int words);
void gpu_async_start(struct psx_gpu *gpu);
void gpu_async_stop(struct psx_gpu *gpu);
void gpu_async_sync(struct psx_gpu *gpu);
int  gpu_async_sync_scanout(struct psx_gpu *gpu);
void gpu_async_sync_ecmds(struct psx_gpu *gpu);
void gpu_async_try_delayed_flip(struct psx_gpu *gpu, int force);
void gpu_async_notify_screen_change(struct psx_gpu *gpu);
void gpu_async_set_interlace(struct psx_gpu *gpu, int enable, int is_odd);

#else

#define gpu_async_enabled(gpu) 0
#define gpu_async_do_cmd_list(gpu, list, list_len, c0, c1, cmd, vrd) (list_len)
#define gpu_async_try_dma(gpu, data, words) 0
#define gpu_async_start(gpu)
#define gpu_async_stop(gpu)
#define gpu_async_sync(gpu) do {} while (0)
#define gpu_async_sync_scanout(gpu) 0
#define gpu_async_sync_ecmds(gpu)
#define gpu_async_try_delayed_flip(gpu, force)
#define gpu_async_notify_screen_change(gpu)
#define gpu_async_set_interlace(gpu, enable, is_odd)

#endif

#endif // __GPULIB_GPU_ASYNC_H__
