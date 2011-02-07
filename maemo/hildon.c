#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <hildon/hildon.h>
#include "plugin_lib.h"
#include "main.h"
#include "../libpcsxcore/psemu_plugin_defs.h"

#define X_RES           800
#define Y_RES           480
#define D_WIDTH			640
#define D_HEIGHT		480

static GdkImage *image;
static HildonAnimationActor *actor;
static GtkWidget *window, *drawing;

void *pl_fbdev_buf;
int in_type = PSE_PAD_TYPE_STANDARD;
int in_keystate, in_a1[2], in_a2[2];

static int keymap[65536];

// map psx4m compatible keymap to PSX keys
static const unsigned char keymap2[14] = {
	DKEY_LEFT,   // 0
	DKEY_RIGHT,
	DKEY_UP,
	DKEY_DOWN,
	DKEY_CIRCLE,
	DKEY_CROSS,  // 5
	DKEY_TRIANGLE,
	DKEY_SQUARE,
	DKEY_SELECT,
	DKEY_START,
	DKEY_L2,     // 10
	DKEY_R2,
	DKEY_L1,
	DKEY_R1,
};

void hildon_quit()
{
	gtk_main_quit();
	exit(0);
}

static void
window_key_proxy(GtkWidget *widget,
		     GdkEventKey *event,
		     gpointer user_data)
{
	int key, psxkey1 = -1, psxkey2 = -1;

	key = keymap[event->hardware_keycode];
	if (key < 0)
		return;

	if (key < ARRAY_SIZE(keymap2))
		psxkey1 = keymap2[key];
	else switch (key) {
		case 14:
			hildon_quit();
			break;
		case 15:
			psxkey1 = DKEY_UP;
			psxkey2 = DKEY_LEFT;
			break;
		case 16:
			psxkey1 = DKEY_UP;
			psxkey2 = DKEY_RIGHT;
			break;
		case 17:
			psxkey1 = DKEY_DOWN;
			psxkey2 = DKEY_LEFT;
			break;
		case 18:
			psxkey1 = DKEY_DOWN;
			psxkey2 = DKEY_RIGHT;
			break;
		case 19:
			if (event->type == GDK_KEY_PRESS)
				emu_set_action(SACTION_SAVE_STATE);
			return;
		case 20:
			if (event->type == GDK_KEY_PRESS)
				emu_set_action(SACTION_LOAD_STATE);
			return;
	}

	if (event->type == GDK_KEY_PRESS) {
		if (psxkey1 >= 0)
			in_keystate |= 1 << psxkey1;
		if (psxkey2 >= 0)
			in_keystate |= 1 << psxkey2;
	}
	else if (event->type == GDK_KEY_RELEASE) {
		if (psxkey1 >= 0)
			in_keystate &= ~(1 << psxkey1);
		if (psxkey2 >= 0)
			in_keystate &= ~(1 << psxkey2);

		emu_set_action(SACTION_NONE);
	}
}

void plat_finish()
{
	hildon_quit();
}

void maemo_init(int *argc, char ***argv)
{
	FILE* pFile;
	pFile = fopen("/opt/psx4m/keys", "r"); // assume the file exists and has data
	int ch;
	int i=0;
	for (i=0;i<65536;i++)
		keymap[i]=164;
	if (NULL != pFile) {
		for(i=0;i<21;i++){
			fscanf(pFile, "%i",&ch);
			keymap[ch]=i;
		}
		fclose(pFile);
	}
	
	gtk_init (argc, argv);

	window = hildon_stackable_window_new ();
	gtk_widget_realize (window);
	gtk_window_fullscreen (GTK_WINDOW(window));
	g_signal_connect (G_OBJECT (window), "key-press-event",
				G_CALLBACK (window_key_proxy), NULL);
	g_signal_connect (G_OBJECT (window), "key-release-event",
				G_CALLBACK (window_key_proxy), NULL);
	g_signal_connect (G_OBJECT (window), "delete_event",
				G_CALLBACK (hildon_quit), NULL);
	gtk_widget_add_events (window,
				GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	actor = HILDON_ANIMATION_ACTOR (hildon_animation_actor_new());
	hildon_animation_actor_set_position (actor, (X_RES - D_WIDTH)/2, (Y_RES - D_HEIGHT)/2 );
	hildon_animation_actor_set_parent (actor, GTK_WINDOW (window));

	drawing = gtk_image_new ();

	gtk_container_add (GTK_CONTAINER (actor), drawing);

	gtk_widget_show_all (GTK_WIDGET (actor));
	gtk_widget_show_all (GTK_WIDGET (window));
}

void menu_loop(void)
{
}

void *pl_fbdev_set_mode(int w, int h, int bpp)
{
	if (w <= 0 || h <= 0)
		return pl_fbdev_buf;

	if (image) gdk_image_destroy(image);
	image = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(), w, h );

	pl_fbdev_buf = (void *) image->mem;

	gtk_image_set_from_image (GTK_IMAGE(drawing), image, NULL);

	gtk_window_resize (GTK_WINDOW (actor), w, h);
	hildon_animation_actor_set_scale (actor,
				(gdouble)D_WIDTH / (gdouble)w,
				(gdouble)D_HEIGHT / (gdouble)h
	);

	return pl_fbdev_buf;
}

void *pl_fbdev_flip(void)
{
	gtk_widget_queue_draw (drawing);
	return pl_fbdev_buf;
}

void pl_frame_limit(void)
{
	extern void CheckFrameRate(void);
	//CheckFrameRate();

	/* process GTK+ events */
	while (gtk_events_pending())
		gtk_main_iteration();
}

void pl_fbdev_close(void)
{
}

int pl_fbdev_open(void)
{
	return 0;
}

static void pl_get_layer_pos(int *x, int *y, int *w, int *h)
{
	*x = 0;
	*y = 0;
	*w = 800;
	*h = 640;
}

extern int UseFrameSkip; // hmh

const struct rearmed_cbs pl_rearmed_cbs = {
	pl_get_layer_pos,
	pl_fbdev_open,
	pl_fbdev_set_mode,
	pl_fbdev_flip,
	pl_fbdev_close,
	&UseFrameSkip,
};

