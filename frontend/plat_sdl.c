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
#include "libpicofe/fonts.h"
#include "../plugins/gpulib/cspace.h"
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
static int in_menu, old_fullscreen;

static void overlay_clear(void);

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

      if (!overlay->hw_overlay) {
        fprintf(stderr, "warning: video overlay is not hardware accelerated, "
                        "disabling it.\n");
        g_use_overlay = 0;
        SDL_FreeYUVOverlay(overlay);
        overlay = NULL;
      }
      else
        overlay_clear();
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
      change_video_mode(psx_w, psx_h);
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

  bgr_to_uyvy_init();
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

static void uyvy_to_rgb565(void *d, const void *s, int pixels)
{
  unsigned short *dst = d;
  const unsigned int *src = s;
  int v;

  // no colors, for now
  for (; pixels > 0; src++, dst += 2, pixels -= 2) {
    v = (*src >> 8) & 0xff;
    v = (v - 16) * 255 / 219 / 8;
    dst[0] = (v << 11) | (v << 6) | v;

    v = (*src >> 24) & 0xff;
    v = (v - 16) * 255 / 219 / 8;
    dst[1] = (v << 11) | (v << 6) | v;
  }
}

static void overlay_clear(void)
{
  int pixels = overlay->w * overlay->h;
  int *dst = (int *)overlay->pixels[0];
  int v = 0x10801080;

  for (; pixels > 0; dst += 4, pixels -= 2 * 4)
    dst[0] = dst[1] = dst[2] = dst[3] = v;

  for (; pixels > 0; dst++, pixels -= 2)
    *dst = v;
}

static void overlay_blit(int doffs, const void *src_, int w, int h,
                         int sstride, int bgr24)
{
  const unsigned short *src = src_;
  unsigned short *dst;
  int dstride = overlay->w;

  SDL_LockYUVOverlay(overlay);
  dst = (void *)overlay->pixels[0];

  dst += doffs;
  if (bgr24) {
    for (; h > 0; dst += dstride, src += sstride, h--)
      bgr888_to_uyvy(dst, src, w);
  }
  else {
    for (; h > 0; dst += dstride, src += sstride, h--)
      bgr555_to_uyvy(dst, src, w);
  }

  SDL_UnlockYUVOverlay(overlay);
}

static void overlay_hud_print(int x, int y, const char *str, int bpp)
{
  SDL_LockYUVOverlay(overlay);
  basic_text_out_uyvy_nf(overlay->pixels[0], overlay->w, x, y, str);
  SDL_UnlockYUVOverlay(overlay);
}

void *plat_gvideo_set_mode(int *w, int *h, int *bpp)
{
  change_video_mode(*w, *h);
  if (overlay != NULL) {
    pl_plat_clear = overlay_clear;
    pl_plat_blit = overlay_blit;
    pl_plat_hud_print = overlay_hud_print;
    return NULL;
  }
  else {
    pl_plat_clear = NULL;
    pl_plat_blit = NULL;
    pl_plat_hud_print = NULL;
    return screen->pixels;
  }
}

void *plat_gvideo_flip(void)
{
  if (!in_menu && overlay != NULL) {
    SDL_Rect dstrect = { 0, 0, screen->w, screen->h };
    SDL_DisplayYUVOverlay(overlay, &dstrect);
    return NULL;
  }
  else {
    // XXX: missing SDL_LockSurface()
    SDL_Flip(screen);
    return screen->pixels;
  }
}

void plat_gvideo_close(void)
{
}

void plat_video_menu_enter(int is_rom_loaded)
{
  in_menu = 1;

  /* surface will be lost, must adjust pl_vout_buf for menu bg */
  if (overlay != NULL)
    uyvy_to_rgb565(menubg_img, overlay->pixels[0], psx_w * psx_h);
  else
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
