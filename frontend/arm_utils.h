#ifdef __cplusplus
extern "C"
{
#endif

void bgr555_to_rgb565(void *dst, void *src, int bytes);
void bgr888_to_rgb888(void *dst, void *src, int bytes);
void bgr888_to_rgb565(void *dst, void *src, int bytes);

#ifdef __cplusplus
}
#endif
