#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <unistd.h>
#include <hildon/hildon.h>
#include "minimal.h"

GdkImage *image;
HildonAnimationActor *actor;
GtkWidget *window, *drawing;

#define X_RES           800
#define Y_RES           480
#define D_WIDTH			640
#define D_HEIGHT		480

int screen_size;

void *pl_fbdev_buf=NULL;
int keymap[65536];
void hildon_quit();
unsigned long keys = 0;

static void
window_key_proxy (GtkWidget *widget,
		     GdkEventKey *event,
		     gpointer user_data)
{
	unsigned long key = 0;
switch(keymap[event->hardware_keycode]){
	case -1:
		return; break;
	case 0:
			key = GP2X_LEFT;break;
	case 1:
			key = GP2X_RIGHT;break;
	case 2:
			key = GP2X_UP;break;
	case 3:
			key = GP2X_DOWN;break;
	case 4:
			key = GP2X_B;break;
	case 5:
			key = GP2X_X;break;
	case 6:
			key = GP2X_Y;break;
	case 7:
			key = GP2X_A;break;
	case 8:
			key = GP2X_SELECT;break;
	case 9:
			key = GP2X_START;break;
	case 10:
			key = GP2X_VOL_DOWN;break;
	case 11:
			key = GP2X_VOL_UP;break;
	case 12:
			key = GP2X_L;break;
	case 13:
			key = GP2X_R;break;
	case 14:
		hildon_quit();break;
	case 15:
			if (event->type == GDK_KEY_PRESS){
			keys |= GP2X_LEFT;
			keys |= GP2X_UP;
			}else if (event->type == GDK_KEY_RELEASE){
			keys &= ~GP2X_LEFT;
			keys &= ~GP2X_UP;
			}
		key = -1;			return ;			break;
	case 16:
			if (event->type == GDK_KEY_PRESS){
			keys |= GP2X_RIGHT;
			keys |= GP2X_UP;
			}else if (event->type == GDK_KEY_RELEASE){
			keys &= ~GP2X_RIGHT;
			keys &= ~GP2X_UP;
			}
		key = -1;			return ;			break;
	case 17:
			if (event->type == GDK_KEY_PRESS){
			keys |= GP2X_LEFT;
			keys |= GP2X_DOWN;
			}else if (event->type == GDK_KEY_RELEASE){
			keys &= ~GP2X_LEFT;
			keys &= ~GP2X_DOWN;
			}
		key = -1;			return ;			break;
	case 18:
			if (event->type == GDK_KEY_PRESS){
			keys |= GP2X_RIGHT;
			keys |= GP2X_DOWN;
			}else if (event->type == GDK_KEY_RELEASE){
			keys &= ~GP2X_RIGHT;
			keys &= ~GP2X_DOWN;
			}
		key = -1;			return ;			break;
/*	case 19:
		SaveState(cfile);
		key = -1;			return ;			break;
	break;
	case 20:
		LoadState(cfile);
		key = -1;			return ;			break;*/
	break;
}
	
	if (event->type == GDK_KEY_PRESS) {
		keys |= key;
	}
	else if (event->type == GDK_KEY_RELEASE) {
		keys &= ~key;
	}
}
unsigned long int gp2x_joystick_read();
unsigned int ReadZodKeys()
{
	unsigned int pad_status = 0xffff;
	unsigned long int  keys = gp2x_joystick_read();


    if(	keys & GP2X_VOL_DOWN ) // L2
  	{
  		pad_status &= ~(1<<8);
  	}
  	if (keys & GP2X_L)
  	{
  		pad_status &= ~(1<<10); // L ?
  	}
  
  

  	if( keys & GP2X_VOL_UP ) // R2
  	{
  		pad_status &= ~(1<<9);
  	}
  	if (keys & GP2X_R)
  	{
  		pad_status &= ~(1<<11); // R ?
  	}
    
	if (keys & GP2X_UP)
	{
		pad_status &= ~(1<<4); 
	}
	if (keys & GP2X_DOWN)
	{
		pad_status &= ~(1<<6);
	}
	if (keys & GP2X_LEFT)
	{
		pad_status &= ~(1<<7);
	}
	if (keys & GP2X_RIGHT)
	{
		pad_status &= ~(1<<5);
	}
	if (keys & GP2X_START)
	{
		pad_status &= ~(1<<3);
	}
	if (keys & GP2X_SELECT)
	{			
		pad_status &= ~(1);
	}
		
	if (keys & GP2X_X)
	{
		pad_status &= ~(1<<14);
	}
	if (keys & GP2X_B)
	{
		pad_status &= ~(1<<13);
	}
	if (keys & GP2X_A)
	{
		pad_status &= ~(1<<15);
	}
	if (keys & GP2X_Y)
	{
		pad_status &= ~(1<<12);
	}

	return pad_status;
}

void hildon_quit()
{
	gtk_main_quit();
	exit(0);
}
void plat_finish(){hildon_quit();}

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


void pl_fbdev_set_mode(int w, int h,int bpp)
{
//if (bpp==24){w=800;h=480;}
	if (w <= 0 || h <= 0)
		return;

	if (image) gdk_image_destroy(image);
	image = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(), w, h );

	pl_fbdev_buf = (void*) image->mem;
	screen_size = image->bpl * h  * image->bpp;

	gtk_image_set_from_image (GTK_IMAGE(drawing), image, NULL);

	gtk_window_resize (GTK_WINDOW (actor), w, h);
	hildon_animation_actor_set_scale (actor,
				(gdouble)D_WIDTH / (gdouble)w,
				(gdouble)D_HEIGHT / (gdouble)h
	);
}

unsigned long gp2x_joystick_read(void)
{
	//printf("gp2x_joystick_read\n");
	/* process GTK+ events */
	while (gtk_events_pending())
		gtk_main_iteration();

	return keys;
}

void gp2x_video_RGB_clearscreen16(void)
{
	//if (screenbuffer && screen_size)
	//	memset(pl_fbdev_buf, 0, screen_size);
}

void pl_fbdev_flip()
{
	gtk_widget_queue_draw (drawing);
}

void gp2x_printfchar15(gp2x_font *f, unsigned char c)
{
  unsigned short *dst=&((unsigned short*)pl_fbdev_buf)[f->x+f->y*(image->bpl>>1)],w,h=f->h;
//unsigned char  *src=f->data[ (c%16)*f->w + (c/16)*f->h ];
  unsigned char  *src=&f->data[c*10];

 if(f->solid)
         while(h--)
         {
          w=f->wmask;
          while(w)
          {
           if( *src & w ) *dst++=f->fg; else *dst++=f->bg;
           w>>=1;
          }
          src++;    

          dst+=(image->bpl>>1)-(f->w);
         }
 else
         while(h--)
         {
          w=f->wmask;
          while(w)
          {
           if( *src & w ) *dst=f->fg;
           dst++;
           w>>=1;
          }
          src++;

          dst+=(image->bpl>>1)-(f->w);
         }
}

void pl_frame_limit(void){
}

void pl_fbdev_close(void){
}

void pl_fbdev_open(void){
	 
}


