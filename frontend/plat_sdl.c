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
#include "menu.h"
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
  { SDLK_F7,     IN_BINDTYPE_EMU, SACTION_TOGGLE_FPS },
  { SDLK_F8,     IN_BINDTYPE_EMU, SACTION_SWITCH_DISPMODE },
  { SDLK_F11,    IN_BINDTYPE_EMU, SACTION_TOGGLE_FULLSCREEN },
  { SDLK_BACKSPACE, IN_BINDTYPE_EMU, SACTION_FAST_FORWARD },
  { 0, 0, 0 }
};

// XXX: maybe determine this instead..
#define WM_DECORATION_H 32

static SDL_Surface *screen;
static SDL_Overlay *overlay;
static int window_w, window_h;
static int fs_w, fs_h;
static int psx_w, psx_h;
static void *menubg_img;
static int in_menu, pending_resize, old_fullscreen;

static int change_video_mode(int w, int h)
{
  psx_w = w;
  psx_h = h;

  if (overlay != NULL) {
    SDL_FreeYUVOverlay(overlay);
    overlay = NULL;
  }

  if (g_use_overlay && !in_menu) {
    Uint32 flags = SDL_RESIZABLE;
    int win_w = window_w;
    int win_h = window_h;

    if (g_fullscreen) {
      flags |= SDL_FULLSCREEN;
      win_w = fs_w;
      win_h = fs_h;
    }

    screen = SDL_SetVideoMode(win_w, win_h, 0, flags);
    if (screen == NULL) {
      fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
      return -1;
    }

    overlay = SDL_CreateYUVOverlay(w, h, SDL_UYVY_OVERLAY, screen);
    if (overlay != NULL) {
      /*printf("overlay: fmt %x, planes: %d, pitch: %d, hw: %d\n",
        overlay->format, overlay->planes, *overlay->pitches,
        overlay->hw_overlay);*/

      if ((long)overlay->pixels[0] & 3)
        fprintf(stderr, "warning: overlay pointer is unaligned\n");
      if (!overlay->hw_overlay)
        fprintf(stderr, "warning: video overlay is not hardware accelerated,"
                        " you may want to disable it.\n");
    }
    else {
      fprintf(stderr, "warning: could not create overlay.\n");
    }
  }

  if (overlay == NULL) {
    screen = SDL_SetVideoMode(w, h, 16, 0);
    if (screen == NULL) {
      fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
      return -1;
    }

    if (!in_menu) {
      window_w = screen->w;
      window_h = screen->h;
    }
  }

  old_fullscreen = g_fullscreen;
  return 0;
}

static void event_handler(void *event_)
{
  SDL_Event *event = event_;

  if (event->type == SDL_VIDEORESIZE) {
    //printf("%dx%d\n", event->resize.w, event->resize.h);
    if (overlay != NULL && !g_fullscreen && !old_fullscreen) {
      window_w = event->resize.w;
      window_h = event->resize.h;
      pending_resize = 1;
    }
  }
}

void plat_init(void)
{
  const SDL_VideoInfo *info;
  int ret, h;

  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
  if (ret != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    exit(1);
  }

  info = SDL_GetVideoInfo();
  if (info != NULL) {
    fs_w = info->current_w;
    fs_h = info->current_h;
  }

  in_menu = 1;
  g_menuscreen_w = 640;
  if (fs_w != 0 && g_menuscreen_w > fs_w)
    g_menuscreen_w = fs_w;
  g_menuscreen_h = 480;
  if (fs_h != 0) {
    h = fs_h;
    if (info && info->wm_available && h > WM_DECORATION_H)
      h -= WM_DECORATION_H;
    if (g_menuscreen_h > h)
      g_menuscreen_h = h;
  }

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
  }
  g_menuscreen_w = window_w = screen->w;
  g_menuscreen_h = window_h = screen->h;

  SDL_WM_SetCaption("PCSX-ReARMed " REV, NULL);

  menubg_img = malloc(640 * 512 * 2);
  if (menubg_img == NULL)
    goto fail;

  in_sdl_init(in_sdl_defbinds, event_handler);
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

static void test_convert(void *d, const void *s, int pixels)
{
  unsigned int *dst = d;
  const unsigned short *src = s;
  int r0, g0, b0, r1, g1, b1;
  int y0, y1, u, v;

  for (; pixels > 0; src += 2, dst++, pixels -= 2) {
    r0 =  src[0] >> 11;
    g0 = (src[0] >> 6) & 0x1f;
    b0 =  src[0] & 0x1f;
    r1 =  src[1] >> 11;
    g1 = (src[1] >> 6) & 0x1f;
    b1 =  src[1] & 0x1f;
    y0 = (int)((0.299f * r0) + (0.587f * g0) + (0.114f * b0));
    y1 = (int)((0.299f * r1) + (0.587f * g1) + (0.114f * b1));
    //u = (int)(((-0.169f * r0) + (-0.331f * g0) + ( 0.499f * b0)) * 8) + 128;
    //v = (int)((( 0.499f * r0) + (-0.418f * g0) + (-0.0813f * b0)) * 8) + 128;
    u = (int)(8 * 0.565f * (b0 - y0)) + 128;
    v = (int)(8 * 0.713f * (r0 - y0)) + 128;
    // valid Y range seems to be 16..235
    y0 = 16 + 219 * y0 / 31;
    y1 = 16 + 219 * y1 / 31;

    if (y0 < 0 || y0 > 255 || y1 < 0 || y1 > 255
        || u < 0 || u > 255 || v < 0 || v > 255)
    {
      printf("oor: %d, %d, %d, %d\n", y0, y1, u, v);
    }
    *dst = (y1 << 24) | (v << 16) | (y0 << 8) | u;
  }
}

/* XXX: missing SDL_LockSurface() */
void *plat_gvideo_flip(void)
{
  if (!in_menu && overlay != NULL) {
    SDL_Rect dstrect = { 0, 0, screen->w, screen->h };
    SDL_LockYUVOverlay(overlay);
    test_convert(overlay->pixels[0], screen->pixels, overlay->w * overlay->h);
    SDL_UnlockYUVOverlay(overlay);
    SDL_DisplayYUVOverlay(overlay, &dstrect);
  }
  else
    SDL_Flip(screen);

  if (pending_resize || g_fullscreen != old_fullscreen) {
    // must be done here so that correct buffer is returned
    change_video_mode(psx_w, psx_h);
    pending_resize = 0;
  }

  return screen->pixels;
}

void plat_gvideo_close(void)
{
}

void plat_video_menu_enter(int is_rom_loaded)
{
  in_menu = 1;

  /* surface will be lost, must adjust pl_vout_buf for menu bg */
  // FIXME?
  memcpy(menubg_img, screen->pixels, psx_w * psx_h * 2);
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
  in_menu = 0;
}

/* unused stuff */
void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
  return 0;
}

void plat_trigger_vibrate(int is_strong)
{
}

void plat_minimize(void)
{
}

// vim:shiftwidth=2:expandtab
