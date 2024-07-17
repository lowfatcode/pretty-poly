#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define PP_IMPLEMENTATION
// #define PP_DEBUG
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


void _pp_round_rect_corner_points(pp_path_t *path, PP_COORD_TYPE cx, PP_COORD_TYPE cy, PP_COORD_TYPE r, int q) {
  float quality = 5; // higher the number, lower the quality - selected by experiment
  int steps = ceil(r / quality) + 2; // + 2 to include start and end
  float delta = -(M_PI / 2) / steps;
  float theta = (M_PI / 2) * q; // select start theta for this quadrant
  for(int i = 0; i <= steps; i++) {
    PP_COORD_TYPE xo = sin(theta) * r;
    PP_COORD_TYPE yo = cos(theta) * r;
    pp_path_add_point(path, (pp_point_t){cx + xo, cy + yo});
    theta += delta;
  }
}

pp_poly_t* filled_rounded_rectangle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE tlr, PP_COORD_TYPE trr, PP_COORD_TYPE brr, PP_COORD_TYPE blr) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);

  if(tlr == 0) {
    pp_path_add_point(path, (pp_point_t){x, y});
  }else{
    _pp_round_rect_corner_points(path, x + tlr, y + tlr, tlr, 3);
  }
  
  if(trr == 0) {
    pp_path_add_point(path, (pp_point_t){x + w, y});
  }else{
    _pp_round_rect_corner_points(path, x + w - trr, y + trr, trr, 2);
  }

  if(brr == 0) {
    pp_path_add_point(path, (pp_point_t){x + w, y + h});
  }else{
    _pp_round_rect_corner_points(path, x + w - brr, y + h - brr, brr, 1);
  }

  if(blr == 0) {
    pp_path_add_point(path, (pp_point_t){x, y + h});
  }else{
    _pp_round_rect_corner_points(path, x + blr, y + h - blr, blr, 0);
  }

  return poly;
}

pp_poly_t* rounded_rectangle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE tlr, PP_COORD_TYPE trr, PP_COORD_TYPE brr, PP_COORD_TYPE blr, PP_COORD_TYPE t) {
  pp_poly_t *outer = filled_rounded_rectangle(x, y, w, h, tlr, trr, brr, blr);

  tlr = _pp_max(0, tlr - t);
  trr = _pp_max(0, trr - t);
  brr = _pp_max(0, brr - t);
  blr = _pp_max(0, blr - t);

  pp_poly_t *inner = filled_rounded_rectangle(x + t, y + t, w - 2 * t, h - 2 * t, tlr, trr, brr, blr);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* filled_rectangle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  pp_path_add_point(path, (pp_point_t){x, y});
  pp_path_add_point(path, (pp_point_t){x + w, y});
  pp_path_add_point(path, (pp_point_t){x + w, y + h});
  pp_path_add_point(path, (pp_point_t){x, y + h});
  return poly;
}

pp_poly_t* rectangle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE t) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *outer_path = pp_poly_add_path(poly);
  pp_path_add_point(outer_path, (pp_point_t){x, y});
  pp_path_add_point(outer_path, (pp_point_t){x + w, y});
  pp_path_add_point(outer_path, (pp_point_t){x + w, y + h});
  pp_path_add_point(outer_path, (pp_point_t){x, y + h});

  pp_path_t *inner_path = pp_poly_add_path(poly);
  x += t; y += t; w -= 2 * t; h -= 2 * t;
  pp_path_add_point(inner_path, (pp_point_t){x, y});
  pp_path_add_point(inner_path, (pp_point_t){x + w, y});
  pp_path_add_point(inner_path, (pp_point_t){x + w, y + h});
  pp_path_add_point(inner_path, (pp_point_t){x, y + h});
  return poly;
}

pp_poly_t* filled_regular_polygon(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, int s) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  for(int i = 0; i < s; i++) {
    pp_point_t p;
    float step = ((M_PI * 2.0f) / (float)s) * (float)i;
    p.x = sin(step) * r + x;
    p.y = cos(step) * r + y;
    pp_path_add_point(path, p);
  }  
  return poly;
}

pp_poly_t* regular_polygon(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, int s, PP_COORD_TYPE t) {
  pp_poly_t *outer = filled_regular_polygon(x, y, r, s);
  pp_poly_t *inner = filled_regular_polygon(x, y, r - t, s);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* filled_circle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r) {
  int s = _pp_max(8, r);
  return filled_regular_polygon(x, y, r, s);
}

pp_poly_t* circle(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, PP_COORD_TYPE t) {
  pp_poly_t *outer = filled_circle(x, y, r);
  pp_poly_t *inner = filled_circle(x, y, r - t);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* filled_star(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  for(int i = 0; i < c * 2; i++) {
    pp_point_t p;
    float step = ((M_PI * 2) / (float)(c * 2)) * (float)i;
    PP_COORD_TYPE r = i % 2 == 0 ? or: ir; 
    p.x = sin(step) * r + x;
    p.y = cos(step) * r + y;
    pp_path_add_point(path, p);
  }  
  return poly;
}

pp_poly_t* star(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir, PP_COORD_TYPE t) {
  pp_poly_t *outer = filled_star(x, y, c, or, ir);
  pp_poly_t *inner = filled_star(x, y, c, or - t, ir - t);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}


int main() { 
  pp_tile_callback(blend_tile);
  pp_antialias(PP_AA_X4);
  pp_clip(0, 0, WIDTH, HEIGHT);

  for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      buffer[y][x] = ((x / 8) + (y / 8)) % 2 == 0 ? create_colour(16, 32, 48, 255) : create_colour(0, 0, 0, 255);
    }
  }

// pp_point_t outline[] = {{-128, -128}, {128, -128}, {128, 128}, {-128, 128}};
//   pp_point_t hole[]    = {{ -64,   64}, { 64,   64}, { 64, -64}, { -64, -64}};
  

  //pp_poly_t *poly = rectangle(-128, -128, 256, 256, 10.5);

  //pp_poly_t *poly = circle(0, 0, 256, 25);
  for(int y = 0; y < 4; y++) {
    for(int x = 0; x < 4; x++) {
      int i = x + y * 4;
      pp_poly_t *poly = NULL;
      switch(i) {
        case 0: {
          poly = filled_rectangle(-100, -100, 200, 200);        
        }break;
        case 1: {
          poly = rectangle(-100, -100, 200, 200, 20);
        }break;
        case 2: {
          poly = filled_circle(0, 0, 100);        
        }break;
        case 3: {
          poly = circle(0, 0, 100, 20);        
        }break;
        case 4: {          
          poly = filled_regular_polygon(0, 0, 100, 5);        
        }break;
        case 5: {          
          poly = regular_polygon(0, 0, 100, 5, 80);        
        }break;
        case 6: {
          poly = filled_star(0, 0, 15, 100, 75);        
        }break;
        case 7: {
          poly = star(0, 0, 15, 100, 75, 5);        
        }break;
        case 8: {
          poly = filled_rounded_rectangle(-100, -100, 200, 200, 50, 5, 15, 70);
        }break;
        case 9: {
          poly = rounded_rectangle(-100, -100, 200, 200, 50, 5, 15, 70, 20);
        }break;
      }

      if(poly) {
        pp_mat3_t t = pp_mat3_identity();
        pp_mat3_translate(&t, 128 + (x * 256), 128 + (y * 256));
        pp_transform(&t);

        colour pen = create_colour(255, 255, 255, 255);
        set_pen(pen);
        pp_render(poly);

        pp_poly_free(poly);
      }
    }
  }
  
  // for(int i = 0; i < 1000; i += 30) {
  //   pp_mat3_t t = pp_mat3_identity();

  //   float tx = sin((float)i / 100.0f) * (float)i / 3.0f;
  //   float ty = cos((float)i / 100.0f) * (float)i / 3.0f;
  //   float scale = (float)i / 1000.0f;
  //   pp_mat3_translate(&t, 512 + tx, 512 + ty);
  //   pp_mat3_rotate(&t, i);
  //   pp_mat3_scale(&t, scale, scale);
  //   pp_transform(&t);

  //   float hue = (float)i / 1000.0f;
  //   set_pen(create_colour_hsv(hue, 1.0f, 1.0f, 0.5f));

  //   pp_render(&poly);
  // }


  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}