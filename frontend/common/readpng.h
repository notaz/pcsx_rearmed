typedef enum
{
	READPNG_BG = 1,
	READPNG_FONT,
	READPNG_SELECTOR,
	READPNG_24,
}
readpng_what;

#ifdef __cplusplus
extern "C" {
#endif

int readpng(void *dest, const char *fname, readpng_what what, int w, int h);

#ifdef __cplusplus
}
#endif
