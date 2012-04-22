/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2012
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <SDL.h>
#include "common/input.h"
#include "common/in_sdl.h"
#include "common/menu.h"
#include "plat.h"
#include "revision.h"

// XXX
struct in_default_bind in_evdev_defbinds[] = {
	{ 0, 0, 0 }
};

static SDL_Surface *screen;

static int change_video_mode(int w, int h)
{
  screen = SDL_SetVideoMode(w, h, 16, 0);
  if (screen == NULL) {
    fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}

void plat_init(void)
{
  int ret;

  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
  if (ret != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    exit(1);
  }

  g_menuscreen_w = 640;
  g_menuscreen_h = 480;
  ret = change_video_mode(g_menuscreen_w, g_menuscreen_h);
  if (ret != 0) {
    ret = change_video_mode(0, 0);
    if (ret != 0)
      goto fail;

    if (screen->w < 320 || screen->h < 240) {
      fprintf(stderr, "resolution %dx%d is too small, sorry.\n",
              screen->w, screen->h);
      goto fail;
    }
    g_menuscreen_w = screen->w;
    g_menuscreen_h = screen->h;
  }
  SDL_WM_SetCaption("PCSX-ReARMed " REV, NULL);

  in_sdl_init();
  in_probe();
  return;

fail:
  SDL_Quit();
  exit(1);
}

void plat_finish(void)
{
  SDL_Quit();
}

void plat_gvideo_open(void)
{
}

void *plat_gvideo_set_mode(int *w, int *h, int *bpp)
{
  change_video_mode(*w, *h);
  return screen->pixels;
}

void *plat_gvideo_flip(void)
{
  SDL_Flip(screen);
  return screen->pixels;
}

void plat_gvideo_close(void)
{
}

void plat_video_menu_enter(int is_rom_loaded)
{
  change_video_mode(g_menuscreen_w, g_menuscreen_h);
}

void plat_video_menu_begin(void)
{
  SDL_LockSurface(screen);
  g_menuscreen_ptr = screen->pixels;
}

void plat_video_menu_end(void)
{
  SDL_UnlockSurface(screen);
  SDL_Flip(screen);
  g_menuscreen_ptr = NULL;
}

void plat_video_menu_leave(void)
{
}

/* unused stuff */
void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
  return 0;
}

int plat_cpu_clock_get(void)
{
  return -1;
}

int plat_cpu_clock_apply(int cpu_clock)
{
  return -1;
}

int plat_get_bat_capacity(void)
{
  return -1;
}

void plat_step_volume(int is_up)
{
}

void plat_trigger_vibrate(int is_strong)
{
}

void plat_minimize(void)
{
}

// vim:shiftwidth=2:expandtab
