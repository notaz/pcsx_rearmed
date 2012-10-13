#define setup_sprite_tiled_initialize_4bpp_4x()                                \
  u16 *clut_ptr = psx_gpu->clut_ptr;                                           \
  vec_8x16u clut_a, clut_b;                                                    \
  vec_16x8u clut_low, clut_high;                                               \
                                                                               \
  load_8x16b(clut_a, clut_ptr);                                                \
  load_8x16b(clut_b, clut_ptr + 8);                                            \
  unzip_16x8b(clut_low, clut_high, clut_a, clut_b)                             \


#define setup_sprite_tiled_initialize_8bpp_4x()                                \


#define setup_sprite_tile_fetch_texel_block_8bpp_4x(offset)                    \
  texture_block_ptr = psx_gpu->texture_page_ptr +                              \
   ((texture_offset + offset) & texture_mask);                                 \
                                                                               \
  load_64b(texels, texture_block_ptr)                                          \


#define setup_sprite_tile_setup_block_yes_4x(side, offset, texture_mode)       \

#define setup_sprite_tile_setup_block_no_4x(side, offset, texture_mode)        \

#define setup_sprite_tile_add_blocks_4x(tile_num_blocks)                       \
  num_blocks += tile_num_blocks * 4;                                           \
  sprite_blocks += tile_num_blocks * 4;                                        \
                                                                               \
  if(num_blocks > MAX_BLOCKS)                                                  \
  {                                                                            \
    flush_render_block_buffer(psx_gpu);                                        \
    num_blocks = tile_num_blocks * 4;                                          \
    block = psx_gpu->blocks;                                                   \
  }                                                                            \

#define setup_sprite_tile_full_4bpp_4x(edge)                                   \
{                                                                              \
  vec_8x8u texels_low, texels_high;                                            \
  vec_8x16u pixels, pixels_wide;                                               \
  setup_sprite_tile_add_blocks_4x(sub_tile_height * 2);                        \
  u32 left_mask_bits_a = left_mask_bits & 0xFF;                                \
  u32 left_mask_bits_b = left_mask_bits >> 8;                                  \
  u32 right_mask_bits_a = right_mask_bits & 0xFF;                              \
  u32 right_mask_bits_b = right_mask_bits >> 8;                                \
                                                                               \
  while(sub_tile_height)                                                       \
  {                                                                            \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(0);                            \
    tbl_16(texels_low, texels, clut_low);                                      \
    tbl_16(texels_high, texels, clut_high);                                    \
    zip_8x16b(pixels, texels_low, texels_high);                                \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.low, pixels.low);		 \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = left_mask_bits_a;                                  \
    block->fb_ptr = fb_ptr;                                                    \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = left_mask_bits_a;                                  \
    block->fb_ptr = fb_ptr + 1024;                                             \
    block++;                                                                   \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.high, pixels.high);  \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = left_mask_bits_b;                                  \
    block->fb_ptr = fb_ptr + 8;                                                \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = left_mask_bits_b;                                  \
    block->fb_ptr = fb_ptr + 1024 + 8;                                         \
    block++;                                                                   \
                                                                               \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(8);                            \
    tbl_16(texels_low, texels, clut_low);                                      \
    tbl_16(texels_high, texels, clut_high);                                    \
    zip_8x16b(pixels, texels_low, texels_high);                                \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.low, pixels.low);	   \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = right_mask_bits_a;                                 \
    block->fb_ptr = fb_ptr + 16;                                               \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = right_mask_bits_a;                                 \
    block->fb_ptr = fb_ptr + 1024 + 16;                                        \
    block++;                                                                   \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.high, pixels.high);  \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = right_mask_bits_b;                                 \
    block->fb_ptr = fb_ptr + 24;                                               \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = right_mask_bits_b;                                 \
    block->fb_ptr = fb_ptr + 1024 + 24;                                        \
    block++;                                                                   \
                                                                               \
    fb_ptr += 2048;                                                            \
    texture_offset += 0x10;                                                    \
    sub_tile_height--;                                                         \
  }                                                                            \
  texture_offset += 0xF00;                                                     \
  psx_gpu->num_blocks = num_blocks;                                            \
}                                                                              \

#define setup_sprite_tile_half_4bpp_4x(edge)                                   \
{                                                                              \
  vec_8x8u texels_low, texels_high;                                            \
  vec_8x16u pixels, pixels_wide;                                               \
  setup_sprite_tile_add_blocks(sub_tile_height);                               \
  u32 edge##_mask_bits_a = edge##_mask_bits & 0xFF;                            \
  u32 edge##_mask_bits_b = edge##_mask_bits >> 8;                              \
                                                                               \
  while(sub_tile_height)                                                       \
  {                                                                            \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(0);                            \
    tbl_16(texels_low, texels, clut_low);                                      \
    tbl_16(texels_high, texels, clut_high);                                    \
    zip_8x16b(pixels, texels_low, texels_high);                                \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.low, pixels.low);		 \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = edge##_mask_bits_a;                                \
    block->fb_ptr = fb_ptr;                                                    \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = edge##_mask_bits_a;                                \
    block->fb_ptr = fb_ptr + 1024;                                             \
    block++;                                                                   \
                                                                               \
    zip_4x32b(vector_cast(vec_4x32u, pixels_wide), pixels.high, pixels.high);  \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = edge##_mask_bits_b;                                \
    block->fb_ptr = fb_ptr + 8;                                                \
    block++;                                                                   \
                                                                               \
    block->texels = pixels_wide;                                               \
    block->draw_mask_bits = edge##_mask_bits_b;                                \
    block->fb_ptr = fb_ptr + 1024 + 8;                                         \
    block++;                                                                   \
                                                                               \
    fb_ptr += 2048;                                                            \
    texture_offset += 0x10;                                                    \
    sub_tile_height--;                                                         \
  }                                                                            \
  texture_offset += 0xF00;                                                     \
  psx_gpu->num_blocks = num_blocks;                                            \
}                                                                              \

  
#define setup_sprite_tile_full_8bpp_4x(edge)                                   \
{                                                                              \
  setup_sprite_tile_add_blocks_4x(sub_tile_height * 2);                        \
  vec_16x8u texels_wide;                                                       \
  u32 left_mask_bits_a = left_mask_bits & 0xFF;                                \
  u32 left_mask_bits_b = left_mask_bits >> 8;                                  \
  u32 right_mask_bits_a = right_mask_bits & 0xFF;                              \
  u32 right_mask_bits_b = right_mask_bits >> 8;                                \
                                                                               \
  while(sub_tile_height)                                                       \
  {                                                                            \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(0);                            \
    zip_8x16b(vector_cast(vec_8x16u, texels_wide), texels, texels);            \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = left_mask_bits_a;                                  \
    block->fb_ptr = fb_ptr;                                                    \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = left_mask_bits_a;                                  \
    block->fb_ptr = fb_ptr + 1024;                                             \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = left_mask_bits_b;                                  \
    block->fb_ptr = fb_ptr + 8;                                                \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = left_mask_bits_b;                                  \
    block->fb_ptr = fb_ptr + 1024 + 8;                                         \
    block++;                                                                   \
                                                                               \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(8);                            \
    zip_8x16b(vector_cast(vec_8x16u, texels_wide), texels, texels);            \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = right_mask_bits_a;                                 \
    block->fb_ptr = fb_ptr + 16;                                               \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = right_mask_bits_a;                                 \
    block->fb_ptr = fb_ptr + 1024;                                             \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = right_mask_bits_b;                                 \
    block->fb_ptr = fb_ptr + 24 + 1024;                                        \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = right_mask_bits_b;                                 \
    block->fb_ptr = fb_ptr + 24 + 1024;                                        \
    block++;                                                                   \
                                                                               \
    fb_ptr += 2048;                                                            \
    texture_offset += 0x10;                                                    \
    sub_tile_height--;                                                         \
  }                                                                            \
  texture_offset += 0xF00;                                                     \
  psx_gpu->num_blocks = num_blocks;                                            \
}                                                                              \

#define setup_sprite_tile_half_8bpp_4x(edge)                                   \
{                                                                              \
  setup_sprite_tile_add_blocks_4x(sub_tile_height * 2);                        \
  vec_16x8u texels_wide;                                                       \
  u32 edge##_mask_bits_a = edge##_mask_bits & 0xFF;                            \
  u32 edge##_mask_bits_b = edge##_mask_bits >> 8;                              \
                                                                               \
  while(sub_tile_height)                                                       \
  {                                                                            \
    setup_sprite_tile_fetch_texel_block_8bpp_4x(0);                            \
    zip_8x16b(vector_cast(vec_8x16u, texels_wide), texels, texels);            \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = edge##_mask_bits_a;                                \
    block->fb_ptr = fb_ptr;                                                    \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.low;                                                \
    block->draw_mask_bits = edge##_mask_bits_a;                                \
    block->fb_ptr = fb_ptr + 1024;                                             \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = edge##_mask_bits_b;                                \
    block->fb_ptr = fb_ptr + 8;                                                \
    block++;                                                                   \
                                                                               \
    block->r = texels_wide.high;                                               \
    block->draw_mask_bits = edge##_mask_bits_b;                                \
    block->fb_ptr = fb_ptr + 8 + 1024;                                         \
    block++;                                                                   \
                                                                               \
    fb_ptr += 2048;                                                            \
    texture_offset += 0x10;                                                    \
    sub_tile_height--;                                                         \
  }                                                                            \
  texture_offset += 0xF00;                                                     \
  psx_gpu->num_blocks = num_blocks;                                            \
}                                                                              \

  
#define setup_sprite_tile_column_edge_pre_adjust_half_right_4x()               \
  texture_offset = texture_offset_base + 8;                                    \
  fb_ptr += 16                                                                 \

#define setup_sprite_tile_column_edge_pre_adjust_half_left_4x()                \
  texture_offset = texture_offset_base                                         \

#define setup_sprite_tile_column_edge_pre_adjust_half_4x(edge)                 \
  setup_sprite_tile_column_edge_pre_adjust_half_##edge##_4x()                  \

#define setup_sprite_tile_column_edge_pre_adjust_full_4x(edge)                 \
  texture_offset = texture_offset_base                                         \

#define setup_sprite_tile_column_edge_post_adjust_half_right_4x()              \
  fb_ptr -= 16                                                                 \

#define setup_sprite_tile_column_edge_post_adjust_half_left_4x()               \

#define setup_sprite_tile_column_edge_post_adjust_half_4x(edge)                \
  setup_sprite_tile_column_edge_post_adjust_half_##edge##_4x()                 \

#define setup_sprite_tile_column_edge_post_adjust_full_4x(edge)                \


#define setup_sprite_tile_column_height_single_4x(edge_mode, edge,             \
 texture_mode)                                                                 \
do                                                                             \
{                                                                              \
  sub_tile_height = column_data;                                               \
  setup_sprite_tile_column_edge_pre_adjust_##edge_mode##_4x(edge);             \
  setup_sprite_tile_##edge_mode##_##texture_mode##_4x(edge);                   \
  setup_sprite_tile_column_edge_post_adjust_##edge_mode##_4x(edge);            \
} while(0)                                                                     \

#define setup_sprite_tile_column_height_multi_4x(edge_mode, edge,              \
 texture_mode)                                                                 \
do                                                                             \
{                                                                              \
  u32 tiles_remaining = column_data >> 16;                                     \
  sub_tile_height = column_data & 0xFF;                                        \
  setup_sprite_tile_column_edge_pre_adjust_##edge_mode##_4x(edge);             \
  setup_sprite_tile_##edge_mode##_##texture_mode##_4x(edge);                   \
  tiles_remaining -= 1;                                                        \
                                                                               \
  while(tiles_remaining)                                                       \
  {                                                                            \
    sub_tile_height = 16;                                                      \
    setup_sprite_tile_##edge_mode##_##texture_mode##_4x(edge);                 \
    tiles_remaining--;                                                         \
  }                                                                            \
                                                                               \
  sub_tile_height = (column_data >> 8) & 0xFF;                                 \
  setup_sprite_tile_##edge_mode##_##texture_mode##_4x(edge);                   \
  setup_sprite_tile_column_edge_post_adjust_##edge_mode##_4x(edge);            \
} while(0)                                                                     \


#define setup_sprite_column_data_single_4x()                                   \
  column_data = height                                                         \

#define setup_sprite_column_data_multi_4x()                                    \
  column_data = 16 - offset_v;                                                 \
  column_data |= ((height_rounded & 0xF) + 1) << 8;                            \
  column_data |= (tile_height - 1) << 16                                       \


#define setup_sprite_tile_column_width_single_4x(texture_mode, multi_height,   \
 edge_mode, edge)                                                              \
{                                                                              \
  setup_sprite_column_data_##multi_height##_4x();                              \
  left_mask_bits = left_block_mask | right_block_mask;                         \
  right_mask_bits = left_mask_bits >> 16;                                      \
                                                                               \
  setup_sprite_tile_column_height_##multi_height##_4x(edge_mode, edge,         \
   texture_mode);                                                              \
}                                                                              \

#define setup_sprite_tiled_advance_column_4x()                                 \
  texture_offset_base += 0x100;                                                \
  if((texture_offset_base & 0xF00) == 0)                                       \
    texture_offset_base -= (0x100 + 0xF00)                                     \

#define setup_sprite_tile_column_width_multi_4x(texture_mode, multi_height,    \
 left_mode, right_mode)                                                        \
{                                                                              \
  setup_sprite_column_data_##multi_height##_4x();                              \
  s32 fb_ptr_advance_column = 32 - (2048 * height);                            \
                                                                               \
  tile_width -= 2;                                                             \
  left_mask_bits = left_block_mask;                                            \
  right_mask_bits = left_mask_bits >> 16;                                      \
                                                                               \
  setup_sprite_tile_column_height_##multi_height##_4x(left_mode, right,        \
   texture_mode);                                                              \
  fb_ptr += fb_ptr_advance_column;                                             \
                                                                               \
  left_mask_bits = 0x00;                                                       \
  right_mask_bits = 0x00;                                                      \
                                                                               \
  while(tile_width)                                                            \
  {                                                                            \
    setup_sprite_tiled_advance_column_4x();                                    \
    setup_sprite_tile_column_height_##multi_height##_4x(full, none,            \
     texture_mode);                                                            \
    fb_ptr += fb_ptr_advance_column;                                           \
    tile_width--;                                                              \
  }                                                                            \
                                                                               \
  left_mask_bits = right_block_mask;                                           \
  right_mask_bits = left_mask_bits >> 16;                                      \
                                                                               \
  setup_sprite_tiled_advance_column();                                         \
  setup_sprite_tile_column_height_##multi_height##_4x(right_mode, left,        \
   texture_mode);                                                              \
}                                                                              \


#define setup_sprite_tiled_builder_4x(texture_mode)                            \
void setup_sprite_##texture_mode##_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y,   \
 s32 u, s32 v, s32 width, s32 height, u32 color)                               \
{                                                                              \
  s32 offset_u = u & 0xF;                                                      \
  s32 offset_v = v & 0xF;                                                      \
                                                                               \
  s32 width_rounded = offset_u + width + 15;                                   \
  s32 height_rounded = offset_v + height + 15;                                 \
  s32 tile_height = height_rounded / 16;                                       \
  s32 tile_width = width_rounded / 16;                                         \
  u32 offset_u_right = width_rounded & 0xF;                                    \
                                                                               \
  u32 left_block_mask = ~(0xFFFFFFFF << (offset_u * 2));                       \
  u32 right_block_mask = 0xFFFFFFFE << (offset_u_right * 2);                   \
                                                                               \
  u32 left_mask_bits;                                                          \
  u32 right_mask_bits;                                                         \
                                                                               \
  u32 sub_tile_height;                                                         \
  u32 column_data;                                                             \
                                                                               \
  u32 texture_mask = (psx_gpu->texture_mask_width & 0xF) |                     \
   ((psx_gpu->texture_mask_height & 0xF) << 4) |                               \
   ((psx_gpu->texture_mask_width >> 4) << 8) |                                 \
   ((psx_gpu->texture_mask_height >> 4) << 12);                                \
  u32 texture_offset = ((v & 0xF) << 4) | ((u & 0xF0) << 4) |                  \
   ((v & 0xF0) << 8);                                                          \
  u32 texture_offset_base = texture_offset;                                    \
  u32 control_mask;                                                            \
                                                                               \
  u16 *fb_ptr = psx_gpu->vram_ptr + (y * 1024) + (x - offset_u);               \
  u32 num_blocks = psx_gpu->num_blocks;                                        \
  block_struct *block = psx_gpu->blocks + num_blocks;                          \
                                                                               \
  u16 *texture_block_ptr;                                                      \
  vec_8x8u texels;                                                             \
                                                                               \
  setup_sprite_tiled_initialize_##texture_mode##_4x();                         \
                                                                               \
  control_mask = tile_width == 1;                                              \
  control_mask |= (tile_height == 1) << 1;                                     \
  control_mask |= ((left_block_mask & 0xFFFF) == 0xFFFF) << 2;                 \
  control_mask |= (((right_block_mask >> 8) & 0xFFFF) == 0xFFFF) << 3;         \
                                                                               \
  sprites_##texture_mode++;                                                    \
                                                                               \
  switch(control_mask)                                                         \
  {                                                                            \
    default:                                                                   \
    case 0x0:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, multi, full,       \
       full);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x1:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, multi, full,      \
       none);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x2:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, single, full,      \
       full);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x3:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, single, full,     \
       none);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x4:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, multi, half,       \
       full);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x5:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, multi, half,      \
       right);                                                                 \
      break;                                                                   \
                                                                               \
    case 0x6:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, single, half,      \
       full);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x7:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, single, half,     \
       right);                                                                 \
      break;                                                                   \
                                                                               \
    case 0x8:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, multi, full,       \
       half);                                                                  \
      break;                                                                   \
                                                                               \
    case 0x9:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, multi, half,      \
       left);                                                                  \
      break;                                                                   \
                                                                               \
    case 0xA:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, single, full,      \
       half);                                                                  \
      break;                                                                   \
                                                                               \
    case 0xB:                                                                  \
      setup_sprite_tile_column_width_single_4x(texture_mode, single, half,     \
       left);                                                                  \
      break;                                                                   \
                                                                               \
    case 0xC:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, multi, half,       \
       half);                                                                  \
      break;                                                                   \
                                                                               \
    case 0xE:                                                                  \
      setup_sprite_tile_column_width_multi_4x(texture_mode, single, half,      \
       half);                                                                  \
      break;                                                                   \
  }                                                                            \
}                                                                              \


void setup_sprite_4bpp_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y, s32 u, s32 v,
 s32 width, s32 height, u32 color);
void setup_sprite_8bpp_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y, s32 u, s32 v,
 s32 width, s32 height, u32 color);
void setup_sprite_16bpp_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y, s32 u, s32 v,
 s32 width, s32 height, u32 color);

//#ifndef NEON_BUILD
#if 1
setup_sprite_tiled_builder_4x(4bpp);
setup_sprite_tiled_builder_4x(8bpp);

void setup_sprite_16bpp_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y, s32 u,
 s32 v, s32 width, s32 height, u32 color)
{
  u32 left_offset = u & 0x7;
  u32 width_rounded = width + left_offset + 7;

  u16 *fb_ptr = psx_gpu->vram_ptr + (y * 1024) + (s32)(x - left_offset);
  u32 right_width = width_rounded & 0x7;
  u32 block_width = width_rounded / 8;
  u32 fb_ptr_pitch = (1024 + 8) - (block_width * 8);

  u32 left_mask_bits = ~(0xFFFF << (left_offset * 2));
  u32 right_mask_bits = 0xFE << (right_width * 2);

  u32 texture_offset_base = u + (v * 1024);
  u32 texture_mask =
   psx_gpu->texture_mask_width | (psx_gpu->texture_mask_height * 1024);

  u32 blocks_remaining;
  u32 num_blocks = psx_gpu->num_blocks;
  block_struct *block = psx_gpu->blocks + num_blocks;

  u16 *texture_page_ptr = psx_gpu->texture_page_ptr;
  u16 *texture_block_ptr;

  texture_offset_base &= ~0x7;

  sprites_16bpp++;

  if(block_width == 1)
  {
    u32 mask_bits = left_mask_bits | right_mask_bits;
    u32 mask_bits_a = mask_bits & 0xFF;
    u32 mask_bits_b = mask_bits >> 8;
    
    vec_8x16u texels;
    vec_8x16u texels_wide;

    while(height)
    {
      num_blocks += 4;
      sprite_blocks += 4;

      if(num_blocks > MAX_BLOCKS)
      {
        flush_render_block_buffer(psx_gpu);
        num_blocks = 4;
        block = psx_gpu->blocks;
      }
      
      texture_block_ptr =
       texture_page_ptr + (texture_offset_base & texture_mask);

      load_128b(texels, texture_block_ptr);
      
      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.low, texels.low);
      block->texels = texels_wide;
      block->draw_mask_bits = mask_bits_a;
      block->fb_ptr = fb_ptr;          
      block++;
      
      block->texels = texels_wide;
      block->draw_mask_bits = mask_bits_a;
      block->fb_ptr = fb_ptr + 1024;          
      block++;
      
      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.high, texels.high);
      block->texels = texels_wide;
      block->draw_mask_bits = mask_bits_b;
      block->fb_ptr = fb_ptr + 8;
      block++;
      
      block->texels = texels_wide;
      block->draw_mask_bits = mask_bits_b;
      block->fb_ptr = fb_ptr + 8 + 1024;          
      block++;      

      texture_offset_base += 1024;
      fb_ptr += 2048;

      height--;
      psx_gpu->num_blocks = num_blocks;
    }
  }
  else
  {
    u32 texture_offset;
    
    u32 left_mask_bits_a = left_mask_bits & 0xFF;
    u32 left_mask_bits_b = left_mask_bits >> 8;
    u32 right_mask_bits_a = right_mask_bits & 0xFF;
    u32 right_mask_bits_b = right_mask_bits >> 8;
    
    vec_8x16u texels;
    vec_8x16u texels_wide;    

    while(height)
    {
      blocks_remaining = block_width - 2;
      num_blocks += block_width * 4;
      sprite_blocks += block_width * 4;

      if(num_blocks > MAX_BLOCKS)
      {
        flush_render_block_buffer(psx_gpu);
        num_blocks = block_width;
        block = psx_gpu->blocks;
      }

      texture_offset = texture_offset_base;
      texture_offset_base += 1024;

      texture_block_ptr = texture_page_ptr + (texture_offset & texture_mask);
      
      load_128b(texels, texture_block_ptr);

      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.low, texels.low);
      block->texels = texels_wide;
      block->draw_mask_bits = left_mask_bits_a;
      block->fb_ptr = fb_ptr;
      block++;
      
      block->texels = texels_wide;
      block->draw_mask_bits = left_mask_bits_a;
      block->fb_ptr = fb_ptr + 1024;
      block++;      

      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.high, texels.high);
      block->texels = texels_wide;
      block->draw_mask_bits = left_mask_bits_b;
      block->fb_ptr = fb_ptr + 8;
      block++;  
      
      block->texels = texels_wide;
      block->draw_mask_bits = left_mask_bits_b;
      block->fb_ptr = fb_ptr + 8 + 1024;
      block++;  
      
      texture_offset += 8;
      fb_ptr += 16;

      while(blocks_remaining)
      {
        texture_block_ptr = texture_page_ptr + (texture_offset & texture_mask);
        load_128b(texels, texture_block_ptr);

        zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.low, texels.low);
        block->texels = texels_wide;
        block->draw_mask_bits = 0;
        block->fb_ptr = fb_ptr;
        block++;
        
        block->texels = texels_wide;
        block->draw_mask_bits = 0;
        block->fb_ptr = fb_ptr + 1024;
        block++;      

        zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.high, texels.high);
        block->texels = texels_wide;
        block->draw_mask_bits = 0;
        block->fb_ptr = fb_ptr + 8;
        block++;
        
        block->texels = texels_wide;
        block->draw_mask_bits = 0;
        block->fb_ptr = fb_ptr + 8 + 1024;
        block++;
        
        texture_offset += 8;
        fb_ptr += 8;

        blocks_remaining--;
      }

      texture_block_ptr = texture_page_ptr + (texture_offset & texture_mask);
      load_128b(texels, texture_block_ptr);
      
      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.low, texels.low);
      block->texels = texels_wide;
      block->draw_mask_bits = right_mask_bits_a;
      block->fb_ptr = fb_ptr;
      block++;
      
      block->texels = texels_wide;
      block->draw_mask_bits = right_mask_bits_a;
      block->fb_ptr = fb_ptr + 1024;
      block++;      

      zip4x32b(vector_cast(vec_4x32u, texels_wide), texels.high, texels.high);
      block->texels = texels_wide;
      block->draw_mask_bits = right_mask_bits_b;
      block->fb_ptr = fb_ptr + 8;
      block++;

      block->texels = texels_wide;
      block->draw_mask_bits = right_mask_bits_b;
      block->fb_ptr = fb_ptr + 8 + 1024;      
      block++;

      fb_ptr += fb_ptr_pitch;

      height--;
      psx_gpu->num_blocks = num_blocks;
    }
  }
}

#endif

#define setup_sprite_blocks_switch_textured_4x(texture_mode)                   \
  setup_sprite_##texture_mode##_4x                                             \

#define setup_sprite_blocks_switch_untextured_4x(texture_mode)                 \
  setup_sprite_untextured                                                      \

#define setup_sprite_blocks_switch_4x(texturing, texture_mode)                 \
  setup_sprite_blocks_switch_##texturing##_4x(texture_mode)                    \

  
#define render_sprite_blocks_switch_block_modulation_4x(texture_mode,          \
 blend_mode, mask_evaluate, shading, dithering, texturing, blending,           \
 modulation)                                                                   \
{                                                                              \
  setup_sprite_blocks_switch_4x(texturing, texture_mode),                      \
  texture_sprite_blocks_switch_##texturing(texture_mode),                      \
  shade_blocks_switch(unshaded, texturing, modulation, undithered, blending,   \
   mask_evaluate),                                                             \
  blend_blocks_switch(texturing, blending, blend_mode, mask_evaluate)          \
}                                                                              \

#define render_sprite_blocks_switch_block_blending_4x(texture_mode,            \
 blend_mode, mask_evaluate, shading, dithering, texturing, blending)           \
  render_sprite_blocks_switch_block_modulation_4x(texture_mode, blend_mode,    \
   mask_evaluate, shading, dithering, texturing, blending, modulated),         \
  render_sprite_blocks_switch_block_modulation_4x(texture_mode, blend_mode,    \
   mask_evaluate, shading, dithering, texturing, blending, unmodulated)        \

#define render_sprite_blocks_switch_block_texturing_4x(texture_mode,           \
 blend_mode, mask_evaluate, shading, dithering, texturing)                     \
  render_sprite_blocks_switch_block_blending_4x(texture_mode, blend_mode,      \
   mask_evaluate, shading, dithering, texturing, unblended),                   \
  render_sprite_blocks_switch_block_blending_4x(texture_mode, blend_mode,      \
   mask_evaluate, shading, dithering, texturing, blended)                      \

#define render_sprite_blocks_switch_block_dithering_4x(texture_mode,           \
 blend_mode, mask_evaluate, shading, dithering)                                \
  render_sprite_blocks_switch_block_texturing_4x(texture_mode, blend_mode,     \
   mask_evaluate, shading, dithering, untextured),                             \
  render_sprite_blocks_switch_block_texturing_4x(texture_mode, blend_mode,     \
   mask_evaluate, shading, dithering, textured)                                \

#define render_sprite_blocks_switch_block_shading_4x(texture_mode, blend_mode, \
 mask_evaluate, shading)                                                       \
  render_sprite_blocks_switch_block_dithering_4x(texture_mode, blend_mode,     \
   mask_evaluate, shading, undithered),                                        \
  render_sprite_blocks_switch_block_dithering_4x(texture_mode, blend_mode,     \
   mask_evaluate, shading, dithered)                                           \

#define render_sprite_blocks_switch_block_mask_evaluate_4x(texture_mode,       \
 blend_mode, mask_evaluate)                                                    \
  render_sprite_blocks_switch_block_shading_4x(texture_mode, blend_mode,       \
   mask_evaluate, unshaded),                                                   \
  render_sprite_blocks_switch_block_shading_4x(texture_mode, blend_mode,       \
   mask_evaluate, shaded)                                                      \

#define render_sprite_blocks_switch_block_blend_mode_4x(texture_mode,          \
 blend_mode)                                                                   \
  render_sprite_blocks_switch_block_mask_evaluate_4x(texture_mode, blend_mode, \
   off),                                                                       \
  render_sprite_blocks_switch_block_mask_evaluate_4x(texture_mode, blend_mode, \
   on)                                                                         \

#define render_sprite_blocks_switch_block_texture_mode_4x(texture_mode)        \
  render_sprite_blocks_switch_block_blend_mode_4x(texture_mode, average),      \
  render_sprite_blocks_switch_block_blend_mode_4x(texture_mode, add),          \
  render_sprite_blocks_switch_block_blend_mode_4x(texture_mode, subtract),     \
  render_sprite_blocks_switch_block_blend_mode_4x(texture_mode, add_fourth)    \

#define render_sprite_blocks_switch_block_4x()                                 \
  render_sprite_blocks_switch_block_texture_mode_4x(4bpp),                     \
  render_sprite_blocks_switch_block_texture_mode_4x(8bpp),                     \
  render_sprite_blocks_switch_block_texture_mode_4x(16bpp),                    \
  render_sprite_blocks_switch_block_texture_mode_4x(4bpp)                      \


render_block_handler_struct render_sprite_block_handlers_4x[] =
{
  render_sprite_blocks_switch_block_4x()
};


void render_sprite_4x(psx_gpu_struct *psx_gpu, s32 x, s32 y, u32 u, u32 v,
 s32 width, s32 height, u32 flags, u32 color)
{
  x *= 2;
  y *= 2;

  s32 x_right = x + width - 1;
  s32 y_bottom = y + height - 1;

#ifdef PROFILE
  sprites++;
#endif

  if(x < psx_gpu->viewport_start_x)
  {
    u32 clip = psx_gpu->viewport_start_x - x;
    x += clip;
    u += clip;
    width -= clip;
  }

  if(y < psx_gpu->viewport_start_y)
  {
    s32 clip = psx_gpu->viewport_start_y - y;
    y += clip;
    v += clip;
    height -= clip;
  }

  if(x_right > psx_gpu->viewport_end_x)
    width -= x_right - psx_gpu->viewport_end_x;

  if(y_bottom > psx_gpu->viewport_end_y)
    height -= y_bottom - psx_gpu->viewport_end_y;

  if((width <= 0) || (height <= 0))
    return;

#ifdef PROFILE
  span_pixels += width * height;
  spans += height;
#endif

  u32 render_state = flags &
   (RENDER_FLAGS_MODULATE_TEXELS | RENDER_FLAGS_BLEND |
   RENDER_FLAGS_TEXTURE_MAP);
  render_state |=
   (psx_gpu->render_state_base & ~RENDER_STATE_DITHER);

  if((psx_gpu->render_state != render_state) ||
   (psx_gpu->primitive_type != PRIMITIVE_TYPE_SPRITE))
  {
    psx_gpu->render_state = render_state;
    flush_render_block_buffer(psx_gpu);
#ifdef PROFILE
    state_changes++;
#endif
  }

  psx_gpu->primitive_type = PRIMITIVE_TYPE_SPRITE;

  color &= 0xFFFFFF;

  if(psx_gpu->triangle_color != color)
  {
    flush_render_block_buffer(psx_gpu);
    psx_gpu->triangle_color = color;
  }

  if(color == 0x808080)
    render_state |= RENDER_FLAGS_MODULATE_TEXELS;

  render_block_handler_struct *render_block_handler =
   &(render_sprite_block_handlers_4x[render_state]);
  psx_gpu->render_block_handler = render_block_handler;

  ((setup_sprite_function_type *)render_block_handler->setup_blocks)
   (psx_gpu, x, y, u, v, width, height, color);
}

