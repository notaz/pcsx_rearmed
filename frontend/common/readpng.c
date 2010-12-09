/*
 * (C) Gražvydas "notaz" Ignotas, 2008-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <png.h>
#include "readpng.h"
#include "lprintf.h"

int readpng(void *dest, const char *fname, readpng_what what, int req_w, int req_h)
{
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytepp row_ptr = NULL;
	int ret = -1;

	if (dest == NULL || fname == NULL)
	{
		return -1;
	}

	fp = fopen(fname, "rb");
	if (fp == NULL)
	{
		lprintf(__FILE__ ": failed to open: %s\n", fname);
		return -1;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		lprintf(__FILE__ ": png_create_read_struct() failed\n");
		fclose(fp);
		return -1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		lprintf(__FILE__ ": png_create_info_struct() failed\n");
		goto done;
	}

	// Start reading
	png_init_io(png_ptr, fp);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING, NULL);
	row_ptr = png_get_rows(png_ptr, info_ptr);
	if (row_ptr == NULL)
	{
		lprintf(__FILE__ ": png_get_rows() failed\n");
		goto done;
	}

	// lprintf("%s: %ix%i @ %ibpp\n", fname, (int)info_ptr->width, (int)info_ptr->height, info_ptr->pixel_depth);

	switch (what)
	{
		case READPNG_BG:
		{
			int height, width, h;
			unsigned short *dst = dest;
			if (info_ptr->pixel_depth != 24)
			{
				lprintf(__FILE__ ": bg image uses %ibpp, needed 24bpp\n", info_ptr->pixel_depth);
				break;
			}
			height = info_ptr->height;
			if (height > req_h)
				height = req_h;
			width = info_ptr->width;
			if (width > req_w)
				width = req_w;

			for (h = 0; h < height; h++)
			{
				unsigned char *src = row_ptr[h];
				int len = width;
				while (len--)
				{
#ifdef PSP
					*dst++ = ((src[2]&0xf8)<<8) | ((src[1]&0xf8)<<3) | (src[0] >> 3); // BGR
#else
					*dst++ = ((src[0]&0xf8)<<8) | ((src[1]&0xf8)<<3) | (src[2] >> 3); // RGB
#endif
					src += 3;
				}
				dst += req_w - width;
			}
			break;
		}

		case READPNG_FONT:
		{
			int x, y, x1, y1;
			unsigned char *dst = dest;
			if (info_ptr->width != req_w || info_ptr->height != req_h)
			{
				lprintf(__FILE__ ": unexpected font image size %dx%d, needed %dx%d\n",
					(int)info_ptr->width, (int)info_ptr->height, req_w, req_h);
				break;
			}
			if (info_ptr->pixel_depth != 8)
			{
				lprintf(__FILE__ ": font image uses %ibpp, needed 8bpp\n", info_ptr->pixel_depth);
				break;
			}
			for (y = 0; y < 16; y++)
			{
				for (x = 0; x < 16; x++)
				{
					/* 16x16 grid of syms */
					int sym_w = req_w / 16;
					int sym_h = req_h / 16;
					for (y1 = 0; y1 < sym_h; y1++)
					{
						unsigned char *src = row_ptr[y*sym_h + y1] + x*sym_w;
						for (x1 = sym_w/2; x1 > 0; x1--, src+=2)
							*dst++ = ((src[0]^0xff) & 0xf0) | ((src[1]^0xff) >> 4);
					}
				}
			}
			break;
		}

		case READPNG_SELECTOR:
		{
			int x1, y1;
			unsigned char *dst = dest;
			if (info_ptr->width != req_w || info_ptr->height != req_h)
			{
				lprintf(__FILE__ ": unexpected selector image size %ix%i, needed %dx%d\n",
					(int)info_ptr->width, (int)info_ptr->height, req_w, req_h);
				break;
			}
			if (info_ptr->pixel_depth != 8)
			{
				lprintf(__FILE__ ": selector image uses %ibpp, needed 8bpp\n", info_ptr->pixel_depth);
				break;
			}
			for (y1 = 0; y1 < req_h; y1++)
			{
				unsigned char *src = row_ptr[y1];
				for (x1 = req_w/2; x1 > 0; x1--, src+=2)
					*dst++ = ((src[0]^0xff) & 0xf0) | ((src[1]^0xff) >> 4);
			}
			break;
		}

		case READPNG_24:
		{
			int height, width, h;
			unsigned char *dst = dest;
			if (info_ptr->pixel_depth != 24)
			{
				lprintf(__FILE__ ": image uses %ibpp, needed 24bpp\n", info_ptr->pixel_depth);
				break;
			}
			height = info_ptr->height;
			if (height > req_h)
				height = req_h;
			width = info_ptr->width;
			if (width > req_w)
				width = req_w;

			for (h = 0; h < height; h++)
			{
				int len = width;
				unsigned char *src = row_ptr[h];
				dst += (req_w - width) * 3;
				for (len = width; len > 0; len--, dst+=3, src+=3)
					dst[0] = src[2], dst[1] = src[1], dst[2] = src[0];
			}
			break;
		}
	}


	ret = 0;
done:
	png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, (png_infopp)NULL);
	fclose(fp);
	return ret;
}


