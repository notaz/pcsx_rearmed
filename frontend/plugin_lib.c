/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include "linux/fbdev.h"

static struct vout_fbdev *fbdev;
void *pl_fbdev_buf;

int pl_fbdev_init(void)
{
  const char *fbdev_name;
  int w, h;

  fbdev_name = getenv("FBDEV");
  if (fbdev_name == NULL)
    fbdev_name = "/dev/fb0";

  w = 640;
  h = 512; // ??
  fbdev = vout_fbdev_init(fbdev_name, &w, &h, 16, 3);
  if (fbdev == NULL)
    return -1;

  pl_fbdev_buf = vout_fbdev_flip(fbdev);

  return 0;
}

int pl_fbdev_set_mode(int w, int h, int bpp)
{
  printf("set mode %dx%d@%d\n", w, h, bpp);
  return vout_fbdev_resize(fbdev, w, h, bpp, 0, 0, 0, 0, 3);
}

void *pl_fbdev_flip(void)
{
  pl_fbdev_buf = vout_fbdev_flip(fbdev);
  return pl_fbdev_buf;
}

void pl_fbdev_finish(void)
{
  if (fbdev != NULL)
    vout_fbdev_finish(fbdev);
  fbdev = NULL;
}

