#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <hildon/hildon.h>
#include "plugin_lib.h"

#include "main.h"
#include "plat.h"
#include "../libpcsxcore/psemu_plugin_defs.h"
#include "common/readpng.h"
#include "maemo_common.h"

#define X_RES           800
#define Y_RES           480
#define D_WIDTH			640
#define D_HEIGHT		480

static GdkImage *image;
static HildonAnimationActor *actor;
static GtkWidget *window, *drawing;

static int pl_buf_w, pl_buf_h;
static int sens, y_def;
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
			{
				emu_save_state(state_slot);
				char buf[MAXPATHLEN];
				sprintf (buf,"/opt/maemo/usr/games/screenshots%s.%3.3d",file_name,state_slot);
				writepng(buf, image->mem, pl_buf_w,pl_buf_h);
			}
			return;
		case 20:
			if (event->type == GDK_KEY_PRESS)
				emu_load_state(state_slot);
			return;
		case 21:
			if (event->type == GDK_KEY_PRESS)
				state_slot=(state_slot<9)?state_slot+1:0;
			return;
		case 22:
			if (event->type == GDK_KEY_PRESS)
				state_slot=(state_slot>0)?state_slot-1:8;
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
	
	pFile = fopen("/opt/psx4m/config", "r");
	if (NULL != pFile) {
		fscanf(pFile, "%d %d",&sens,&y_def);
		fclose(pFile);
	} else {
		sens=150;
		y_def=500; //near 45 degrees =)
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
	if (g_maemo_opts & 2)
		hildon_animation_actor_set_position (actor, 0, 0 );
	else
		hildon_animation_actor_set_position (actor, (X_RES - D_WIDTH)/2, (Y_RES - D_HEIGHT)/2 );
	hildon_animation_actor_set_parent (actor, GTK_WINDOW (window));

	drawing = gtk_image_new ();

	gtk_container_add (GTK_CONTAINER (actor), drawing);

	gtk_widget_show_all (GTK_WIDGET (actor));
	gtk_widget_show_all (GTK_WIDGET (window));

	g_layer_x = (X_RES - D_WIDTH) / 2;
	g_layer_y = (Y_RES - D_HEIGHT) / 2;
	g_layer_w = D_WIDTH, g_layer_h = D_HEIGHT;

	pl_rearmed_cbs.only_16bpp = 1;
}

void menu_loop(void)
{
}

void *plat_gvideo_set_mode(int *w_, int *h_, int *bpp_)
{
	int w = *w_, h = *h_;

	if (w <= 0 || h <= 0)
		return pl_vout_buf;

	if (image) gdk_image_destroy(image);
	image = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(), w, h );

	pl_vout_buf = (void *) image->mem;

	gtk_image_set_from_image (GTK_IMAGE(drawing), image, NULL);

	gtk_window_resize (GTK_WINDOW (actor), w, h);
	if (g_maemo_opts & 2)
		hildon_animation_actor_set_scale (actor,
				(gdouble)800 / (gdouble)w,
				(gdouble)480 / (gdouble)h
				);
	else
		hildon_animation_actor_set_scale (actor,
				(gdouble)D_WIDTH / (gdouble)w,
				(gdouble)D_HEIGHT / (gdouble)h
				);
	pl_buf_w=w;pl_buf_h=h;
	return pl_vout_buf;
}

void *plat_gvideo_flip(void)
{
	gtk_widget_queue_draw (drawing);

	// process accelometer
	if (g_maemo_opts & 4) {
		int x, y, z;
		FILE* f = fopen( "/sys/class/i2c-adapter/i2c-3/3-001d/coord", "r" );
		if( !f ) {printf ("err in accel"); exit(1);}
		fscanf( f, "%d %d %d", &x, &y, &z );
		fclose( f );

		if( x > sens ) in_keystate |= 1 << DKEY_LEFT;
		else if( x < -sens ) in_keystate |= 1 << DKEY_RIGHT;
		else {in_keystate &= ~(1 << DKEY_LEFT);in_keystate &= ~(1 << DKEY_RIGHT);}

		y+=y_def;
		if( y > sens )in_keystate |= 1 << DKEY_UP;
		else if( y < -sens ) in_keystate |= 1 << DKEY_DOWN; 
		else {in_keystate &= ~(1 << DKEY_DOWN);in_keystate &= ~(1 << DKEY_UP);}

	}

	/* process GTK+ events */
	while (gtk_events_pending())
		gtk_main_iteration();

	return pl_vout_buf;
}

void plat_gvideo_open(void)
{
}

void plat_gvideo_close(void)
{
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	return NULL;
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
