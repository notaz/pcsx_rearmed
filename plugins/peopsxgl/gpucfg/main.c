#include "config.h"

#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#endif 

#define SETCHECK(winame)  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON ((GtkWidget*) gtk_object_get_data (GTK_OBJECT (CfgWnd),winame)), TRUE)
#define SETEDIT(winame,sz)  gtk_entry_set_text(GTK_ENTRY((GtkWidget*) gtk_object_get_data (GTK_OBJECT (CfgWnd),winame)), sz)
#define SETEDITVAL(winame,v)  sprintf(t,"%d",v);gtk_entry_set_text(GTK_ENTRY((GtkWidget*) gtk_object_get_data (GTK_OBJECT (CfgWnd),winame)), t)
#define SETLIST(winame,v)  gtk_list_select_item(GTK_LIST(GTK_COMBO((GtkWidget*) gtk_object_get_data (GTK_OBJECT (CfgWnd),winame))->list),v)

static GtkWidget * wndMain=0;

int main (int argc, char *argv[])
{
  GtkWidget *CfgWnd;
  FILE *in;char t[256];int len,val; 
  char * pB, * p; 

  if(argc!=2) return 0;
  if(strcmp(argv[1],"CFG")!=0 && strcmp(argv[1],"ABOUT")!=0)
   return 0;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  if (strcmp(argv[1],"ABOUT") == 0)
   {
    CfgWnd  = create_AboutWnd ();
    gtk_widget_show (CfgWnd);
    gtk_main ();
    return 0;
   }

  CfgWnd  = create_CfgWnd ();
  wndMain = CfgWnd;

  in = fopen("gpuPeopsMesaGL.cfg","rb");
  if(in)
   {
    pB=(char *)malloc(32767);
    memset(pB,0,32767);
    len = fread(pB, 1, 32767, in);
    fclose(in);
   }
  else pB=0;

  val=640;
  if(pB)
   {
    strcpy(t,"\nResX");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<10) val=10;
   }
  SETEDITVAL("edtXSize",val);

  val=480;
  if(pB)
   {
    strcpy(t,"\nResY");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<10) val=10;
   }
  SETEDITVAL("edtYSize",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nKeepRatio");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkKeepRatio");

  val=0;
  if(pB)
   {
    strcpy(t,"\nVRamSize");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0)    val=0;
    if(val>1024) val=1024;
   }
  SETEDITVAL("edtMaxTex",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\n15bitMdec");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chk15bitMdec");

  val=0;
  if(pB)
   {
    strcpy(t,"\nHiResTextures");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>2) val=2;
   }
  SETLIST("cmbHiresTex",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nFullScreen");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkFullScreen");

  val=0;
  if(pB)
   {
    strcpy(t,"\nScanLines");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkScanlines");

  val=0;
  if(pB)
   {
    strcpy(t,"\nScanLinesBlend");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<-1)  val=-1;
    if(val>255) val=255;
   }
  SETEDITVAL("edtScanBlend",val);

  val=1;
  if(pB)
   {
    strcpy(t,"\nFrameTextures");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>3) val=3;
   }
  SETLIST("cmbFrameTex",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nFrameAccess");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>4) val=4;
   }
  SETLIST("cmbFrameAcc",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nTexFilter");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>6) val=6;
   }
  SETLIST("cmbFilter",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nAdvancedBlend");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkABlend");

  val=0;
  if(pB)
   {
    strcpy(t,"\nDithering");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkDither");

  val=0;
  if(pB)
   {
    strcpy(t,"\nLineMode");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkLinemode");

  val=0;
  if(pB)
   {
    strcpy(t,"\nShowFPS");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkShowFPS");

  val=1;
  if(pB)
   {
    strcpy(t,"\nUseFrameLimit");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkFPSLimit");

  val=0;
  if(pB)
   {
    strcpy(t,"\nUseFrameSkip");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkFPSSkip");

  val=2;
  if(pB)
   {
    strcpy(t,"\nFPSDetection");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len)+1;
    if(val<1) val=1;
    if(val>2) val=2;
   }
  if(val==2) SETCHECK("rdbLimAuto");
  if(val==1) SETCHECK("rdbLimMan");

  val=200;
  if(pB)
   {
    strcpy(t,"\nFrameRate");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0)    val=0;
    if(val>1000) val=1000;
   }
  SETEDITVAL("edtFPSlim",val);

  val=2;
  if(pB)
   {
    strcpy(t,"\nOffscreenDrawing");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>4) val=4;
   }
  SETLIST("cmbOffscreen",val);

  val=1;
  if(pB)
   {
    strcpy(t,"\nOpaquePass");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkOpaque");

  val=0;
  if(pB)
   {
    strcpy(t,"\nAntiAlias");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkAntiA");

  val=0;
  if(pB)
   {
    strcpy(t,"\nTexQuality");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>4) val=4;
   }
  SETLIST("cmbQuality",val);

  val=0;
  if(pB)
   {
    strcpy(t,"\nMaskDetect");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkMaskBit");

  val=1;
  if(pB)
   {
    strcpy(t,"\nFastMdec");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkFastMdec");

  val=0;
  if(pB)
   {
    strcpy(t,"\nOGLExtensions");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkExtensions");

  val=0;
  if(pB)
   {
    strcpy(t,"\nScreenSmoothing");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkBlur");

  val=0;
  if(pB)
   {
    strcpy(t,"\nUseFixes");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
    if(val<0) val=0;
    if(val>1) val=1;
   }
  if(val) SETCHECK("chkGameFixes");

  val=0;
  if(pB)
   {
    strcpy(t,"\nCfgFixes");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
    if(p) val=atoi(p+len);
   }

  for(len=0;len<18;len++)
   {
    if(val & (1<<len))
     {
      sprintf(t,"chkFix%d",len);
      SETCHECK(t);
     }
   }

  if(pB) free(pB);

  gtk_widget_show (CfgWnd);
  gtk_main ();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void SetCfgVal(char * pB,char * pE,int val)
{
 char * p, *ps, *pC;char t[32];

 sprintf(t,"%d",val);

 p=strstr(pB,pE);
 if(p)
  {
   p=strstr(p,"=");
   if(!p) return;
   p++;
   while(*p && *p!='\n' && *p!='-' && (*p<'0' || *p>'9')) p++;
   if(*p==0 || *p=='\n') return;
   ps=p;
   while((*p>='0' && *p<='9') || *p=='-') p++;
   pC=(char *)malloc(32767);
   strcpy(pC,p);
   strcpy(ps,t);
   strcat(pB,pC);
   free(pC);
  }
 else
  {
   strcat(pB,pE);
   strcat(pB," = ");
   strcat(pB,t);
   strcat(pB,"\n");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////

#define GETEDITVAL(winame) atoi(gtk_entry_get_text(GTK_ENTRY((GtkWidget*) gtk_object_get_data (GTK_OBJECT (wndMain),winame))))
#define GETCHECK(winame)  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON ((GtkWidget*) gtk_object_get_data (GTK_OBJECT (wndMain),winame)))?1:0
#define GETLIST(winame) atoi(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO((GtkWidget*) gtk_object_get_data (GTK_OBJECT (wndMain),winame))->entry)))

void SaveConfig(void)
{
  FILE *in;int len,val;char * pB;char t[16];

  pB=(char *)malloc(32767);
  memset(pB,0,32767);

  in = fopen("gpuPeopsMesaGL.cfg","rb");
  if(in)
   {
    len = fread(pB, 1, 32767, in);
    fclose(in); 
   }

 ///////////////////////////////////////////////////////////////////////////////

 val=GETEDITVAL("edtXSize");
 if(val<10) val=10;
 SetCfgVal(pB,"\nResX",val);

 val=GETEDITVAL("edtYSize");
 if(val<10) val=10;
 SetCfgVal(pB,"\nResY",val);

 val=GETCHECK("chkKeepRatio");
 SetCfgVal(pB,"\nKeepRatio",val);

 val=GETEDITVAL("edtMaxTex");
 if(val<0)    val=0;
 if(val>1024) val=1024;
 SetCfgVal(pB,"\nVRamSize",val);

 val=GETCHECK("chk15bitMdec");
 SetCfgVal(pB,"\n15bitMdec",val);

 val=GETLIST("cmbHiresTex");
 SetCfgVal(pB,"\nHiResTextures",val);

 val=GETCHECK("chkFullScreen");
 SetCfgVal(pB,"\nFullScreen",val);

 val=GETCHECK("chkScanlines");
 SetCfgVal(pB,"\nScanLines",val);

 val=GETEDITVAL("edtScanBlend");
 if(val<-1)  val=-1;
 if(val>255) val=255;
 SetCfgVal(pB,"\nScanLinesBlend",val);

 val=GETLIST("cmbFrameTex");
 SetCfgVal(pB,"\nFrameTextures",val);

 val=GETLIST("cmbFrameAcc");
 SetCfgVal(pB,"\nFrameAccess",val);

 val=GETLIST("cmbFilter");
 SetCfgVal(pB,"\nTexFilter",val);

 val=GETCHECK("chkABlend");
 SetCfgVal(pB,"\nAdvancedBlend",val);

 val=GETCHECK("chkDither");
 SetCfgVal(pB,"\nDithering",val);

 val=GETCHECK("chkLinemode");
 SetCfgVal(pB,"\nLineMode",val);

 val=GETCHECK("chkShowFPS");
 SetCfgVal(pB,"\nShowFPS",val);

 val=GETCHECK("chkFPSLimit");
 SetCfgVal(pB,"\nUseFrameLimit",val);

 val=GETCHECK("chkFPSSkip");
 SetCfgVal(pB,"\nUseFrameSkip",val);

 val=GETCHECK("rdbLimAuto");
 if(val) val=1; else val=0;
 SetCfgVal(pB,"\nFPSDetection",val);

 val=GETEDITVAL("edtFPSlim");
 if(val<0)    val=0;
 if(val>1000) val=1000;
 SetCfgVal(pB,"\nFrameRate",val);

 val=GETLIST("cmbOffscreen");
 SetCfgVal(pB,"\nOffscreenDrawing",val);

 val=GETCHECK("chkOpaque");
 SetCfgVal(pB,"\nOpaquePass",val);

 val=GETCHECK("chkAntiA");
 SetCfgVal(pB,"\nAntiAlias",val);

 val=GETLIST("cmbQuality");
 SetCfgVal(pB,"\nTexQuality",val);

 val=GETCHECK("chkMaskBit");
 SetCfgVal(pB,"\nMaskDetect",val);

 val=GETCHECK("chkFastMdec");
 SetCfgVal(pB,"\nFastMdec",val);

 val=GETCHECK("chkExtensions");
 SetCfgVal(pB,"\nOGLExtensions",val);

 val=GETCHECK("chkBlur");
 SetCfgVal(pB,"\nScreenSmoothing",val);

 val=GETCHECK("chkGameFixes");
 SetCfgVal(pB,"\nUseFixes",val);

 val=0;
 for(len=0;len<18;len++)
  {
   sprintf(t,"chkFix%d",len);
   if(GETCHECK(t)) val|=(1<<len);
  }
 SetCfgVal(pB,"\nCfgFixes",val);

 ///////////////////////////////////////////////////////////////////////////////

 if((in=fopen("gpuPeopsMesaGL.cfg","wb"))!=NULL)
  {
   fwrite(pB,strlen(pB),1,in);
   fclose(in);
  }

 free(pB);
}



