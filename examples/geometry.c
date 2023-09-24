#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "pretty-poly.h"
#include "helpers.h"

const int WIDTH = 1024;
const int HEIGHT = 1024;
colour buffer[1024][1024];

colour pen;
void set_pen(colour c) {
  pen = c;
}

void blend_tile(const pp_tile_t *t) {
  for(int32_t y = t->y; y < t->y + t->h; y++) {
    for(int32_t x = t->x; x < t->x + t->w; x++) {     
      colour alpha_pen = pen;
      alpha_pen.rgba.a = alpha(pen.rgba.a, pp_tile_get(t, x, y));
      buffer[y][x] = blend(buffer[y][x], alpha_pen);
    }
  }
}

int main() {
  //(callback, PP_AA_X4, {0, 0, WIDTH, HEIGHT});
  
  pp_tile_callback(blend_tile);
  pp_antialias(PP_AA_X4);
  pp_clip(0, 0, WIDTH, HEIGHT);

  for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      buffer[x][y] = ((x / 8) + (y / 8)) % 2 == 0 ? create_colour(16, 32, 48, 255) : create_colour(0, 0, 0, 255);
    }
  }

  pp_point_t points[] = {{-128, -128}, {128, -128}, {128, 128}, {-128, 128}};

  pp_contour_t contour = {
    .points = &points, 
    .point_count = 4
  };

  pp_polygon_t polygon = {
    .contours = &contour,
    .contour_count = 1
  };

  pp_render(&polygon);
  
  for(int i = 0; i < 1000; i += 10) {
    pp_mat3_t t = pp_mat3_identity();

    float tx = sin((float)i / 100.0f) * (float)i / 3.0f;
    float ty = cos((float)i / 100.0f) * (float)i / 3.0f;
    pp_mat3_translate(&t, 512 + tx, 512 + ty);
    pp_mat3_rotate(&t, i);
    float scale = (float)i / 1000.0f;
    pp_mat3_scale(&t, scale, scale);
    pp_transform(&t);

    float hue = (float)i / 1000.0f;
    set_pen(create_colour_hsv(hue, 1.0f, 1.0f, 0.1f));


    pp_render(&polygon);
  }

  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}