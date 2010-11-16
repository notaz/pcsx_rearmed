#include "config.h"

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include <stdio.h>
#include <stdlib.h>

void SaveConfig(void);

void
on_btnSave_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
 SaveConfig();
 exit(0);
}


void
on_CfgWnd_destroy                      (GtkObject       *object,
                                        gpointer         user_data)
{
 exit(0);
}


void
on_btnCancel_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
 exit(0);
}


void
on_AboutWnd_destroy                    (GtkObject       *object,
                                        gpointer         user_data)
{
 exit(0);
}


void
on_bntAClose_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
 exit(0);
}
