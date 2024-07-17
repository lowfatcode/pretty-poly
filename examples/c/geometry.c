#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define PP_IMPLEMENTATION
#define PP_DEBUG
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
  for(int y = t->y; y < t->y + t->h; y++) {
    for(int x = t->x; x < t->x + t->w; x++) {     
      colour alpha_pen = pen;
      alpha_pen.a = alpha(pen.a, pp_tile_get(t, x, y));
      buffer[y][x] = blend(buffer[y][x], alpha_pen);
    }
  }

  // draw border of tile for debugging
  /*colour box = create_colour(160, 180, 200, 150);
  for(int32_t x = t->x; x < t->x + t->w; x++) {     
    buffer[t->y][x] = blend(buffer[t->y][x], box);
    buffer[t->y + t->h][x] = blend(buffer[t->y + t->h][x], box);
  }
  for(int32_t y = t->y; y < t->y + t->h; y++) {
    buffer[y][t->x] = blend(buffer[y][t->x], box);
    buffer[y][t->x + t->w] = blend(buffer[y][t->x + t->w], box);
  }*/
}



int main() {
  //(callback, PP_AA_X4, {0, 0, WIDTH, HEIGHT});
  
  pp_tile_callback(blend_tile);
  pp_antialias(PP_AA_X4);
  pp_clip(0, 0, WIDTH, HEIGHT);

  for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      buffer[y][x] = ((x / 8) + (y / 8)) % 2 == 0 ? create_colour(16, 32, 48, 255) : create_colour(0, 0, 0, 255);
    }
  }

  // pp_poly_t *poly = pp_make_poly();
  // pp_path_t *outline = pp_poly_new_contours(poly);
  // pp_contour_add_point(outline, {x, y});
  // pp_contour_add_points(outline, {x, y}, {x, y}, {x, y}, {x, y}, {x, y});
  // pp_contour_allocate_points(outline, 10);


  // pp_poly_t *poly = pp_make_poly();
  // pp_path_t *outline = pp_poly_new_contour(poly);
  // pp_contour_add_point(outline, {x, y});


  pp_poly_t *poly = pp_create_poly();
  pp_polygon_allocate_paths(&poly, 2);
  pp_path_t *outline = poly.paths;
  pp_path_t *hole = poly.paths->next;

  outline->points = pp_allocate_path_points(4);
  hole->points = pp_allocate_path_points(4);

  
  // create a 256 x 256 square centered around 0, 0 with a 128 x 128 hole
  pp_point_t outline[] = {{-128, -128}, {128, -128}, {128, 128}, {-128, 128}};
  pp_point_t hole[]    = {{ -64,   64}, { 64,   64}, { 64, -64}, { -64, -64}};
  pp_path_t paths[] = {
    {.points = outline, .count = 4},
    {.points = hole,    .count = 4}
  };  
  pp_poly_t poly = {.paths = paths, .count = 2};

  for(int i = 0; i < 1000; i += 30) {
    pp_mat3_t t = pp_mat3_identity();

    float tx = sin((float)i / 100.0f) * (float)i / 3.0f;
    float ty = cos((float)i / 100.0f) * (float)i / 3.0f;
    float scale = (float)i / 1000.0f;
    pp_mat3_translate(&t, 512 + tx, 512 + ty);
    pp_mat3_rotate(&t, i);
    pp_mat3_scale(&t, scale, scale);
    pp_transform(&t);

    float hue = (float)i / 1000.0f;
    set_pen(create_colour_hsv(hue, 1.0f, 1.0f, 0.5f));

    pp_render(&poly);
  }


  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}