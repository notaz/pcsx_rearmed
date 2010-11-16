#ifndef __G_FAKE_TYPES_H__
#define __G_FAKE_TYPES_H__

/* fake gtk types to avoid code drift from upstream PCSX */
typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

#define g_free free

#endif
