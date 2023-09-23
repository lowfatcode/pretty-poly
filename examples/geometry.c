#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define PP_DEBUG

#include "pretty-poly.h"

const int WIDTH = 1024;
const int HEIGHT = 1024;
uint32_t buffer[WIDTH][HEIGHT];


void callback(const pp_tile_t tile) {
  for(int32_t y = 0; y < tile.bounds.h; y++) {
    for(int32_t x = 0; x < tile.bounds.w; x++) {     
      uint8_t alpha = pp_tile_get_value(&tile, x, y); 
      uint8_t r = 234;
      uint8_t g = 78;
      uint8_t b = 63;
      buffer[y + tile.bounds.y][x + tile.bounds.x] = alpha << 24 | b << 16 | g << 8 | r;
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
    {-256, -256}, {256, -256}, {256, 256}, {-256, 256}
  };

  pp_contour_t square_contours[] = {
    (pp_contour_t){.points = square_outline, .point_count = sizeof(square_outline) / sizeof(pp_point_t)}
  };

  pp_polygon_t square = (pp_polygon_t){.contours = square_contours, .contour_count = sizeof(square_contours) / sizeof(pp_contour_t)};

  pp_mat3_t transform = pp_mat3_identity();
  transform = pp_mat3_mul(transform, pp_mat3_translation(512, 512));
  transform = pp_mat3_mul(transform, pp_mat3_rotation(45));
  pp_transform(&transform);
  draw_polygon(square);

  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}