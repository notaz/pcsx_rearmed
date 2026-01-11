#ifndef __GPU_UNAI_GPU_ARM_H__
#define __GPU_UNAI_GPU_ARM_H__

#ifdef __cplusplus
extern "C" {
#endif

struct gpu_unai_inner_t;

void gpu_fill_asm(void *d, u32 rgbx2, u32 w, u32 h);

void tile_driver_st0_asm(void *d, u16 c, u32 cnt, const struct gpu_unai_inner_t *inn);
void tile_driver_st1_asm(void *d, u16 c, u32 cnt, const struct gpu_unai_inner_t *inn);
void tile_driver_st3_asm(void *d, u16 c, u32 cnt, const struct gpu_unai_inner_t *inn);

void sprite_driver_4bpp_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_16bpp_asm(void *pPixel, const void *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_4bpp_x16_asm(void *d, const void *s, void *pal, int lines);

void sprite_driver_4bpp_l0_std_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_4bpp_l0_st0_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_l0_std_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_l0_st0_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);

void poly_utx_l0d0m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_utx_l0d0m0st1_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_utx_l0d0m0st3_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_l0d0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_l0d0m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_8bp_l0d0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_8bp_l0d0m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count);

#ifdef HAVE_ARMV6

void tile_driver_st2_asm(void *d, u16 c, u32 cnt, const struct gpu_unai_inner_t *inn);

void sprite_driver_4bpp_l1_std_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_4bpp_l1_st0_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_4bpp_l1_st1_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_l1_std_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_l1_st0_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);
void sprite_driver_8bpp_l1_st1_asm(void *pPixel, const u8 *pTxt_base,
	u32 count, const struct gpu_unai_inner_t *inn);

void poly_utx_l0d0m0st2_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_utx_g1d0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_utx_g1d1m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_utx_g1d1m1std_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_4bp_l1d0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_l1d0m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_l1d1m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_4bp_l1d1m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_4bp_lgd0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_lgd0m0st1_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_4bp_lgd1m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_4bp_lgd1m0st1_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_8bp_l1d0m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_8bp_l1d0m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count);
void poly_8bp_l1d1m0std_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);
void poly_8bp_l1d1m0st0_asm(void *d, const struct gpu_unai_inner_t *inn, int count, u32 dv);

#endif // HAVE_ARMV6

#ifdef __cplusplus
}
#endif

#endif /* __GPU_UNAI_GPU_ARM_H__ */
