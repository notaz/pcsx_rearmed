/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*   Copyright (C) 2011 notaz                                              *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu.h"

#define u8 uint8_t
#define s8 int8_t
#define u16 uint16_t
#define s16 int16_t
#define u32 uint32_t
#define s32 int32_t
#define s64 int64_t

#define INLINE static

#define	FRAME_BUFFER_SIZE  (1024*512*2)
#define	FRAME_WIDTH        1024
#define	FRAME_HEIGHT       512
#define	FRAME_OFFSET(x,y)  (((y)<<10)+(x))

static bool isSkip = false; /* skip frame (info coming from GPU) */
static int linesInterlace = 0;  /* internal lines interlace */

#define alt_fps 0

static bool light = true; /* lighting */
static bool blend = true; /* blending */
static bool FrameToRead = false; /* load image in progress */
static bool FrameToWrite = false; /* store image in progress */

static bool enableAbbeyHack = false; /* Abe's Odyssey hack */

static u8 BLEND_MODE;
static u8 TEXT_MODE;
static u8 Masking;

static u16 PixelMSB;
static u16 PixelData;

///////////////////////////////////////////////////////////////////////////////
//  GPU Global data
///////////////////////////////////////////////////////////////////////////////

//  Dma Transfers info
static s32		px,py;
static s32		x_end,y_end;
static u16*  pvram;

static s32 PacketCount;
static s32 PacketIndex;

//  Rasterizer status
static u32 TextureWindow [4];
static u32 DrawingArea   [4];
static u32 DrawingOffset [2];

static u16* TBA;
static u16* CBA;

//  Inner Loops
static s32   u4, du4;
static s32   v4, dv4;
static s32   r4, dr4;
static s32   g4, dg4;
static s32   b4, db4;
static u32   lInc;
static u32   tInc, tMsk;

union GPUPacket
{
	u32 U4[16];
	s32 S4[16];
	u16 U2[32];
	s16 S2[32];
	u8  U1[64];
	s8  S1[64];
};

static GPUPacket PacketBuffer;
static u16  *GPU_FrameBuffer;
static u32   GPU_GP1;

///////////////////////////////////////////////////////////////////////////////

#include "../gpu_unai/gpu_fixedpoint.h"

//  Inner loop driver instanciation file
#include "../gpu_unai/gpu_inner.h"

//  GPU Raster Macros
#define	GPU_RGB16(rgb)        ((((rgb)&0xF80000)>>9)|(((rgb)&0xF800)>>6)|(((rgb)&0xF8)>>3))

#define GPU_EXPANDSIGN_POLY(x)  (((s32)(x)<<20)>>20)
//#define GPU_EXPANDSIGN_POLY(x)  (((s32)(x)<<21)>>21)
#define GPU_EXPANDSIGN_SPRT(x)  (((s32)(x)<<21)>>21)

//#define	GPU_TESTRANGE(x)      { if((u32)(x+1024) > 2047) return; }
#define	GPU_TESTRANGE(x)      { if ((x<-1023) || (x>1023)) return; }

#define	GPU_SWAP(a,b,t)	{(t)=(a);(a)=(b);(b)=(t);}

// GPU internal image drawing functions
#include "../gpu_unai/gpu_raster_image.h"

// GPU internal line drawing functions
#include "../gpu_unai/gpu_raster_line.h"

// GPU internal polygon drawing functions
#include "../gpu_unai/gpu_raster_polygon.h"

// GPU internal sprite drawing functions
#include "../gpu_unai/gpu_raster_sprite.h"

// GPU command buffer execution/store
#include "../gpu_unai/gpu_command.h"

#define unai_do_prim(cmd, list, len) \
  memcpy(PacketBuffer.U4, list, (len) * 4); \
  gpuSendPacketFunction(cmd)

/////////////////////////////////////////////////////////////////////////////

int renderer_init(void)
{
	GPU_FrameBuffer = (u16 *)gpu.vram;

	// s_invTable
	for(int i=1;i<=(1<<TABLE_BITS);++i)
	{
		double v = 1.0 / double(i);
		#ifdef GPU_TABLE_10_BITS
		v *= double(0xffffffff>>1);
		#else
		v *= double(0x80000000);
		#endif
		s_invTable[i-1]=s32(v);
	}

	return 0;
}

extern const unsigned char cmd_lengths[256];

void do_cmd_list(unsigned int *list, int list_len)
{
  unsigned int cmd, len;

  unsigned int *list_end = list + list_len;

  for (; list < list_end; list += 1 + len)
  {
    cmd = *list >> 24;
    len = cmd_lengths[cmd];

    switch(cmd)
    {
      case 0x48 ... 0x4F:
      {
        u32 num_vertexes = 1;
        u32 *list_position = &(list[2]);
        u32 PRIM = cmd;

        memcpy(&PacketBuffer.U4[0], list, 4 * 3);
        gpuDrawLF(gpuPixelDrivers [ (Blending_Mode | Masking | Blending | (PixelMSB>>3)) >> 1]);

        while(1)
        {
          if((*list_position & 0xf000f000) == 0x50005000 || list_position >= list_end)
            break;

          PacketBuffer.U4[1] = PacketBuffer.U4[2];
          PacketBuffer.U4[2] = *list_position++;
          gpuDrawLF(gpuPixelDrivers [ (Blending_Mode | Masking | Blending | (PixelMSB>>3)) >> 1]);

          num_vertexes++;
        }

        if(num_vertexes > 2)
          len += (num_vertexes - 2);

        break;
      }

      case 0x58 ... 0x5F:
      {
        u32 num_vertexes = 1;
        u32 *list_position = &(list[2]);
        u32 PRIM = cmd;

        memcpy(&PacketBuffer.U4[0], list, 4 * 4);
        gpuDrawLG(gpuPixelDrivers [ (Blending_Mode | Masking | Blending | (PixelMSB>>3)) >> 1]);

        while(1)
        {
          if((*list_position & 0xf000f000) == 0x50005000 || list_position >= list_end)
            break;

          PacketBuffer.U4[0] = PacketBuffer.U4[2];
          PacketBuffer.U4[1] = PacketBuffer.U4[3];
          PacketBuffer.U4[2] = *list_position++;
          PacketBuffer.U4[3] = *list_position++;
          gpuDrawLG(gpuPixelDrivers [ (Blending_Mode | Masking | Blending | (PixelMSB>>3)) >> 1]);

          num_vertexes++;
        }

        if(num_vertexes > 2)
          len += (num_vertexes - 2) * 2;

        break;
      }

#ifdef TEST
      case 0xA0:          //  sys -> vid
      {
        u32 load_width = list[2] & 0xffff;
        u32 load_height = list[2] >> 16;
        u32 load_size = load_width * load_height;

        len += load_size / 2;
        break;
      }
#endif

      default:
        unai_do_prim(cmd, list, len + 1);
        break;
    }
  }
}

void renderer_sync_ecmds(uint32_t *ecmds)
{
  unai_do_prim(0xe1, &ecmds[1], 1);
  unai_do_prim(0xe2, &ecmds[2], 1);
  unai_do_prim(0xe3, &ecmds[3], 1);
  unai_do_prim(0xe4, &ecmds[4], 1);
  unai_do_prim(0xe5, &ecmds[5], 1);
  unai_do_prim(0xe6, &ecmds[6], 1);
}

void renderer_invalidate_caches(int x, int y, int w, int h)
{
}

void renderer_flush_queues(void)
{
}

#ifndef TEST

#include "../../frontend/plugin_lib.h"

void renderer_set_config(const struct rearmed_cbs *cbs)
{
  enableAbbeyHack = cbs->gpu_unai.abe_hack;
  light = !cbs->gpu_unai.no_light;
  blend = !cbs->gpu_unai.no_blend;
}

#endif
