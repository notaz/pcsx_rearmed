#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x) gettext(x)
#else
#define _(x) (x)
#endif

GtkWidget*
create_CfgWnd (void)
{
  GtkWidget *CfgWnd;
  GtkWidget *fixed1;
  GtkWidget *btnSave;
  GtkWidget *frmTextures;
  GtkWidget *fixed3;
  GtkWidget *edtMaxTex;
  GtkWidget *label5;
  GtkWidget *cmbQuality;
  GList *cmbQuality_items = NULL;
  GtkWidget *combo_entry2;
  GtkWidget *label7;
  GtkWidget *cmbFilter;
  GList *cmbFilter_items = NULL;
  GtkWidget *combo_entry3;
  GtkWidget *label6;
  GtkWidget *label23;
  GtkWidget *cmbHiresTex;
  GList *cmbHiresTex_items = NULL;
  GtkWidget *combo_entry7;
  GtkWidget *frmWindow;
  GtkWidget *fixed2;
  GtkWidget *edtXSize;
  GtkWidget *edtYSize;
  GtkWidget *label2;
  GtkWidget *label3;
  GtkWidget *chkKeepRatio;
  GtkWidget *chkFullScreen;
  GtkWidget *chkDither;
  GtkWidget *btnCancel;
  GtkWidget *frmFPS;
  GtkWidget *fixed4;
  GtkWidget *edtFPSlim;
  GtkWidget *label8;
  GSList *fixed4_group = NULL;
  GtkWidget *rdbLimMan;
  GtkWidget *chkShowFPS;
  GtkWidget *chkFPSLimit;
  GtkWidget *rdbLimAuto;
  GtkWidget *chkFPSSkip;
  GtkWidget *frmCompat;
  GtkWidget *fixed5;
  GtkWidget *chkABlend;
  GtkWidget *label10;
  GtkWidget *label9;
  GtkWidget *label22;
  GtkWidget *chkOpaque;
  GtkWidget *chkMaskBit;
  GtkWidget *cmbOffscreen;
  GList *cmbOffscreen_items = NULL;
  GtkWidget *combo_entry4;
  GtkWidget *cmbFrameTex;
  GList *cmbFrameTex_items = NULL;
  GtkWidget *combo_entry5;
  GtkWidget *cmbFrameAcc;
  GList *cmbFrameAcc_items = NULL;
  GtkWidget *combo_entry6;
  GtkWidget *frmFixes;
  GtkWidget *fixed7;
  GtkWidget *chkFix3;
  GtkWidget *chkFix4;
  GtkWidget *chkFix5;
  GtkWidget *chkGameFixes;
  GtkWidget *chkFix2;
  GtkWidget *chkFix1;
  GtkWidget *chkFix7;
  GtkWidget *chkFix0;
  GtkWidget *chkFix6;
  GtkWidget *chkFix8;
  GtkWidget *chkFix9;
  GtkWidget *chkFix10;
  GtkWidget *chkFix11;
  GtkWidget *chkFix12;
  GtkWidget *chkFix13;
  GtkWidget *chkFix14;
  GtkWidget *chkFix15;
  GtkWidget *chkFix17;
  GtkWidget *chkFix16;
  GtkWidget *frmMisc;
  GtkWidget *fixed6;
  GtkWidget *edtScanBlend;
  GtkWidget *chkScanlines;
  GtkWidget *label11;
  GtkWidget *chkBlur;
  GtkWidget *chkExtensions;
  GtkWidget *chkAntiA;
  GtkWidget *chkLinemode;
  GtkWidget *chkFastMdec;
  GtkWidget *chk15bitMdec;

  CfgWnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (CfgWnd), "CfgWnd", CfgWnd);
  gtk_container_set_border_width (GTK_CONTAINER (CfgWnd), 8);
  gtk_window_set_title (GTK_WINDOW (CfgWnd), _("OpenGL Driver configuration"));
  gtk_window_set_position (GTK_WINDOW (CfgWnd), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (CfgWnd), TRUE);
  gtk_window_set_policy (GTK_WINDOW (CfgWnd), FALSE, FALSE, FALSE);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed1", fixed1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_container_add (GTK_CONTAINER (CfgWnd), fixed1);

  btnSave = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (btnSave);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "btnSave", btnSave,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (btnSave);
  gtk_fixed_put (GTK_FIXED (fixed1), btnSave, 134, 552);
  gtk_widget_set_usize (btnSave, 160, 24);

  frmTextures = gtk_frame_new (_("Textures"));
  gtk_widget_ref (frmTextures);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmTextures", frmTextures,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmTextures);
  gtk_fixed_put (GTK_FIXED (fixed1), frmTextures, 372, 0);
  gtk_widget_set_usize (frmTextures, 364, 136);

  fixed3 = gtk_fixed_new ();
  gtk_widget_ref (fixed3);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed3", fixed3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed3);
  gtk_container_add (GTK_CONTAINER (frmTextures), fixed3);

  edtMaxTex = gtk_entry_new ();
  gtk_widget_ref (edtMaxTex);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "edtMaxTex", edtMaxTex,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (edtMaxTex);
  gtk_fixed_put (GTK_FIXED (fixed3), edtMaxTex, 278, 80);
  gtk_widget_set_usize (edtMaxTex, 66, 24);

  label5 = gtk_label_new (_("Quality:"));
  gtk_widget_ref (label5);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label5", label5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label5);
  gtk_fixed_put (GTK_FIXED (fixed3), label5, 8, 0);
  gtk_widget_set_usize (label5, 64, 24);

  cmbQuality = gtk_combo_new ();
  gtk_widget_ref (cmbQuality);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbQuality", cmbQuality,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbQuality);
  gtk_fixed_put (GTK_FIXED (fixed3), cmbQuality, 80, 0);
  gtk_widget_set_usize (cmbQuality, 264, 24);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbQuality), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbQuality), TRUE);
  cmbQuality_items = g_list_append (cmbQuality_items, (gpointer) _("0: don't care - Use driver's default textures"));
  cmbQuality_items = g_list_append (cmbQuality_items, (gpointer) _("1: 4444 - Fast, but less colorful"));
  cmbQuality_items = g_list_append (cmbQuality_items, (gpointer) _("2: 5551 - Nice colors, bad transparency"));
  cmbQuality_items = g_list_append (cmbQuality_items, (gpointer) _("3: 8888 - Best colors, more ram needed"));
  cmbQuality_items = g_list_append (cmbQuality_items, (gpointer) _("4: BGR8888 - Faster on some cards"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbQuality), cmbQuality_items);
  g_list_free (cmbQuality_items);

  combo_entry2 = GTK_COMBO (cmbQuality)->entry;
  gtk_widget_ref (combo_entry2);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry2", combo_entry2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry2);
  gtk_entry_set_text (GTK_ENTRY (combo_entry2), _("0: don't care - Use driver's default textures"));

  label7 = gtk_label_new (_("VRam size in MBytes (0..1024, 0=auto):"));
  gtk_widget_ref (label7);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label7", label7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label7);
  gtk_fixed_put (GTK_FIXED (fixed3), label7, 8, 80);
  gtk_widget_set_usize (label7, 260, 24);

  cmbFilter = gtk_combo_new ();
  gtk_widget_ref (cmbFilter);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbFilter", cmbFilter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbFilter);
  gtk_fixed_put (GTK_FIXED (fixed3), cmbFilter, 80, 24);
  gtk_widget_set_usize (cmbFilter, 264, 24);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbFilter), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbFilter), TRUE);
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("0: None"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("1: Standard - Glitches will happen"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("2: Extended - No black borders"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("3: Standard without sprites - unfiltered 2D"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("4: Extended without sprites - unfiltered 2D"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("5: Standard + smoothed sprites"));
  cmbFilter_items = g_list_append (cmbFilter_items, (gpointer) _("6: Extended + smoothed sprites"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbFilter), cmbFilter_items);
  g_list_free (cmbFilter_items);

  combo_entry3 = GTK_COMBO (cmbFilter)->entry;
  gtk_widget_ref (combo_entry3);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry3", combo_entry3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry3);
  gtk_entry_set_text (GTK_ENTRY (combo_entry3), _("0: None"));

  label6 = gtk_label_new (_("Filtering:"));
  gtk_widget_ref (label6);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label6", label6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label6);
  gtk_fixed_put (GTK_FIXED (fixed3), label6, 8, 24);
  gtk_widget_set_usize (label6, 64, 24);

  label23 = gtk_label_new (_("HiRes Tex:"));
  gtk_widget_ref (label23);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label23", label23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label23);
  gtk_fixed_put (GTK_FIXED (fixed3), label23, 8, 48);
  gtk_widget_set_usize (label23, 64, 24);

  cmbHiresTex = gtk_combo_new ();
  gtk_widget_ref (cmbHiresTex);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbHiresTex", cmbHiresTex,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbHiresTex);
  gtk_fixed_put (GTK_FIXED (fixed3), cmbHiresTex, 80, 48);
  gtk_widget_set_usize (cmbHiresTex, 264, 22);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbHiresTex), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbHiresTex), TRUE);
  cmbHiresTex_items = g_list_append (cmbHiresTex_items, (gpointer) _("0: None (standard)"));
  cmbHiresTex_items = g_list_append (cmbHiresTex_items, (gpointer) _("1: 2xSaI (much vram needed)"));
  cmbHiresTex_items = g_list_append (cmbHiresTex_items, (gpointer) _("2: Scaled (needs tex filtering)"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbHiresTex), cmbHiresTex_items);
  g_list_free (cmbHiresTex_items);

  combo_entry7 = GTK_COMBO (cmbHiresTex)->entry;
  gtk_widget_ref (combo_entry7);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry7", combo_entry7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry7);
  gtk_entry_set_text (GTK_ENTRY (combo_entry7), _("0: None (standard)"));

  frmWindow = gtk_frame_new (_("Window options"));
  gtk_widget_ref (frmWindow);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmWindow", frmWindow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmWindow);
  gtk_fixed_put (GTK_FIXED (fixed1), frmWindow, 0, 0);
  gtk_widget_set_usize (frmWindow, 364, 136);

  fixed2 = gtk_fixed_new ();
  gtk_widget_ref (fixed2);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed2", fixed2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed2);
  gtk_container_add (GTK_CONTAINER (frmWindow), fixed2);

  edtXSize = gtk_entry_new_with_max_length (5);
  gtk_widget_ref (edtXSize);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "edtXSize", edtXSize,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (edtXSize);
  gtk_fixed_put (GTK_FIXED (fixed2), edtXSize, 56, 0);
  gtk_widget_set_usize (edtXSize, 72, 24);

  edtYSize = gtk_entry_new ();
  gtk_widget_ref (edtYSize);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "edtYSize", edtYSize,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (edtYSize);
  gtk_fixed_put (GTK_FIXED (fixed2), edtYSize, 56, 32);
  gtk_widget_set_usize (edtYSize, 72, 24);

  label2 = gtk_label_new (_("Width:"));
  gtk_widget_ref (label2);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label2", label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label2);
  gtk_fixed_put (GTK_FIXED (fixed2), label2, 8, 0);
  gtk_widget_set_usize (label2, 48, 24);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_RIGHT);

  label3 = gtk_label_new (_("Height:"));
  gtk_widget_ref (label3);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label3", label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label3);
  gtk_fixed_put (GTK_FIXED (fixed2), label3, 8, 32);
  gtk_widget_set_usize (label3, 48, 24);
  gtk_label_set_justify (GTK_LABEL (label3), GTK_JUSTIFY_RIGHT);

  chkKeepRatio = gtk_check_button_new_with_label (_("Keep psx aspect ratio"));
  gtk_widget_ref (chkKeepRatio);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkKeepRatio", chkKeepRatio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkKeepRatio);
  gtk_fixed_put (GTK_FIXED (fixed2), chkKeepRatio, 8, 88);
  gtk_widget_set_usize (chkKeepRatio, 280, 24);

  chkFullScreen = gtk_check_button_new_with_label (_("Fullscreen"));
  gtk_widget_ref (chkFullScreen);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFullScreen", chkFullScreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFullScreen);
  gtk_fixed_put (GTK_FIXED (fixed2), chkFullScreen, 196, 0);
  gtk_widget_set_usize (chkFullScreen, 125, 24);

  chkDither = gtk_check_button_new_with_label (_("Dithering"));
  gtk_widget_ref (chkDither);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkDither", chkDither,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkDither);
  gtk_fixed_put (GTK_FIXED (fixed2), chkDither, 8, 64);
  gtk_widget_set_usize (chkDither, 280, 24);

  btnCancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (btnCancel);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "btnCancel", btnCancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (btnCancel);
  gtk_fixed_put (GTK_FIXED (fixed1), btnCancel, 430, 552);
  gtk_widget_set_usize (btnCancel, 160, 24);

  frmFPS = gtk_frame_new (_("Framerate"));
  gtk_widget_ref (frmFPS);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmFPS", frmFPS,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmFPS);
  gtk_fixed_put (GTK_FIXED (fixed1), frmFPS, 0, 136);
  gtk_widget_set_usize (frmFPS, 364, 176);

  fixed4 = gtk_fixed_new ();
  gtk_widget_ref (fixed4);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed4", fixed4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed4);
  gtk_container_add (GTK_CONTAINER (frmFPS), fixed4);

  edtFPSlim = gtk_entry_new ();
  gtk_widget_ref (edtFPSlim);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "edtFPSlim", edtFPSlim,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (edtFPSlim);
  gtk_fixed_put (GTK_FIXED (fixed4), edtFPSlim, 175, 104);
  gtk_widget_set_usize (edtFPSlim, 72, 24);

  label8 = gtk_label_new (_("FPS"));
  gtk_widget_ref (label8);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label8", label8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label8);
  gtk_fixed_put (GTK_FIXED (fixed4), label8, 250, 104);
  gtk_widget_set_usize (label8, 40, 24);

  rdbLimMan = gtk_radio_button_new_with_label (fixed4_group, _("FPS limit manual"));
  fixed4_group = gtk_radio_button_group (GTK_RADIO_BUTTON (rdbLimMan));
  gtk_widget_ref (rdbLimMan);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "rdbLimMan", rdbLimMan,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (rdbLimMan);
  gtk_fixed_put (GTK_FIXED (fixed4), rdbLimMan, 32, 104);
  gtk_widget_set_usize (rdbLimMan, 140, 24);

  chkShowFPS = gtk_check_button_new_with_label (_("Show FPS display on startup"));
  gtk_widget_ref (chkShowFPS);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkShowFPS", chkShowFPS,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkShowFPS);
  gtk_fixed_put (GTK_FIXED (fixed4), chkShowFPS, 8, 0);
  gtk_widget_set_usize (chkShowFPS, 280, 24);

  chkFPSLimit = gtk_check_button_new_with_label (_("Use FPS limit"));
  gtk_widget_ref (chkFPSLimit);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFPSLimit", chkFPSLimit,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFPSLimit);
  gtk_fixed_put (GTK_FIXED (fixed4), chkFPSLimit, 8, 24);
  gtk_widget_set_usize (chkFPSLimit, 280, 24);

  rdbLimAuto = gtk_radio_button_new_with_label (fixed4_group, _("FPS limit auto-detection"));
  fixed4_group = gtk_radio_button_group (GTK_RADIO_BUTTON (rdbLimAuto));
  gtk_widget_ref (rdbLimAuto);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "rdbLimAuto", rdbLimAuto,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (rdbLimAuto);
  gtk_fixed_put (GTK_FIXED (fixed4), rdbLimAuto, 32, 80);
  gtk_widget_set_usize (rdbLimAuto, 200, 24);

  chkFPSSkip = gtk_check_button_new_with_label (_("Use Frame skipping"));
  gtk_widget_ref (chkFPSSkip);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFPSSkip", chkFPSSkip,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFPSSkip);
  gtk_fixed_put (GTK_FIXED (fixed4), chkFPSSkip, 8, 48);
  gtk_widget_set_usize (chkFPSSkip, 280, 24);

  frmCompat = gtk_frame_new (_("Compatibility"));
  gtk_widget_ref (frmCompat);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmCompat", frmCompat,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmCompat);
  gtk_fixed_put (GTK_FIXED (fixed1), frmCompat, 372, 136);
  gtk_widget_set_usize (frmCompat, 364, 176);

  fixed5 = gtk_fixed_new ();
  gtk_widget_ref (fixed5);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed5", fixed5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed5);
  gtk_container_add (GTK_CONTAINER (frmCompat), fixed5);

  chkABlend = gtk_check_button_new_with_label (_("Advanced blending (Accurate psx color emulation)"));
  gtk_widget_ref (chkABlend);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkABlend", chkABlend,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkABlend);
  gtk_fixed_put (GTK_FIXED (fixed5), chkABlend, 8, 128);
  gtk_widget_set_usize (chkABlend, 366, 24);

  label10 = gtk_label_new (_("Framebuffer textures:"));
  gtk_widget_ref (label10);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label10", label10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label10);
  gtk_fixed_put (GTK_FIXED (fixed5), label10, 0, 24);
  gtk_widget_set_usize (label10, 136, 24);

  label9 = gtk_label_new (_("Offscreen Drawing:"));
  gtk_widget_ref (label9);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label9", label9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label9);
  gtk_fixed_put (GTK_FIXED (fixed5), label9, 0, 0);
  gtk_widget_set_usize (label9, 136, 24);

  label22 = gtk_label_new (_("Framebuffer access:"));
  gtk_widget_ref (label22);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label22", label22,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label22);
  gtk_fixed_put (GTK_FIXED (fixed5), label22, 0, 48);
  gtk_widget_set_usize (label22, 136, 24);

  chkOpaque = gtk_check_button_new_with_label (_("Alpha Multipass (correct opaque texture areas)"));
  gtk_widget_ref (chkOpaque);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkOpaque", chkOpaque,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkOpaque);
  gtk_fixed_put (GTK_FIXED (fixed5), chkOpaque, 8, 104);
  gtk_widget_set_usize (chkOpaque, 366, 24);

  chkMaskBit = gtk_check_button_new_with_label (_("Mask bit detection (needed by a few games, zbuffer)"));
  gtk_widget_ref (chkMaskBit);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkMaskBit", chkMaskBit,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkMaskBit);
  gtk_fixed_put (GTK_FIXED (fixed5), chkMaskBit, 8, 80);
  gtk_widget_set_usize (chkMaskBit, 366, 24);

  cmbOffscreen = gtk_combo_new ();
  gtk_widget_ref (cmbOffscreen);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbOffscreen", cmbOffscreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbOffscreen);
  gtk_fixed_put (GTK_FIXED (fixed5), cmbOffscreen, 136, 0);
  gtk_widget_set_usize (cmbOffscreen, 208, 24);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbOffscreen), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbOffscreen), TRUE);
  cmbOffscreen_items = g_list_append (cmbOffscreen_items, (gpointer) _("0: None - Fastest, most glitches"));
  cmbOffscreen_items = g_list_append (cmbOffscreen_items, (gpointer) _("1: Minimum - Missing screens"));
  cmbOffscreen_items = g_list_append (cmbOffscreen_items, (gpointer) _("2: Standard - OK for most games"));
  cmbOffscreen_items = g_list_append (cmbOffscreen_items, (gpointer) _("3: Enhanced - Shows more stuff"));
  cmbOffscreen_items = g_list_append (cmbOffscreen_items, (gpointer) _("4: Extended - Causing garbage"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbOffscreen), cmbOffscreen_items);
  g_list_free (cmbOffscreen_items);

  combo_entry4 = GTK_COMBO (cmbOffscreen)->entry;
  gtk_widget_ref (combo_entry4);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry4", combo_entry4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry4);
  gtk_entry_set_text (GTK_ENTRY (combo_entry4), _("0: None - Fastest, most glitches"));

  cmbFrameTex = gtk_combo_new ();
  gtk_widget_ref (cmbFrameTex);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbFrameTex", cmbFrameTex,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbFrameTex);
  gtk_fixed_put (GTK_FIXED (fixed5), cmbFrameTex, 136, 24);
  gtk_widget_set_usize (cmbFrameTex, 208, 24);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbFrameTex), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbFrameTex), TRUE);
  cmbFrameTex_items = g_list_append (cmbFrameTex_items, (gpointer) _("0: Emulated vram - Needs FVP"));
  cmbFrameTex_items = g_list_append (cmbFrameTex_items, (gpointer) _("1: Black - Fast, no effects"));
  cmbFrameTex_items = g_list_append (cmbFrameTex_items, (gpointer) _("2: Gfx card buffer - Can be slow"));
  cmbFrameTex_items = g_list_append (cmbFrameTex_items, (gpointer) _("3: Gfx card & soft - slow"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbFrameTex), cmbFrameTex_items);
  g_list_free (cmbFrameTex_items);

  combo_entry5 = GTK_COMBO (cmbFrameTex)->entry;
  gtk_widget_ref (combo_entry5);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry5", combo_entry5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry5);
  gtk_entry_set_text (GTK_ENTRY (combo_entry5), _("0: Emulated vram - Needs FVP"));

  cmbFrameAcc = gtk_combo_new ();
  gtk_widget_ref (cmbFrameAcc);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "cmbFrameAcc", cmbFrameAcc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cmbFrameAcc);
  gtk_fixed_put (GTK_FIXED (fixed5), cmbFrameAcc, 136, 48);
  gtk_widget_set_usize (cmbFrameAcc, 208, 22);
  gtk_combo_set_value_in_list (GTK_COMBO (cmbFrameAcc), TRUE, FALSE);
  gtk_combo_set_use_arrows_always (GTK_COMBO (cmbFrameAcc), TRUE);
  cmbFrameAcc_items = g_list_append (cmbFrameAcc_items, (gpointer) _("0: Emulated vram - ok most times"));
  cmbFrameAcc_items = g_list_append (cmbFrameAcc_items, (gpointer) _("1: Gfx card buffer reads"));
  cmbFrameAcc_items = g_list_append (cmbFrameAcc_items, (gpointer) _("2: Gfx card buffer moves"));
  cmbFrameAcc_items = g_list_append (cmbFrameAcc_items, (gpointer) _("3: Gfx buffer reads & moves"));
  cmbFrameAcc_items = g_list_append (cmbFrameAcc_items, (gpointer) _("4: Full Software (FVP)"));
  gtk_combo_set_popdown_strings (GTK_COMBO (cmbFrameAcc), cmbFrameAcc_items);
  g_list_free (cmbFrameAcc_items);

  combo_entry6 = GTK_COMBO (cmbFrameAcc)->entry;
  gtk_widget_ref (combo_entry6);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "combo_entry6", combo_entry6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry6);
  gtk_entry_set_text (GTK_ENTRY (combo_entry6), _("0: Emulated vram - ok most times"));

  frmFixes = gtk_frame_new (_("Special game fixes"));
  gtk_widget_ref (frmFixes);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmFixes", frmFixes,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmFixes);
  gtk_fixed_put (GTK_FIXED (fixed1), frmFixes, 372, 312);
  gtk_widget_set_usize (frmFixes, 364, 232);

  fixed7 = gtk_fixed_new ();
  gtk_widget_ref (fixed7);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed7", fixed7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed7);
  gtk_container_add (GTK_CONTAINER (frmFixes), fixed7);

  chkGameFixes = gtk_check_button_new_with_label (_("Use game fixes"));
  gtk_widget_ref (chkGameFixes);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkGameFixes", chkGameFixes,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkGameFixes);
  gtk_fixed_put (GTK_FIXED (fixed7), chkGameFixes, 8, 0);
  gtk_widget_set_usize (chkGameFixes, 336, 24);

  chkFix0 = gtk_check_button_new_with_label (_("Battle cursor (FF7)"));
  gtk_widget_ref (chkFix0);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix0", chkFix0,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix0);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix0, 8, 24);
  gtk_widget_set_usize (chkFix0, 196, 20);

  chkFix1 = gtk_check_button_new_with_label (_("Direct FB updates"));
  gtk_widget_ref (chkFix1);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix1", chkFix1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix1);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix1, 8, 44);
  gtk_widget_set_usize (chkFix1, 196, 20);

  chkFix2 = gtk_check_button_new_with_label (_("Black brightness (Lunar)"));
  gtk_widget_ref (chkFix2);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix2", chkFix2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix2);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix2, 8, 64);
  gtk_widget_set_usize (chkFix2, 196, 20);

  chkFix3 = gtk_check_button_new_with_label (_("Swap front detection"));
  gtk_widget_ref (chkFix3);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix3", chkFix3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix3);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix3, 8, 84);
  gtk_widget_set_usize (chkFix3, 196, 20);

  chkFix4 = gtk_check_button_new_with_label (_("Disable coord check"));
  gtk_widget_ref (chkFix4);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix4", chkFix4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix4);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix4, 8, 104);
  gtk_widget_set_usize (chkFix4, 196, 20);

  chkFix5 = gtk_check_button_new_with_label (_("No blue glitches (LoD)"));
  gtk_widget_ref (chkFix5);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix5", chkFix5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix5);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix5, 8, 124);
  gtk_widget_set_usize (chkFix5, 196, 20);

  chkFix6 = gtk_check_button_new_with_label (_("Soft FB access"));
  gtk_widget_ref (chkFix6);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix6", chkFix6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix6);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix6, 8, 144);
  gtk_widget_set_usize (chkFix6, 196, 20);

  chkFix7 = gtk_check_button_new_with_label (_("PC fps calculation"));
  gtk_widget_ref (chkFix7);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix7", chkFix7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix7);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix7, 8, 164);
  gtk_widget_set_usize (chkFix7, 196, 20);

  chkFix8 = gtk_check_button_new_with_label (_("Old frame skipping"));
  gtk_widget_ref (chkFix8);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix8", chkFix8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix8);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix8, 8, 184);
  gtk_widget_set_usize (chkFix8, 196, 20);

  chkFix9 = gtk_check_button_new_with_label (_("Yellow rect (FF9)"));
  gtk_widget_ref (chkFix9);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix9", chkFix9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix9);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix9, 194, 24);
  gtk_widget_set_usize (chkFix9, 196, 20);

  chkFix10 = gtk_check_button_new_with_label (_("No subtr. blending"));
  gtk_widget_ref (chkFix10);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix10", chkFix10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix10);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix10, 194, 44);
  gtk_widget_set_usize (chkFix10, 196, 20);

  chkFix11 = gtk_check_button_new_with_label (_("Lazy upload (DW7)"));
  gtk_widget_ref (chkFix11);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix11", chkFix11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix11);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix11, 194, 64);
  gtk_widget_set_usize (chkFix11, 196, 20);

  chkFix12 = gtk_check_button_new_with_label (_("Odd/even hack"));
  gtk_widget_ref (chkFix12);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix12", chkFix12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix12);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix12, 194, 84);
  gtk_widget_set_usize (chkFix12, 196, 20);

  chkFix13 = gtk_check_button_new_with_label (_("Adjust screen width"));
  gtk_widget_ref (chkFix13);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix13", chkFix13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix13);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix13, 194, 104);
  gtk_widget_set_usize (chkFix13, 196, 20);

  chkFix14 = gtk_check_button_new_with_label (_("Old texture filtering"));
  gtk_widget_ref (chkFix14);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix14", chkFix14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix14);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix14, 194, 124);
  gtk_widget_set_usize (chkFix14, 196, 20);

  chkFix15 = gtk_check_button_new_with_label (_("Additional uploads"));
  gtk_widget_ref (chkFix15);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix15", chkFix15,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix15);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix15, 194, 144);
  gtk_widget_set_usize (chkFix15, 196, 20);

  chkFix16 = gtk_check_button_new_with_label (_("unused"));
  gtk_widget_ref (chkFix16);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix16", chkFix16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix16);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix16, 194, 164);
  gtk_widget_set_usize (chkFix16, 196, 20);

  chkFix17 = gtk_check_button_new_with_label (_("Fake 'gpu busy'"));
  gtk_widget_ref (chkFix17);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFix17", chkFix17,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFix17);
  gtk_fixed_put (GTK_FIXED (fixed7), chkFix17, 194, 184);
  gtk_widget_set_usize (chkFix17, 196, 20);

  frmMisc = gtk_frame_new (_("Misc"));
  gtk_widget_ref (frmMisc);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "frmMisc", frmMisc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frmMisc);
  gtk_fixed_put (GTK_FIXED (fixed1), frmMisc, 0, 312);
  gtk_widget_set_usize (frmMisc, 364, 232);

  fixed6 = gtk_fixed_new ();
  gtk_widget_ref (fixed6);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "fixed6", fixed6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed6);
  gtk_container_add (GTK_CONTAINER (frmMisc), fixed6);

  edtScanBlend = gtk_entry_new ();
  gtk_widget_ref (edtScanBlend);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "edtScanBlend", edtScanBlend,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (edtScanBlend);
  gtk_fixed_put (GTK_FIXED (fixed6), edtScanBlend, 285, 0);
  gtk_widget_set_usize (edtScanBlend, 54, 22);

  chkScanlines = gtk_check_button_new_with_label (_("Scanlines"));
  gtk_widget_ref (chkScanlines);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkScanlines", chkScanlines,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkScanlines);
  gtk_fixed_put (GTK_FIXED (fixed6), chkScanlines, 8, 0);
  gtk_widget_set_usize (chkScanlines, 100, 24);

  label11 = gtk_label_new (_("Blending (0..255, -1=dot):"));
  gtk_widget_ref (label11);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "label11", label11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label11);
  gtk_fixed_put (GTK_FIXED (fixed6), label11, 108, 0);
  gtk_widget_set_usize (label11, 164, 24);

  chkBlur = gtk_check_button_new_with_label (_("Screen smoothing (can be slow or unsupported)"));
  gtk_widget_ref (chkBlur);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkBlur", chkBlur,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkBlur);
  gtk_fixed_put (GTK_FIXED (fixed6), chkBlur, 8, 132);
  gtk_widget_set_usize (chkBlur, 350, 20);

  chkExtensions = gtk_check_button_new_with_label (_("Use OpenGL extensions (recommended)"));
  gtk_widget_ref (chkExtensions);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkExtensions", chkExtensions,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkExtensions);
  gtk_fixed_put (GTK_FIXED (fixed6), chkExtensions, 8, 112);
  gtk_widget_set_usize (chkExtensions, 350, 20);

  chkAntiA = gtk_check_button_new_with_label (_("Polygon anti-aliasing (slow with most cards)"));
  gtk_widget_ref (chkAntiA);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkAntiA", chkAntiA,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkAntiA);
  gtk_fixed_put (GTK_FIXED (fixed6), chkAntiA, 8, 92);
  gtk_widget_set_usize (chkAntiA, 350, 20);

  chkLinemode = gtk_check_button_new_with_label (_("Line mode (polygons will not get filled)"));
  gtk_widget_ref (chkLinemode);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkLinemode", chkLinemode,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkLinemode);
  gtk_fixed_put (GTK_FIXED (fixed6), chkLinemode, 8, 72);
  gtk_widget_set_usize (chkLinemode, 350, 20);

  chk15bitMdec = gtk_check_button_new_with_label (_("Force 15 bit framebuffer updates (faster movies)"));
  gtk_widget_ref (chk15bitMdec);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chk15bitMdec", chk15bitMdec,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chk15bitMdec);
  gtk_fixed_put (GTK_FIXED (fixed6), chk15bitMdec, 8, 52);
  gtk_widget_set_usize (chk15bitMdec, 350, 20);

  chkFastMdec = gtk_check_button_new_with_label (_("Unfiltered MDECs (small movie speedup)"));
  gtk_widget_ref (chkFastMdec);
  gtk_object_set_data_full (GTK_OBJECT (CfgWnd), "chkFastMdec", chkFastMdec,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chkFastMdec);
  gtk_fixed_put (GTK_FIXED (fixed6), chkFastMdec, 8, 32);
  gtk_widget_set_usize (chkFastMdec, 350, 20);

  gtk_signal_connect (GTK_OBJECT (CfgWnd), "destroy",
                      GTK_SIGNAL_FUNC (on_CfgWnd_destroy), NULL);
  gtk_signal_connect (GTK_OBJECT (btnSave), "clicked",
                      GTK_SIGNAL_FUNC (on_btnSave_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (btnCancel), "clicked",
                      GTK_SIGNAL_FUNC (on_btnCancel_clicked), NULL);

  return CfgWnd;
}

GtkWidget*
create_AboutWnd (void)
{
  GtkWidget *AboutWnd;
  GtkWidget *fixed8;
  GtkWidget *bntAClose;
  GtkWidget *label13;
  GtkWidget *label15;
  GtkWidget *label21;
  GtkWidget *label19;

  AboutWnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (AboutWnd), "AboutWnd", AboutWnd);
  gtk_container_set_border_width (GTK_CONTAINER (AboutWnd), 12);
  gtk_window_set_title (GTK_WINDOW (AboutWnd), _("About"));
  gtk_window_set_position (GTK_WINDOW (AboutWnd), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (AboutWnd), TRUE);
  gtk_window_set_policy (GTK_WINDOW (AboutWnd), FALSE, FALSE, FALSE);

  fixed8 = gtk_fixed_new ();
  gtk_widget_ref (fixed8);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "fixed8", fixed8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed8);
  gtk_container_add (GTK_CONTAINER (AboutWnd), fixed8);

  bntAClose = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (bntAClose);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "bntAClose", bntAClose,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (bntAClose);
  gtk_fixed_put (GTK_FIXED (fixed8), bntAClose, 136, 184);
  gtk_widget_set_uposition (bntAClose, 136, 184);
  gtk_widget_set_usize (bntAClose, 88, 24);

  label13 = gtk_label_new (_("Adapted from P.E.Op.S OpenGL GPU by Pete Bernert"));
  gtk_widget_ref (label13);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "label13", label13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label13);
  gtk_fixed_put (GTK_FIXED (fixed8), label13, 0, 8);
  gtk_widget_set_uposition (label13, 0, 8);
  gtk_widget_set_usize (label13, 360, 16);

  label15 = gtk_label_new (_("Homepage: http://www.pbernert.com"));
  gtk_widget_ref (label15);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "label15", label15,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label15);
  gtk_fixed_put (GTK_FIXED (fixed8), label15, 0, 40);
  gtk_widget_set_uposition (label15, 0, 40);
  gtk_widget_set_usize (label15, 360, 16);

  label21 = gtk_label_new ("Compile date: " __DATE__);
  gtk_widget_ref (label21);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "label21", label21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label21);
  gtk_fixed_put (GTK_FIXED (fixed8), label21, 0, 136);
  gtk_widget_set_uposition (label21, 0, 136);
  gtk_widget_set_usize (label21, 360, 16);

  label19 = gtk_label_new (_("Version: 1.78"));
  gtk_widget_ref (label19);
  gtk_object_set_data_full (GTK_OBJECT (AboutWnd), "label19", label19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label19);
  gtk_fixed_put (GTK_FIXED (fixed8), label19, 0, 104);
  gtk_widget_set_uposition (label19, 0, 104);
  gtk_widget_set_usize (label19, 360, 16);

  gtk_signal_connect (GTK_OBJECT (AboutWnd), "destroy",
                      GTK_SIGNAL_FUNC (on_AboutWnd_destroy),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (bntAClose), "clicked",
                      GTK_SIGNAL_FUNC (on_bntAClose_clicked),
                      NULL);

  return AboutWnd;
}
