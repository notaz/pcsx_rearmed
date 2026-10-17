#include <stdio.h>
#include <stdlib.h>

#include "../../plugins/dfxvideo/gpulib_if.c"

struct psx_gpu gpu;
const unsigned char cmd_lengths[256]={0};

static unsigned quantize(unsigned value, unsigned threshold)
{
 unsigned result=value/8;
 if(result<31 && value%8>threshold)
  result++;
 return result;
}

int main(void)
{
 unsigned x,y,r,g,b,mask,expected,actual;
 unsigned cases=0;
 unsigned short *pixel;

 psxVuw=(unsigned short *)calloc(4096,sizeof(*psxVuw));
 if(!psxVuw)
  return 2;

 for(y=0;y<4;y++)
  for(x=0;x<4;x++)
   for(r=0;r<256;r++)
    for(g=0;g<256;g++)
     for(mask=0;mask<=0x8000;mask+=0x8000)
      {
       unsigned threshold=dithertable[y*4+x];
       b=(r*13+g*29)&255;
       pixel=psxVuw+y*1024+x;
       expected=quantize(r,threshold)|(quantize(g,threshold)<<5)
        |(quantize(b,threshold)<<10)|mask;
       *pixel=0;
       Dither16(pixel,r,g,b,(unsigned short)mask);
       actual=GETLE16(pixel);
       if(actual!=expected)
        {
         fprintf(stderr,"dither mismatch x=%u y=%u rgb=%u,%u,%u mask=%04x: %04x != %04x\n",
          x,y,r,g,b,mask,actual,expected);
         free(psxVuw);
         return 1;
        }
       cases++;
      }

 free(psxVuw);
 printf("DITHER_OK cases=%u\n",cases);
 return 0;
}
