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

#include "libpicofe/input.h"
#include "libpicofe/in_sdl.h"
#include "libpicofe/menu.h"
#include "plugin_lib.h"
#include "main.h"
#include "plat.h"
#include "revision.h"

static const struct in_default_bind in_sdl_defbinds[] = {
  { SDLK_UP,     IN_BINDTYPE_PLAYER12, DKEY_UP },
  { SDLK_DOWN,   IN_BINDTYPE_PLAYER12, DKEY_DOWN },
  { SDLK_LEFT,   IN_BINDTYPE_PLAYER12, DKEY_LEFT },
  { SDLK_RIGHT,  IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
  { SDLK_d,      IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
  { SDLK_z,      IN_BINDTYPE_PLAYER12, DKEY_CROSS },
  { SDLK_x,      IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
  { SDLK_s,      IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
  { SDLK_v,      IN_BINDTYPE_PLAYER12, DKEY_START },
  { SDLK_c,      IN_BINDTYPE_PLAYER12, DKEY_SELECT },
  { SDLK_w,      IN_BINDTYPE_PLAYER12, DKEY_L1 },
  { SDLK_r,      IN_BINDTYPE_PLAYER12, DKEY_R1 },
  { SDLK_e,      IN_BINDTYPE_PLAYER12, DKEY_L2 },
  { SDLK_t,      IN_BINDTYPE_PLAYER12, DKEY_R2 },
  { SDLK_ESCAPE, IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
  { SDLK_F1,     IN_BINDTYPE_EMU, SACTION_SAVE_STATE },
  { SDLK_F2,     IN_BINDTYPE_EMU, SACTION_LOAD_STATE },
  { SDLK_F3,     IN_BINDTYPE_EMU, SACTION_PREV_SSLOT },
  { SDLK_F4,     IN_BINDTYPE_EMU, SACTION_NEXT_SSLOT },
  { SDLK_F5,     IN_BINDTYPE_EMU, SACTION_TOGGLE_FSKIP },
  { SDLK_F6,     IN_BINDTYPE_EMU, SACTION_SCREENSHOT },
  { SDLK_F7,     IN_BINDTYPE_EMU, SACTION_FAST_FORWARD },
  { SDLK_F8,     IN_BINDTYPE_EMU, SACTION_SWITCH_DISPMODE },
  { 0, 0, 0 }
};

static SDL_Surface *screen;
static void *menubg_img;

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

  menubg_img = malloc(640 * 512 * 2);
  if (menubg_img == NULL)
    goto fail;

  in_sdl_init(in_sdl_defbinds);
  in_probe();
  pl_rearmed_cbs.only_16bpp = 1;
  return;

fail:
  SDL_Quit();
  exit(1);
}

void plat_finish(void)
{
  free(menubg_img);
  menubg_img = NULL;
  SDL_Quit();
}

void plat_gvideo_open(int is_pal)
{
}

void *plat_gvideo_set_mode(int *w, int *h, int *bpp)
{
  change_video_mode(*w, *h);
  return screen->pixels;
}

/* XXX: missing SDL_LockSurface() */
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
  /* surface will be lost, must adjust pl_vout_buf for menu bg */
  memcpy(menubg_img, screen->pixels, screen->w * screen->h * 2);
  pl_vout_buf = menubg_img;

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
