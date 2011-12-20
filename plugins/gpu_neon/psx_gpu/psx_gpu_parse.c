/*
 * Copyright (C) 2011 Gilead Kutnick "Exophase" <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <stdio.h>

#include "common.h"

const u8 command_lengths[256] =
{
	0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 00
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 10
	3,  3,  3,  3,  6,  6,  6,  6,  4,  4,  4,  4,  8,  8,  8,  8,   // 20
	5,  5,  5,  5,  8,  8,  8,  8,  7,  7,  7,  7,  11, 11, 11, 11,  // 30
	2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,   // 40
	3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,   // 50
	2,  2,  2,  2,  3,  3,  3,  3,  1,  1,  1,  1,  1,  1,  1,  1,   // 60
	1,  1,  1,  1,  2,  2,  2,  2,  1,  1,  1,  1,  2,  2,  2,  2,   // 70
	3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 80
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 90
	2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // a0
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // b0
	2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // c0
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // d0
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // e0
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0    // f0
};

void update_texture_ptr(psx_gpu_struct *psx_gpu)
{
  u8 *texture_ptr;

  switch((psx_gpu->render_state_base >> 8) & 0x3)
  {
    default:
    case TEXTURE_MODE_4BPP:
#ifdef TEXTURE_CACHE_4BPP
      texture_ptr = psx_gpu->texture_4bpp_cache[psx_gpu->current_texture_page];
      texture_ptr += psx_gpu->texture_window_x & 0xF;
      texture_ptr += (psx_gpu->texture_window_y & 0xF) << 4;
      texture_ptr += (psx_gpu->texture_window_x >> 4) << 8;
      texture_ptr += (psx_gpu->texture_window_y >> 4) << 12;
#else
      texture_ptr = (u8 *)(psx_gpu->vram_ptr);
      texture_ptr += (psx_gpu->current_texture_page & 0xF) * 128;
      texture_ptr += ((psx_gpu->current_texture_page >> 4) * 256) * 2048;
      texture_ptr += psx_gpu->texture_window_x / 2;
      texture_ptr += (psx_gpu->texture_window_y) * 2048;
#endif
      break;

    case TEXTURE_MODE_8BPP:
#ifdef TEXTURE_CACHE_8BPP
      if(psx_gpu->current_texture_page & 0x1)
      {
        texture_ptr =
         psx_gpu->texture_8bpp_odd_cache[psx_gpu->current_texture_page >> 1];
      }
      else
      {
        texture_ptr =
         psx_gpu->texture_8bpp_even_cache[psx_gpu->current_texture_page >> 1];
      }
      
      texture_ptr += (psx_gpu->texture_window_y & 0xF) << 4;
      texture_ptr += (psx_gpu->texture_window_x >> 4) << 8;
      texture_ptr += (psx_gpu->texture_window_y >> 4) << 12;
#else
      texture_ptr = (u8 *)(psx_gpu->vram_ptr);
      texture_ptr += (psx_gpu->current_texture_page & 0xF) * 128;
      texture_ptr += ((psx_gpu->current_texture_page >> 4) * 256) * 2048;
      texture_ptr += psx_gpu->texture_window_x;
      texture_ptr += (psx_gpu->texture_window_y) * 2048;
#endif
      break;

    case TEXTURE_MODE_16BPP:
      texture_ptr = (u8 *)(psx_gpu->vram_ptr);
      texture_ptr += (psx_gpu->current_texture_page & 0xF) * 128;
      texture_ptr += ((psx_gpu->current_texture_page >> 4) * 256) * 2048;
      texture_ptr += psx_gpu->texture_window_x * 2;
      texture_ptr += (psx_gpu->texture_window_y) * 2048;
      break;
  }

  psx_gpu->texture_page_ptr = texture_ptr;  
}

void set_texture(psx_gpu_struct *psx_gpu, u32 texture_settings)
{
  if(psx_gpu->texture_settings != texture_settings)
  {
    u32 new_texture_page = texture_settings & 0x1F;
    u32 texture_mode = (texture_settings >> 7) & 0x3;
    u32 render_state_base = psx_gpu->render_state_base;

    if(psx_gpu->current_texture_page != new_texture_page)
      flush_render_block_buffer(psx_gpu);

    render_state_base &= ~(0xF << 6);
    render_state_base |= ((texture_settings >> 5) & 0xF) << 6;

    psx_gpu->render_state_base = render_state_base;

    psx_gpu->current_texture_mask = 0x1 << new_texture_page;

    if(texture_mode == TEXTURE_MODE_8BPP)
    {     
      // In 8bpp mode 256x256 takes up two pages. If it's on the right edge it
      // wraps back around to the left edge.
      u32 adjacent_texture_page = ((texture_settings + 1) & 0xF) | (texture_settings & 0x10);
      psx_gpu->current_texture_mask |= 0x1 << adjacent_texture_page;

      if((psx_gpu->last_8bpp_texture_page ^ new_texture_page) & 0x1)
      {
        u32 dirty_textures_8bpp_alternate_mask =
         psx_gpu->dirty_textures_8bpp_alternate_mask;
        psx_gpu->dirty_textures_8bpp_alternate_mask =
         psx_gpu->dirty_textures_8bpp_mask;
        psx_gpu->dirty_textures_8bpp_mask = dirty_textures_8bpp_alternate_mask;
      }

      psx_gpu->last_8bpp_texture_page = new_texture_page;
    }

    psx_gpu->current_texture_page = new_texture_page;
    psx_gpu->texture_settings = texture_settings;

    update_texture_ptr(psx_gpu);
  }
}

void set_clut(psx_gpu_struct *psx_gpu, u32 clut_settings)
{
  if(psx_gpu->clut_settings != clut_settings)
  {
    flush_render_block_buffer(psx_gpu);
    psx_gpu->clut_settings = clut_settings;
    psx_gpu->clut_ptr = psx_gpu->vram_ptr + ((clut_settings & 0x7FFF) * 16);
  }
}

void set_triangle_color(psx_gpu_struct *psx_gpu, u32 triangle_color)
{
  if(psx_gpu->triangle_color != triangle_color)
  {
    flush_render_block_buffer(psx_gpu);
    psx_gpu->triangle_color = triangle_color;
  }
}

#define sign_extend_12bit(value)                                               \
  (((s32)((value) << 20)) >> 20)                                               \

#define get_vertex_data_xy(vertex_number, offset16)                            \
  vertexes[vertex_number].x =                                                  \
   sign_extend_12bit(list_s16[offset16]) + psx_gpu->offset_x;                  \
  vertexes[vertex_number].y =                                                  \
   sign_extend_12bit(list_s16[(offset16) + 1]) + psx_gpu->offset_y;            \

#define get_vertex_data_uv(vertex_number, offset16)                            \
  vertexes[vertex_number].u = list_s16[offset16] & 0xFF;                       \
  vertexes[vertex_number].v = (list_s16[offset16] >> 8) & 0xFF                 \

#define get_vertex_data_rgb(vertex_number, offset32)                           \
  vertexes[vertex_number].r = list[offset32] & 0xFF;                           \
  vertexes[vertex_number].g = (list[offset32] >> 8) & 0xFF;                    \
  vertexes[vertex_number].b = (list[offset32] >> 16) & 0xFF                    \

#define get_vertex_data_xy_uv(vertex_number, offset16)                         \
  get_vertex_data_xy(vertex_number, offset16);                                 \
  get_vertex_data_uv(vertex_number, (offset16) + 2)                            \

#define get_vertex_data_xy_rgb(vertex_number, offset16)                        \
  get_vertex_data_rgb(vertex_number, (offset16) / 2);                          \
  get_vertex_data_xy(vertex_number, (offset16) + 2);                           \

#define get_vertex_data_xy_uv_rgb(vertex_number, offset16)                     \
  get_vertex_data_rgb(vertex_number, (offset16) / 2);                          \
  get_vertex_data_xy(vertex_number, (offset16) + 2);                           \
  get_vertex_data_uv(vertex_number, (offset16) + 4);                           \

#define set_vertex_color_constant(vertex_number, color)                        \
  vertexes[vertex_number].r = color & 0xFF;                                    \
  vertexes[vertex_number].g = (color >> 8) & 0xFF;                             \
  vertexes[vertex_number].b = (color >> 16) & 0xFF                             \

#define get_vertex_data_xy_rgb_constant(vertex_number, offset16, color)        \
  get_vertex_data_xy(vertex_number, offset16);                                 \
  set_vertex_color_constant(vertex_number, color)                              \

vertex_struct vertexes[4] __attribute__((aligned(32)));

void gpu_parse(psx_gpu_struct *psx_gpu, u32 *list, u32 size)
{
  u32 current_command, command_length;
  
  u32 *list_end = list + (size / 4);

  for(; list < list_end; list += 1 + command_length)
  {
  	s16 *list_s16 = (void *)list;
  	current_command = *list >> 24;
  	command_length = command_lengths[current_command];
  
  	switch(current_command)
  	{
  		case 0x00:
  			break;
  
  		case 0x02:
        render_block_fill(psx_gpu, list[0] & 0xFFFFFF, list_s16[2], list_s16[3],
         list_s16[4] & 0x3FF, list_s16[5] & 0x3FF);
  			break;
  
  		case 0x20 ... 0x23:
      {
        set_triangle_color(psx_gpu, list[0] & 0xFFFFFF);
  
        get_vertex_data_xy(0, 2);
        get_vertex_data_xy(1, 4);
        get_vertex_data_xy(2, 6);
  
        render_triangle(psx_gpu, vertexes, current_command);
  			break;
      }
  
  		case 0x24 ... 0x27:
      {
        set_clut(psx_gpu, list_s16[5]);
        set_texture(psx_gpu, list_s16[9]);
        set_triangle_color(psx_gpu, list[0] & 0xFFFFFF);
  
        get_vertex_data_xy_uv(0, 2);
        get_vertex_data_xy_uv(1, 6);
        get_vertex_data_xy_uv(2, 10);
  
        render_triangle(psx_gpu, vertexes, current_command);
  			break;
      }
  
  		case 0x28 ... 0x2B:
      {
        set_triangle_color(psx_gpu, list[0] & 0xFFFFFF);
  
        get_vertex_data_xy(0, 2);
        get_vertex_data_xy(1, 4);
        get_vertex_data_xy(2, 6);
        get_vertex_data_xy(3, 8);
  
        render_triangle(psx_gpu, vertexes, current_command);
        render_triangle(psx_gpu, &(vertexes[1]), current_command);
  			break;
      }
  
  		case 0x2C ... 0x2F:
      {
        set_clut(psx_gpu, list_s16[5]);
        set_texture(psx_gpu, list_s16[9]);
        set_triangle_color(psx_gpu, list[0] & 0xFFFFFF);
  
        get_vertex_data_xy_uv(0, 2);   
        get_vertex_data_xy_uv(1, 6);   
        get_vertex_data_xy_uv(2, 10);  
        get_vertex_data_xy_uv(3, 14);
  
        render_triangle(psx_gpu, vertexes, current_command);
        render_triangle(psx_gpu, &(vertexes[1]), current_command);
  			break;
      }
  
  		case 0x30 ... 0x33:
      {
        get_vertex_data_xy_rgb(0, 0);
        get_vertex_data_xy_rgb(1, 4);
        get_vertex_data_xy_rgb(2, 8);
  
        render_triangle(psx_gpu, vertexes, current_command);
  			break;
      }
  
  		case 0x34:
  		case 0x35:
  		case 0x36:
  		case 0x37:
      {
        set_clut(psx_gpu, list_s16[5]);
        set_texture(psx_gpu, list_s16[11]);
  
        get_vertex_data_xy_uv_rgb(0, 0);
        get_vertex_data_xy_uv_rgb(1, 6);
        get_vertex_data_xy_uv_rgb(2, 12);

        render_triangle(psx_gpu, vertexes, current_command);
  			break;
      }
  
  		case 0x38:
  		case 0x39:
  		case 0x3A:
  		case 0x3B:
      {
        get_vertex_data_xy_rgb(0, 0);
        get_vertex_data_xy_rgb(1, 4);
        get_vertex_data_xy_rgb(2, 8);
        get_vertex_data_xy_rgb(3, 12);
  
        render_triangle(psx_gpu, vertexes, current_command);
        render_triangle(psx_gpu, &(vertexes[1]), current_command);
  			break;
      }
  
  		case 0x3C:
  		case 0x3D:
  		case 0x3E:
  		case 0x3F:
      {
        set_clut(psx_gpu, list_s16[5]);
        set_texture(psx_gpu, list_s16[11]);
  
        get_vertex_data_xy_uv_rgb(0, 0);
        get_vertex_data_xy_uv_rgb(1, 6);
        get_vertex_data_xy_uv_rgb(2, 12);
        get_vertex_data_xy_uv_rgb(3, 18);
  
        render_triangle(psx_gpu, vertexes, current_command);
        render_triangle(psx_gpu, &(vertexes[1]), current_command);
  			break;
      }
  
  		case 0x40 ... 0x47:
      {
        vertexes[0].x = list_s16[2] + psx_gpu->offset_x;
        vertexes[0].y = list_s16[3] + psx_gpu->offset_y;
        vertexes[1].x = list_s16[4] + psx_gpu->offset_x;
        vertexes[1].y = list_s16[5] + psx_gpu->offset_y;

        render_line(psx_gpu, vertexes, current_command, list[0]);
  			break;
      }
  
  		case 0x48 ... 0x4F:
      {
        u32 num_vertexes = 1;
        u32 *list_position = &(list[2]);
        u32 xy = list[1];

        vertexes[1].x = (xy & 0xFFFF) + psx_gpu->offset_x;
        vertexes[1].y = (xy >> 16) + psx_gpu->offset_y;
      
        while(1)
        {
          xy = *list_position;
          if(xy == 0x55555555)
            break;

          vertexes[0] = vertexes[1];

          vertexes[1].x = (xy & 0xFFFF) + psx_gpu->offset_x;
          vertexes[1].y = (xy >> 16) + psx_gpu->offset_y;

          list_position++;
          num_vertexes++;

          render_line(psx_gpu, vertexes, current_command, list[0]);
        }

        if(num_vertexes > 2)
          command_length += (num_vertexes - 2);

  			break;
      }
  
  		case 0x50 ... 0x57:
      {
        vertexes[0].r = list[0] & 0xFF;
        vertexes[0].g = (list[0] >> 8) & 0xFF;
        vertexes[0].b = (list[0] >> 16) & 0xFF;
        vertexes[0].x = list_s16[2] + psx_gpu->offset_x;
        vertexes[0].y = list_s16[3] + psx_gpu->offset_y;

        vertexes[1].r = list[2] & 0xFF;
        vertexes[1].g = (list[2] >> 8) & 0xFF;
        vertexes[1].b = (list[2] >> 16) & 0xFF;
        vertexes[1].x = list_s16[6] + psx_gpu->offset_x;
        vertexes[1].y = list_s16[7] + psx_gpu->offset_y;

        render_line(psx_gpu, vertexes, current_command, 0);
  			break;
      }
 
      case 0x58 ... 0x5F:
      {
        u32 num_vertexes = 1;
        u32 *list_position = &(list[2]);
        u32 color = list[0];
        u32 xy = list[1];

        vertexes[1].r = color & 0xFF;
        vertexes[1].g = (color >> 8) & 0xFF;
        vertexes[1].b = (color >> 16) & 0xFF;
        vertexes[1].x = (xy & 0xFFFF) + psx_gpu->offset_x;
        vertexes[1].y = (xy >> 16) + psx_gpu->offset_y;
      
        while(1)
        {
          color = list_position[0];
          if(color == 0x55555555)
            break;

          xy = list_position[1];

          vertexes[0] = vertexes[1];

          vertexes[1].r = color & 0xFF;
          vertexes[1].g = (color >> 8) & 0xFF;
          vertexes[1].b = (color >> 16) & 0xFF;
          vertexes[1].x = (xy & 0xFFFF) + psx_gpu->offset_x;
          vertexes[1].y = (xy >> 16) + psx_gpu->offset_y;

          list_position += 2;
          num_vertexes++;

          render_line(psx_gpu, vertexes, current_command, 0);
        }

        if(num_vertexes > 2)
          command_length += ((num_vertexes * 2) - 2);

  			break;
      }
  
  		case 0x60 ... 0x63:
      {        
        u32 x = list_s16[2] + psx_gpu->offset_x;
        u32 y = list_s16[3] + psx_gpu->offset_y;
        u32 width = list_s16[4] & 0x3FF;
        u32 height = list_s16[5] & 0x1FF;

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;

        render_sprite(psx_gpu, x, y, 0, 0, width, height, current_command, list[0]);
  			break;
      }
  
  		case 0x64 ... 0x67:
      {        
        u32 x = list_s16[2] + psx_gpu->offset_x;
        u32 y = list_s16[3] + psx_gpu->offset_y;
        u32 uv = list_s16[4];
        u32 width = list_s16[6] & 0x3FF;
        u32 height = list_s16[7] & 0x1FF;

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;
        set_clut(psx_gpu, list_s16[5]);

        render_sprite(psx_gpu, x, y, uv & 0xFF, (uv >> 8) & 0xFF, width, height,
         current_command, list[0]);
  			break;
      }
  
  		case 0x68:
  		case 0x69:
  		case 0x6A:
  		case 0x6B:
      {
        s32 x = list_s16[2] + psx_gpu->offset_x;
        s32 y = list_s16[3] + psx_gpu->offset_y;

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;

        render_sprite(psx_gpu, x, y, 0, 0, 1, 1, current_command, list[0]);
  			break;
      }
  
  		case 0x70:
  		case 0x71:
  		case 0x72:
  		case 0x73:
      {        
        s32 x = list_s16[2] + psx_gpu->offset_x;
        s32 y = list_s16[3] + psx_gpu->offset_y;

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;

        render_sprite(psx_gpu, x, y, 0, 0, 8, 8, current_command, list[0]);
  			break;
      }
  
  		case 0x74:
  		case 0x75:
  		case 0x76:
  		case 0x77:
      {        
        s32 x = list_s16[2] + psx_gpu->offset_x;
        s32 y = list_s16[3] + psx_gpu->offset_y;
        u32 uv = list_s16[4];

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;
        set_clut(psx_gpu, list_s16[5]);

        render_sprite(psx_gpu, x, y, uv & 0xFF, (uv >> 8) & 0xFF, 8, 8,
         current_command, list[0]);
  			break;
      }
  
  		case 0x78:
  		case 0x79:
  		case 0x7A:
  		case 0x7B:
      {        
        s32 x = list_s16[2] + psx_gpu->offset_x;
        s32 y = list_s16[3] + psx_gpu->offset_y;

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;
        render_sprite(psx_gpu, x, y, 0, 0, 16, 16, current_command, list[0]);
  			break;
      }
  
  		case 0x7C:
  		case 0x7D:
  		case 0x7E:
  		case 0x7F:
      {        
        s32 x = list_s16[2] + psx_gpu->offset_x;
        s32 y = list_s16[3] + psx_gpu->offset_y;
        u32 uv = list_s16[4];

        psx_gpu->primitive_color = list[0] & 0xFFFFFF;
        set_clut(psx_gpu, list_s16[5]);

        render_sprite(psx_gpu, x, y, uv & 0xFF, (uv >> 8) & 0xFF, 16, 16,
         current_command, list[0]);
  			break;
      }
  
  		case 0x80:          //  vid -> vid
        render_block_move(psx_gpu, list_s16[2] & 0x3FF, list_s16[3] & 0x1FF,
         list_s16[4] & 0x3FF, list_s16[5] & 0x1FF, list_s16[6], list_s16[7]);
  			break;
  
  		case 0xA0:          //  sys -> vid
      {
        u32 load_x = list_s16[2];
        u32 load_y = list_s16[3];
        u32 load_width = list_s16[4];
        u32 load_height = list_s16[5];
        u32 load_size = load_width * load_height;
  
        command_length += load_size / 2;
  
        render_block_copy(psx_gpu, (u16 *)&(list_s16[6]), load_x, load_y,
         load_width, load_height, load_width);
  			break;
      }
  
  		case 0xC0:          //  vid -> sys
  			break;
  
  		case 0xE1:
        set_texture(psx_gpu, list[0] & 0x1FF);
        if(list[0] & (1 << 9))
          psx_gpu->render_state_base |= RENDER_STATE_DITHER;
        else
          psx_gpu->render_state_base &= ~RENDER_STATE_DITHER;

        psx_gpu->display_area_draw_enable = (list[0] >> 10) & 0x1;
  			break;
  
  		case 0xE2:
      {
        // TODO: Clean
        u32 texture_window_settings = list[0];
        u32 tmp, x, y, w, h;

        if(texture_window_settings != psx_gpu->texture_window_settings)
        {
          tmp = (texture_window_settings & 0x1F) | 0x20;
          for(w = 8; (tmp & 1) == 0; tmp >>= 1, w <<= 1);

          tmp = ((texture_window_settings >> 5) & 0x1f) | 0x20;
          for (h = 8; (tmp & 1) == 0; tmp >>= 1, h <<= 1);

          tmp = 32 - (w >> 3);
          x = ((texture_window_settings >> 10) & tmp) << 3;

          tmp = 32 - (h >> 3);
          y = ((texture_window_settings >> 15) & tmp) << 3;

          flush_render_block_buffer(psx_gpu);
          
          psx_gpu->texture_window_x = x;
          psx_gpu->texture_window_y = y;
          psx_gpu->texture_mask_width = w - 1;
          psx_gpu->texture_mask_height = h - 1;

          update_texture_ptr(psx_gpu);
        }
        break;
  		}
  
  		case 0xE3:
        psx_gpu->viewport_start_x = list[0] & 0x3FF;
        psx_gpu->viewport_start_y = (list[0] >> 10) & 0x1FF;

#ifdef TEXTURE_CACHE_4BPP
        psx_gpu->viewport_mask =
         texture_region_mask(psx_gpu->viewport_start_x,
         psx_gpu->viewport_start_y, psx_gpu->viewport_end_x,
         psx_gpu->viewport_end_y);
#endif
  			break;
  
  		case 0xE4:
        psx_gpu->viewport_end_x = list[0] & 0x3FF;
        psx_gpu->viewport_end_y = (list[0] >> 10) & 0x1FF;

#ifdef TEXTURE_CACHE_4BPP
        psx_gpu->viewport_mask =
         texture_region_mask(psx_gpu->viewport_start_x,
         psx_gpu->viewport_start_y, psx_gpu->viewport_end_x,
         psx_gpu->viewport_end_y);
#endif
  			break;
  
  		case 0xE5:
      {
        s32 offset_x = list[0] << 21;
        s32 offset_y = list[0] << 10;
        psx_gpu->offset_x = offset_x >> 21;
        psx_gpu->offset_y = offset_y >> 21; 
  
  			break;
  		}

  		case 0xE6:
      {
        u32 mask_settings = list[0];
        u16 mask_msb = mask_settings << 15;

        if(list[0] & 0x2)
          psx_gpu->render_state_base |= RENDER_STATE_MASK_EVALUATE;
        else
          psx_gpu->render_state_base &= ~RENDER_STATE_MASK_EVALUATE;

        if(mask_msb != psx_gpu->mask_msb)
        {
          flush_render_block_buffer(psx_gpu);
          psx_gpu->mask_msb = mask_msb;
        }

  			break;
      }
  
  		default:
  			break;
  	}
  }
}

