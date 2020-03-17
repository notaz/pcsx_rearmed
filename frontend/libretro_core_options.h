#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition
 */

struct retro_core_option_definition option_defs_us[] = {
   {
      "pcsx_rearmed_frameskip",
      "Frameskip",
      "Choose how much frames should be skipped to improve performance at the expense of visual smoothness.",
      {
         { "0", NULL },
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { NULL, NULL },
      },
      "0",
   },
   {
      "pcsx_rearmed_bios",
      "Use BIOS",
      "Allows you to use real bios file (if available) or emulated bios (HLE). Its recommended to use official bios file for better compatibility.",
      {
         { "auto", "auto" },
         { "HLE",  "hle" },
         { NULL, NULL },
      },
      "auto",
   },
   {
      "pcsx_rearmed_region",
      "Region",
      "Choose what region the system is from. 60 Hz for NTSC, 50 Hz for PAL.",
      {
         { "auto", "auto" },
         { "NTSC", "ntsc" },
         { "PAL",  "pal" },
         { NULL, NULL },
      },
      "auto",
   },
   {
      "pcsx_rearmed_memcard2",
      "Enable Second Memory Card (Shared)",
      "Enabled the memory card slot 2. This memory card is shared amongst all games.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_pad1type",
      "Pad 1 Type",
      "Pad type for player 1",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "standard",
   },
   {
      "pcsx_rearmed_pad2type",
      "Pad 2 Type",
      "Pad type for player 2",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "standard",
   },
   {
      "pcsx_rearmed_pad3type",
      "Pad 3 Type",
      "Pad type for player 3",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },
   {
      "pcsx_rearmed_pad4type",
      "Pad 4 Type",
      "Pad type for player 4",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },
   {
      "pcsx_rearmed_pad5type",
      "Pad 5 Type",
      "Pad type for player 5",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },{
      "pcsx_rearmed_pad6type",
      "Pad 6 Type",
      "Pad type for player 6",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },{
      "pcsx_rearmed_pad7type",
      "Pad 7 Type",
      "Pad type for player 7",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },{
      "pcsx_rearmed_pad8type",
      "Pad 8 Type",
      "Pad type for player 8",
      {
         { "standard",  NULL },
         { "analog",    NULL },
         { "dualshock", NULL },
         { "negcon",    NULL },
         { "guncon",    NULL },
         { "none",      NULL },
         { NULL, NULL },
      },
      "none",
   },
   {
      "pcsx_rearmed_multitap1",
      "Multitap 1",
      "Enables/Disables multitap on port 1, allowing upto 5 players in games that permit it.",
      {
         { "auto",     NULL },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "auto",
   },
   {
      "pcsx_rearmed_multitap2",
      "Multitap 2",
      "Enables/Disables multitap on port 2, allowing up to 8 players in games that permit it. Multitap 1 has to be enabled for this to work.",
      {
         { "auto",     NULL },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "auto",
   },
   {
      "pcsx_rearmed_negcon_deadzone",
      "NegCon Twist Deadzone (Percent)",
      "Sets the deadzone of the RetroPad left analog stick when simulating the 'twist' action of emulated neGcon Controllers. Used to eliminate drift/unwanted input.",
      {
         { "0",  NULL },
         { "5",  NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { NULL, NULL },
      },
      "0",
   },
   {
      "pcsx_rearmed_negcon_response",
      "NegCon Twist Response",
      "Specifies the analog response when using a RetroPad left analog stick to simulate the 'twist' action of emulated neGcon Controllers.",
      {
         { "linear",    NULL },
         { "quadratic", NULL },
         { "cubic",     NULL },
         { NULL, NULL },
      },
      "linear",
   },
   {
      "pcsx_rearmed_analog_axis_modifier",
      "Analog axis bounds.",
      "Range bounds for analog axis. Square bounds help controllers with highly circular ranges that are unable to fully saturate the x and y axis at 45degree deflections.",
      {
         { "circle", NULL },
         { "square", NULL },
         { NULL, NULL },
      },
      "circle",
   },
   {
      "pcsx_rearmed_vibration",
      "Enable Vibration",
      "Enables vibration feedback for controllers that supports vibration features.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gunconadjustx",
      "Guncon Adjust X",
      "When using Guncon mode, you can override aim in emulator if shots misaligned, this applies an increment on the x axis.",
      {
         { "0", NULL },
         { "-25", NULL },
         { "-24", NULL },
         { "-23", NULL },
         { "-22", NULL },
         { "-21", NULL },
         { "-20", NULL },
         { "-19", NULL },
         { "-18", NULL },
         { "-17", NULL },
         { "-16", NULL },
         { "-15", NULL },
         { "-14", NULL },
         { "-13", NULL },
         { "-12", NULL },
         { "-11", NULL },
         { "-10", NULL },
         { "-09", NULL },
         { "-08", NULL },
         { "-07", NULL },
         { "-06", NULL },
         { "-05", NULL },
         { "-04", NULL },
         { "-03", NULL },
         { "-02", NULL },
         { "-01", NULL },
         { "00", NULL },
         { "01", NULL },
         { "02", NULL },
         { "03", NULL },
         { "04", NULL },
         { "05", NULL },
         { "06", NULL },
         { "07", NULL },
         { "08", NULL },
         { "09", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { "17", NULL },
         { "18", NULL },
         { "19", NULL },
         { "20", NULL },
         { "21", NULL },
         { "22", NULL },
         { "23", NULL },
         { "24", NULL },
         { "25", NULL },
         { NULL, NULL },
      },
      "0",
   },
   {
      "pcsx_rearmed_gunconadjusty",
      "Guncon Adjust Y",
      "When using Guncon mode, you can override aim in emulator if shots misaligned, this applies an increment on the y axis.",
      {
         { "0", NULL },
         { "-25", NULL },
         { "-24", NULL },
         { "-23", NULL },
         { "-22", NULL },
         { "-21", NULL },
         { "-20", NULL },
         { "-19", NULL },
         { "-18", NULL },
         { "-17", NULL },
         { "-16", NULL },
         { "-15", NULL },
         { "-14", NULL },
         { "-13", NULL },
         { "-12", NULL },
         { "-11", NULL },
         { "-10", NULL },
         { "-09", NULL },
         { "-08", NULL },
         { "-07", NULL },
         { "-06", NULL },
         { "-05", NULL },
         { "-04", NULL },
         { "-03", NULL },
         { "-02", NULL },
         { "-01", NULL },
         { "00", NULL },
         { "01", NULL },
         { "02", NULL },
         { "03", NULL },
         { "04", NULL },
         { "05", NULL },
         { "06", NULL },
         { "07", NULL },
         { "08", NULL },
         { "09", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { "17", NULL },
         { "18", NULL },
         { "19", NULL },
         { "20", NULL },
         { "21", NULL },
         { "22", NULL },
         { "23", NULL },
         { "24", NULL },
         { "25", NULL },
         { NULL, NULL },
      },
      "0",
   },
   {
      "pcsx_rearmed_gunconadjustratiox",
      "Guncon Adjust Ratio X",
      "When using Guncon mode, you can override aim in emulator if shots misaligned, this applies a ratio on the x axis.",
      {
         { "1", NULL },
         { "0.75", NULL },
         { "0.76", NULL },
         { "0.77", NULL },
         { "0.78", NULL },
         { "0.79", NULL },
         { "0.80", NULL },
         { "0.81", NULL },
         { "0.82", NULL },
         { "0.83", NULL },
         { "0.84", NULL },
         { "0.85", NULL },
         { "0.86", NULL },
         { "0.87", NULL },
         { "0.88", NULL },
         { "0.89", NULL },
         { "0.90", NULL },
         { "0.91", NULL },
         { "0.92", NULL },
         { "0.93", NULL },
         { "0.94", NULL },
         { "0.95", NULL },
         { "0.96", NULL },
         { "0.97", NULL },
         { "0.98", NULL },
         { "0.99", NULL },
         { "1.00", NULL },
         { "1.01", NULL },
         { "1.02", NULL },
         { "1.03", NULL },
         { "1.04", NULL },
         { "1.05", NULL },
         { "1.06", NULL },
         { "1.07", NULL },
         { "1.08", NULL },
         { "1.09", NULL },
         { "1.10", NULL },
         { "1.11", NULL },
         { "1.12", NULL },
         { "1.13", NULL },
         { "1.14", NULL },
         { "1.15", NULL },
         { "1.16", NULL },
         { "1.17", NULL },
         { "1.18", NULL },
         { "1.19", NULL },
         { "1.20", NULL },
         { "1.21", NULL },
         { "1.22", NULL },
         { "1.23", NULL },
         { "1.24", NULL },
         { "1.25", NULL },
         { NULL, NULL },
      },
      "1",
   },
   {
      "pcsx_rearmed_gunconadjustratioy",
      "Guncon Adjust Ratio Y",
      "When using Guncon mode, you can override aim in emulator if shots misaligned, this applies a ratio on the y axis.",
      {
         { "1", NULL },
         { "0.75", NULL },
         { "0.76", NULL },
         { "0.77", NULL },
         { "0.78", NULL },
         { "0.79", NULL },
         { "0.80", NULL },
         { "0.81", NULL },
         { "0.82", NULL },
         { "0.83", NULL },
         { "0.84", NULL },
         { "0.85", NULL },
         { "0.86", NULL },
         { "0.87", NULL },
         { "0.88", NULL },
         { "0.89", NULL },
         { "0.90", NULL },
         { "0.91", NULL },
         { "0.92", NULL },
         { "0.93", NULL },
         { "0.94", NULL },
         { "0.95", NULL },
         { "0.96", NULL },
         { "0.97", NULL },
         { "0.98", NULL },
         { "0.99", NULL },
         { "1.00", NULL },
         { "1.01", NULL },
         { "1.02", NULL },
         { "1.03", NULL },
         { "1.04", NULL },
         { "1.05", NULL },
         { "1.06", NULL },
         { "1.07", NULL },
         { "1.08", NULL },
         { "1.09", NULL },
         { "1.10", NULL },
         { "1.11", NULL },
         { "1.12", NULL },
         { "1.13", NULL },
         { "1.14", NULL },
         { "1.15", NULL },
         { "1.16", NULL },
         { "1.17", NULL },
         { "1.18", NULL },
         { "1.19", NULL },
         { "1.20", NULL },
         { "1.21", NULL },
         { "1.22", NULL },
         { "1.23", NULL },
         { "1.24", NULL },
         { "1.25", NULL },
         { NULL, NULL },
      },
      "1",
   },
   {
      "pcsx_rearmed_dithering",
      "Enable Dithering",
      "If Off, disables the dithering pattern the PSX applies to combat color banding.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
#if defined HAVE_LIBNX || defined _3DS
	  "disabled",
#else
      "enabled",
#endif
   },

#ifndef DRC_DISABLE
   {
      "pcsx_rearmed_drc",
      "Dynamic Recompiler",
      "Enables core to use dynamic recompiler or interpreter (slower) CPU instructions.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_psxclock",
      "PSX CPU Clock",
#ifdef HAVE_PRE_ARMV7
      "Overclock or underclock the PSX clock. Default is 50",
#else
      "Overclock or underclock the PSX clock. Default is 57",
#endif
      {
         { "30",  NULL },
         { "31",  NULL },
         { "32",  NULL },
         { "33",  NULL },
         { "34",  NULL },
         { "35",  NULL },
         { "36",  NULL },
         { "37",  NULL },
         { "38",  NULL },
         { "39",  NULL },
         { "40",  NULL },
         { "41",  NULL },
         { "42",  NULL },
         { "43",  NULL },
         { "44",  NULL },
         { "45",  NULL },
         { "46",  NULL },
         { "47",  NULL },
         { "48",  NULL },
         { "49",  NULL },
         { "50",  NULL },
         { "51",  NULL },
         { "52",  NULL },
         { "53",  NULL },
         { "54",  NULL },
         { "55",  NULL },
         { "56",  NULL },
         { "57",  NULL },
         { "58",  NULL },
         { "59",  NULL },
         { "60",  NULL },
         { "61",  NULL },
         { "62",  NULL },
         { "63",  NULL },
         { "64",  NULL },
         { "65",  NULL },
         { "66",  NULL },
         { "67",  NULL },
         { "68",  NULL },
         { "69",  NULL },
         { "70",  NULL },
         { "71",  NULL },
         { "72",  NULL },
         { "73",  NULL },
         { "74",  NULL },
         { "75",  NULL },
         { "76",  NULL },
         { "77",  NULL },
         { "78",  NULL },
         { "79",  NULL },
         { "80",  NULL },
         { "81",  NULL },
         { "82",  NULL },
         { "83",  NULL },
         { "84",  NULL },
         { "85",  NULL },
         { "86",  NULL },
         { "87",  NULL },
         { "88",  NULL },
         { "89",  NULL },
         { "90",  NULL },
         { "91",  NULL },
         { "92",  NULL },
         { "93",  NULL },
         { "94",  NULL },
         { "95",  NULL },
         { "96",  NULL },
         { "97",  NULL },
         { "98",  NULL },
         { "99",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
#ifdef HAVE_PRE_ARMV7
      "50",
#else
      "57",
#endif
   },
#endif /* DRC_DISABLE */

#ifdef GPU_NEON
   {
      "pcsx_rearmed_neon_interlace_enable",
      "Enable Interlacing Mode",
      "Enables fake scanlines effect.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_neon_enhancement_enable",
      "Enhanced Resolution (Slow)",
      "Renders in double resolution at the cost of lower performance.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_neon_enhancement_no_main",
      "Enhanced Resolution (Speed Hack)",
      "Speed hack for Enhanced resolution option (glitches some games).",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
#endif /* GPU_NEON */

   {
      "pcsx_rearmed_duping_enable",
      "Frame Duping",
      "A speedup, redraws/reuses the last frame if there was no new data.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_display_internal_fps",
      "Display Internal FPS",
      "Shows an on-screen frames per second counter when enabled.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },

   /* GPU PEOPS OPTIONS */
#ifdef GPU_PEOPS
   {
      "pcsx_rearmed_show_gpu_peops_settings",
      "Advanced GPU P.E.Op.S. Settings",
      "Shows or hides advanced GPU plugin settings. NOTE: Quick Menu must be toggled for this setting to take effect.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_odd_even_bit",
      "(GPU) Odd/Even Bit Hack",
      "Needed for Chrono Cross.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_expand_screen_width",
      "(GPU) Expand Screen Width",
      "Capcom fighting games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_ignore_brightness",
      "(GPU) Ignore Brightness Color",
      "Black screens in Lunar Silver Star Story games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_disable_coord_check",
      "(GPU) Disable Coordinate Check",
      "Compatibility mode",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_lazy_screen_update",
      "(GPU) Lazy Screen Update",
      "Pandemonium 2",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_old_frame_skip",
      "(GPU) Old Frame Skipping",
      "Skip every second frame",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gpu_peops_repeated_triangles",
      "(GPU) Repeated Flat Tex Triangles",
      "Needed by Star Wars: Dark Forces",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_quads_with_triangles",
      "(GPU) Draw Quads with Triangles",
      "Better g-colors, worse textures",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fake_busy_state",
      "(GPU) Fake 'Gpu Busy' States",
      "Toggle busy flags after drawing",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
#endif

    /* GPU UNAI Advanced Options */
#ifdef GPU_UNAI
   {
      "pcsx_rearmed_show_gpu_unai_settings",
      "Advance GPU UNAI/PCSX4All Settings",
      "Shows or hides advanced gpu settings. A core restart might be needed for settings to take effect. NOTE: Quick Menu must be toggled for this setting to take effect.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_unai_blending",
      "(GPU) Enable Blending",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gpu_unai_lighting",
      "(GPU) Enable Lighting",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gpu_unai_fast_lighting",
      "(GPU) Enable Fast Lighting",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gpu_unai_ilace_force",
      "(GPU) Enable Forced Interlace",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_unai_pixel_skip",
      "(GPU) Enable Pixel Skip",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#endif /* GPU UNAI Advanced Settings */

   {
      "pcsx_rearmed_show_bios_bootlogo",
      "Show Bios Bootlogo",
      "When enabled, shows the PlayStation logo when starting or resetting. (Breaks some games).",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_spu_reverb",
      "Sound Reverb",
      "Enables or disables audio reverb effect.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_spu_interpolation",
      "Sound Interpolation",
      NULL,
      {
         { "simple",   "Simple" },
         { "gaussian", "Gaussian" },
         { "cubic",    "Cubic" },
         { "off",      "disabled" },
         { NULL, NULL },
      },
      "simple",
   },
   {
      "pcsx_rearmed_idiablofix",
      "Diablo Music Fix",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_pe2_fix",
      "Parasite Eve 2/Vandal Hearts 1/2 Fix",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_inuyasha_fix",
      "InuYasha Sengoku Battle Fix",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
#ifndef _WIN32
   {
      "pcsx_rearmed_async_cd",
      "CD Access Method (Restart)",
      "Select method used to read data from content disk images. 'Synchronous' mimics original hardware. 'Asynchronous' can reduce stuttering on devices with slow storage.",
      {
         { "sync", "Synchronous" },
         { "async",  "Asynchronous" },
         { NULL, NULL},
      },
      "sync",
   },
#endif
   /* ADVANCED OPTIONS */
   {
      "pcsx_rearmed_noxadecoding",
      "XA Decoding",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_nocdaudio",
      "CD Audio",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "pcsx_rearmed_spuirq",
      "SPU IRQ Always Enabled",
      "Compatibility tweak, should be left to off in most cases.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },

#ifndef DRC_DISABLE
   {
      "pcsx_rearmed_nosmccheck",
      "(Speed Hack) Disable SMC Checks",
      "Will cause crashes when loading, break memcards.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gteregsunneeded",
      "(Speed Hack) Assume GTE Regs Unneeded",
      "May cause graphical glitches.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      "pcsx_rearmed_nogteflags",
      "(Speed Hack) Disable GTE Flags",
      "Will cause graphical glitches.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
#endif /* DRC_DISABLE */

   { NULL, NULL, NULL, {{0}}, NULL },
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   option_defs_tr, /* RETRO_LANGUAGE_TURKISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version >= 1))
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
#else
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, &option_defs_us);
#endif
   }
   else
   {
      size_t i;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options
       * > Note: We are going to skip a number of irrelevant
       *   core options when building the retro_variable array,
       *   but we'll allocate space for all of them. The difference
       *   in resource usage is negligible, and this allows us to
       *   keep the code 'cleaner' */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         /* Skip options that are irrelevant when using the
          * old style core options interface */
         if ((strcmp(key, "pcsx_rearmed_show_gpu_peops_settings") == 0))
            continue;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 0)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[option_index].key   = key;
         variables[option_index].value = values_buf[i];
         option_index++;
      }

      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
