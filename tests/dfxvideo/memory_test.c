#include <stdio.h>
#include <string.h>

#include "../../plugins/dfxvideo/gpulib_if.c"

struct psx_gpu gpu;
const unsigned char cmd_lengths[256]={0};

static unsigned read_bytes(const unsigned char *p)
{
 return p[0]|((unsigned)p[1]<<8);
}

static unsigned alias_access(uint16_t *p, uint16_t *q, unsigned value)
{
 unsigned first=GETLE16(p);
 PUTLE16(q,value);
 return first+GETLE16(p);
}

int main(void)
{
 uint32_t storage[2];
 unsigned char *bytes=(unsigned char *)storage;
 unsigned offset,value,i,expected,actual,cases=0;
 int signed_expected;
 uint16_t *p;

 for(offset=0;offset<sizeof(storage);offset+=2)
  for(value=0;value<65536;value++)
   {
    p=(uint16_t *)(bytes+offset);
    memset(storage,0xa5,sizeof(storage));
    bytes[offset]=(unsigned char)value;
    bytes[offset+1]=(unsigned char)(value>>8);
    signed_expected=value<32768 ? (int)value : (int)value-65536;
    if(GETLE16(p)!=value || GETLEs16(p)!=signed_expected)
     goto fail;
    PUTLE16(p,value^0xffff);
    if(read_bytes(bytes+offset)!=(value^0xffff))
     goto fail;
    for(i=0;i<sizeof(storage);i++)
     if(i!=offset && i!=offset+1 && bytes[i]!=0xa5)
      goto fail;
    cases++;
   }

 for(value=0;value<65536;value++)
  {
   PUTLE32(storage,0x12345678);
   p=(uint16_t *)storage;
   actual=alias_access(p,p,value);
   expected=0x5678+value;
   if(actual!=expected || read_bytes(bytes)!=value
      || read_bytes(bytes+2)!=0x1234)
    goto fail;
   PUTLE32(storage,0x12345678);
   actual=alias_access(p,p+1,value);
   if(actual!=0x5678*2 || read_bytes(bytes)!=0x5678
      || GETLE32(storage)!=((value<<16)|0x5678))
    goto fail;
   PUTLE32(storage,0x89abcdef);
   if(GETLE16(p)!=0xcdef || GETLE16(p+1)!=0x89ab)
    goto fail;
   cases++;
  }

 printf("RENDERER_MEMORY_OK cases=%u\n",cases);
 return 0;

fail:
 fprintf(stderr,"renderer memory mismatch offset=%u value=%04x\n",offset,value);
 return 1;
}
