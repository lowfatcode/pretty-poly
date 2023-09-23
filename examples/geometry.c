#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "pretty-poly.h"

const int WIDTH = 1024;
const int HEIGHT = 1024;
uint32_t buffer[WIDTH][HEIGHT];


uint32_t colour;
void set_colour(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  colour = a << 24 | b << 16 | g << 8 | r;
}

uint32_t blend(uint32_t d, uint32_t s) {
  uint8_t ba = s >> 24;
  uint8_t iba = 255 - ba;
  uint8_t dr = (d & 0x000000ff) >>  0;
  uint8_t dg = (d & 0x0000ff00) >>  8;
  uint8_t db = (d & 0x00ff0000) >> 16;
  uint8_t da = (d & 0xff000000) >> 24;
  uint8_t sr = (s & 0x000000ff) >>  0;
  uint8_t sg = (s & 0x0000ff00) >>  8;
  uint8_t sb = (s & 0x00ff0000) >> 16;
  uint8_t sa = (s & 0xff000000) >> 24;

  uint8_t rr = ((dr * iba) + (sr * ba)) / 255;
  uint8_t rg = ((dg * iba) + (sg * ba)) / 255;
  uint8_t rb = ((db * iba) + (sb * ba)) / 255;
  uint8_t ra = ((da * iba) + (sa * ba)) / 255;
  
  return (ra << 24) | (rb << 16) | (rg << 8) | rr;
}

void callback(const pp_tile_t tile) {
  uint8_t sa = colour >> 24;
  uint32_t sc = colour & 0x00ffffff;

  for(int32_t y = 0; y < tile.bounds.h; y++) {
    for(int32_t x = 0; x < tile.bounds.w; x++) {     
      uint8_t a = (pp_tile_get_value(&tile, x, y) * sa) >> 8; 
    
      uint32_t bc = sc | (a << 24);
      
      buffer[y + tile.bounds.y][x + tile.bounds.x] = blend(buffer[y + tile.bounds.y][x + tile.bounds.x], bc);
      
      /*colour &= 0x00ffffff;
      colour |= alpha << 24;
      buffer[y + tile.bounds.y][x + tile.bounds.x] = colour;*/
    }
  }
}

int main() {
  //(callback, PP_AA_X4, {0, 0, WIDTH, HEIGHT});
  
  pp_tile_callback(callback);
  pp_antialias(PP_AA_X4);
  pp_clip((pp_rect_t){0, 0, WIDTH, HEIGHT});

  for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      buffer[x][y] = ((x / 8) + (y / 8)) % 2 == 0 ? 0xff302010 : 0xff000000;
    }
  }
  
  pp_point_t square_outline[] = { // outline contour
    {-128, -128}, {128, -128}, {128, 128}, {-128, 128}
  };

  pp_contour_t square_contours[] = {
    (pp_contour_t){.points = square_outline, .point_count = sizeof(square_outline) / sizeof(pp_point_t)}
  };

  pp_polygon_t square = (pp_polygon_t){.contours = square_contours, .contour_count = sizeof(square_contours) / sizeof(pp_contour_t)};

  
  for(int i = 0; i < 1000; i += 10) {
    pp_mat3_t transform = pp_mat3_identity();

    float tx = sin((float)i / 100.0f) * (float)i / 3.0f;
    float ty = cos((float)i / 100.0f) * (float)i / 3.0f;
    transform = pp_mat3_mul(transform, pp_mat3_translation(512 + tx, 512 + ty));
    transform = pp_mat3_mul(transform, pp_mat3_rotation(i));
    float scale = (float)i / 1000.0f;
    transform = pp_mat3_mul(transform, pp_mat3_scale(scale, scale));
    pp_transform(&transform);
    set_colour(i / 4, 255 - (i / 4), 0, 255);
    draw_polygon(square);
  }

  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}