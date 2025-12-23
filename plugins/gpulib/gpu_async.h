#ifndef __GPULIB_GPU_ASYNC_H__
#define __GPULIB_GPU_ASYNC_H__

#include <stdint.h>

struct psx_gpu;
struct psx_gpu_async;

#ifdef USE_ASYNC_GPU

#define gpu_async_enabled(gpu) ((gpu)->async)

int gpu_async_do_cmd_list(struct psx_gpu *gpu, uint32_t *list, int list_len,
 int *cycles_sum_out, int *cycles_last, int *last_cmd);
void gpu_async_start(struct psx_gpu *gpu);
void gpu_async_stop(struct psx_gpu *gpu);
void gpu_async_sync(struct psx_gpu *gpu);
void gpu_async_sync_scanout(struct psx_gpu *gpu);
void gpu_async_sync_ecmds(struct psx_gpu *gpu);
void gpu_async_notify_screen_change(struct psx_gpu *gpu);

#else

#define gpu_async_enabled(gpu) 0
#define gpu_async_do_cmd_list(gpu, list, list_len, c0, c1, cmd) (list_len)
#define gpu_async_start(gpu)
#define gpu_async_stop(gpu)
#define gpu_async_sync(gpu) do {} while (0)
#define gpu_async_sync_scanout(gpu) do {} while (0)
#define gpu_async_sync_ecmds(gpu)
#define gpu_async_notify_screen_change(gpu)

#endif

#endif // __GPULIB_GPU_ASYNC_H__
