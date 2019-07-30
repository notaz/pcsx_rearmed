#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

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

#ifdef HAVE_PRE_ARMV7
#define PSX_CLOCK_DEFAULT "50"
#define PSX_CLOCK_LABEL "Overclock or underclock the PSX clock. Default is 50"
#else
#define PSX_CLOCK_DEFAULT "57"
#define PSX_CLOCK_LABEL "Overclock or underclock the PSX clock. Default is 57"
#endif

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
         { NULL, NULL},
      },
      "0",
   },
   {
      "pcsx_rearmed_bios",
      "Use BIOS",
      "Allows you to use real bios file (if available) or emulated bios (HLE). Its recommended to use official bios file for better compatibility.",
      {
         { "auto", "auto" },
         { "HLE", "hle" },
         { NULL, NULL},
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
         { "PAL", "pal" },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "pcsx_rearmed_memcard2",
      "Enable Second Memory Card (Shared)",
      "Enabled the memory card slot 2. This memory card is shared amongst all games.",
      {
         { "disable",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disable",
   },
   {
      "pcsx_rearmed_pad1type",
      "Pad 1 Type",
      "Pad type for player 1",
      {
         { "standard", NULL },
         { "analog", NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "standard",
   },
   {
      "pcsx_rearmed_pad2type",
      "Pad 2 Type",
      "Pad type for player 2",
      {
         { "standard", NULL },
         { "analog", NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "standard",
   },
   {
      "pcsx_rearmed_pad3type",
      "Pad 3 Type",
      "Pad type for player 3",
      {
         { "standard", NULL },
         { "analog", NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },
   {
      "pcsx_rearmed_pad4type",
      "Pad 4 Type",
      "Pad type for player 4",
      {
         { "standard", NULL },
         { "analog", NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },
   {
      "pcsx_rearmed_pad5type",
      "Pad 5 Type",
      "Pad type for player 5",
      {
         { "standard", NULL },
         { "analog",  NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },{
      "pcsx_rearmed_pad6type",
      "Pad 6 Type",
      "Pad type for player 6",
      {
         { "standard", NULL },
         { "analog",  NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },{
      "pcsx_rearmed_pad7type",
      "Pad 7 Type",
      "Pad type for player 7",
      {
         { "standard", NULL },
         { "analog",  NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },{
      "pcsx_rearmed_pad8type",
      "Pad 8 Type",
      "Pad type for player 8",
      {
         { "standard", NULL },
         { "analog",  NULL },
         { "dualshock", NULL },
         { "negcon", NULL },
         { "none", NULL },
         { NULL, NULL},
      },
      "none",
   },
   {
      "pcsx_rearmed_multitap1",
      "Multitap 1",
      "Enables/Disables multitap on port 1, allowing upto 5 players in games that permit it.",
      {
         { "auto", NULL },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "pcsx_rearmed_multitap2",
      "Multitap 2",
      "Enables/Disables multitap on port 2, allowing up to 8 players in games that permit it. Multitap 1 has to be enabled for this to work.",
      {
         { "auto", NULL },
         { "disabled",  NULL },
         { "enabled",   NULL },
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "0",
   },
   {
      "pcsx_rearmed_negcon_response",
      "NegCon Twist Response",
      "Specifies the analog response when using a RetroPad left analog stick to simulate the 'twist' action of emulated neGcon Controllers.",
      {
         { "linear", NULL },
         { "quadratic", NULL },
         { "cubic", NULL },
         { NULL, NULL},
      },
      "linear",
   },
   {
      "pcsx_rearmed_vibration",
      "Enable Vibration",
      "Enables vibration feedback for controllers that supports vibration features.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_dithering",
      "Enable Dithering",
      "If Off, disables the dithering pattern the PSX applies to combat color banding.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },

#ifndef DRC_DISABLE
   {
      "pcsx_rearmed_drc",
      "Dynamic Recompiler",
      "Enables core to use dynamic recompiler or interpreter (slower) CPU instructions.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_psxclock",
      "PSX CPU Clock",
      PSX_CLOCK_LABEL,
      {
         { "30", NULL },
         { "31", NULL },
         { "32", NULL },
         { "33", NULL },
         { "34", NULL },
         { "35", NULL },
         { "36", NULL },
         { "37", NULL },
         { "38", NULL },
         { "39", NULL },
         { "40", NULL },
         { "41", NULL },
         { "42", NULL },
         { "43", NULL },
         { "44", NULL },
         { "45", NULL },
         { "46", NULL },
         { "47", NULL },
         { "48", NULL },
         { "49", NULL },
         { "50", NULL },
         { "51", NULL },
         { "52", NULL },
         { "53", NULL },
         { "54", NULL },
         { "55", NULL },
         { "56", NULL },
         { "57", NULL },
         { "58", NULL },
         { "59", NULL },
         { "60", NULL },
         { "61", NULL },
         { "62", NULL },
         { "63", NULL },
         { "64", NULL },
         { "65", NULL },
         { "66", NULL },
         { "67", NULL },
         { "68", NULL },
         { "69", NULL },
         { "70", NULL },
         { "71", NULL },
         { "72", NULL },
         { "73", NULL },
         { "74", NULL },
         { "75", NULL },
         { "76", NULL },
         { "77", NULL },
         { "78", NULL },
         { "79", NULL },
         { "80", NULL },
         { "81", NULL },
         { "82", NULL },
         { "83", NULL },
         { "84", NULL },
         { "85", NULL },
         { "86", NULL },
         { "87", NULL },
         { "88", NULL },
         { "89", NULL },
         { "90", NULL },
         { "91", NULL },
         { "92", NULL },
         { "93", NULL },
         { "94", NULL },
         { "95", NULL },
         { "96", NULL },
         { "97", NULL },
         { "98", NULL },
         { "99", NULL },
         { "100", NULL },
         { NULL, NULL},
      },
      PSX_CLOCK_DEFAULT,
   },
#endif /* DRC_DISABLE */

#ifdef __ARM_NEON__
   {
      "pcsx_rearmed_neon_interlace_enable",
      "Enable Interlacing Mode",
      "Enables fake scanlines effect.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
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
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "disabled",
   },
#endif /* __ARM_NEON__ */

   {
      "pcsx_rearmed_duping_enable",
      "Frame Duping",
      "A speedup, redraws/reuses the last frame if there was no new data.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
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
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_0",
      "(GPU) Odd/Even Bit Hack",
      "Needed for Chrono Cross.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_1",
      "(GPU) Expand Screen Width",
      "Capcom fighting games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_2",
      "(GPU) Ignore Brightness Color",
      "Black screens in Lunar Silver Star Story games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_3",
      "(GPU) Disable Coordinate Check",
      "Compatibility mode",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_6",
      "(GPU) Lazy Screen Update",
      "Pandemonium 2",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_7",
      "(GPU) Old Frame Skipping",
      "Skip every second frame",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_8",
      "(GPU) Repeated Flat Tex Triangles",
      "Needed by Star Wars: Dark Forces",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_9",
      "(GPU) Draw Quads with Triangles",
      "Better g-colors, worse textures",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      "pcsx_rearmed_gpu_peops_fix_10",
      "(GPU) Fake 'Gpu Busy' States",
      "Toggle busy flags after drawing",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#endif

   {
      "pcsx_rearmed_show_bios_bootlogo",
      "Show Bios Bootlogo",
      "When enabled, shows the PlayStation logo when starting or resetting. (Breaks some games).",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "pcsx_rearmed_spu_interpolation",
      "Sound Interpolation",
      NULL,
      {
         { "simple", NULL },
         { "gaussian", NULL },
         { "cubic", NULL },
         { "off", NULL },
         { NULL, NULL},
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
         { NULL, NULL},
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
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "disabled",
   },

   /* ADVANCED OPTIONS */
   {
      "pcsx_rearmed_noxadecoding",
      "XA Decoding",
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "enabled",
   },

#ifndef DRC_DISABLE
   {
      "pcsx_rearmed_nosmccheck",
      "(Speed Hack) Disable SMC Checks",
      "Will cause crashes when loading, break memcards.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
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
         { NULL, NULL},
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
         { NULL, NULL},
      },
      "disabled",
   },
#endif /* DRC_DISABLE */

   { NULL, NULL, NULL, { NULL, NULL }, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

/*
 ********************************
 * Language Mapping
 ********************************
*/

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
   NULL,           /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
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
            if (num_values > 1)
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
