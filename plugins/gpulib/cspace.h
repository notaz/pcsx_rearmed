#ifdef __cplusplus
extern "C"
{
#endif

void bgr555_to_rgb565(void *dst, const void *src, int bytes);
void bgr888_to_rgb888(void *dst, const void *src, int bytes);
void bgr888_to_rgb565(void *dst, const void *src, int bytes);
void rgb888_to_rgb565(void *dst, const void *src, int bytes);

#ifdef __cplusplus
}
#endif
