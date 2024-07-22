#include "float.h"

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
}


void _pp_round_rect_corner_points(pp_path_t *path, PP_COORD_TYPE cx, PP_COORD_TYPE cy, PP_COORD_TYPE r, int q) {
  float quality = 5; // higher the number, lower the quality - selected by experiment
  int steps = ceil(r / quality) + 2; // + 2 to include start and end
  float delta = -(M_PI / 2) / steps;
  float theta = (M_PI / 2) * q; // select start theta for this quadrant
  for(int i = 0; i <= steps; i++) {
    PP_COORD_TYPE xo = sin(theta) * r, yo = cos(theta) * r;
    pp_path_add_point(path, (pp_point_t){cx + xo, cy + yo});
    theta += delta;
  }
}

pp_poly_t* f_rrect(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE tlr, PP_COORD_TYPE trr, PP_COORD_TYPE brr, PP_COORD_TYPE blr) {
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

pp_poly_t* rrect(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE tlr, PP_COORD_TYPE trr, PP_COORD_TYPE brr, PP_COORD_TYPE blr, PP_COORD_TYPE t) {
  pp_poly_t *outer = f_rrect(x, y, w, h, tlr, trr, brr, blr);

  tlr = _pp_max(0, tlr - t);
  trr = _pp_max(0, trr - t);
  brr = _pp_max(0, brr - t);
  blr = _pp_max(0, blr - t);

  pp_poly_t *inner = f_rrect(x + t, y + t, w - 2 * t, h - 2 * t, tlr, trr, brr, blr);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* f_rect(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  pp_path_add_points(path, (pp_point_t[]){{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, 4);
  return poly;
}

pp_poly_t* rect(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE w, PP_COORD_TYPE h, PP_COORD_TYPE t) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *outer = pp_poly_add_path(poly), *inner = pp_poly_add_path(poly);
  pp_path_add_points(outer, (pp_point_t[]){{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, 4);
  x += t; y += t; w -= 2 * t; h -= 2 * t;
  pp_path_add_points(inner, (pp_point_t[]){{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, 4);
  return poly;
}

pp_poly_t* f_reg(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, int s) {
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

pp_poly_t* reg(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, int s, PP_COORD_TYPE t) {
  pp_poly_t *outer = f_reg(x, y, r, s);
  pp_poly_t *inner = f_reg(x, y, r - t, s);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* f_circ(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r) {
  int s = _pp_max(8, r);
  return f_reg(x, y, r, s);
}

pp_poly_t* circ(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, PP_COORD_TYPE t) {
  pp_poly_t *outer = f_circ(x, y, r);
  pp_poly_t *inner = f_circ(x, y, r - t);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* f_star(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  for(int i = 0; i < c * 2; i++) {
    pp_point_t p;
    float step = ((M_PI * 2) / (float)(c * 2)) * (float)i;
    PP_COORD_TYPE r = i % 2 == 0 ? or : ir; 
    p.x = sin(step) * r + x;
    p.y = cos(step) * r + y;
    pp_path_add_point(path, p);
  }  
  return poly;
}

pp_poly_t* star(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir, PP_COORD_TYPE t) {
  pp_poly_t *outer = f_star(x, y, c, or, ir);
  pp_poly_t *inner = f_star(x, y, c, or - (t * or / ir), ir - t);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;
}

pp_poly_t* f_gear(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  for(int i = 0; i < c * 2; i++) {
    pp_point_t p;
    float step = ((M_PI * 2) / (float)(c * 2)) * (float)i - 0.05;
    PP_COORD_TYPE r = i % 2 == 0 ? or : ir; 
    p.x = sin(step) * r + x;
    p.y = cos(step) * r + y;
    pp_path_add_point(path, p);

    step = ((M_PI * 2) / (float)(c * 2)) * (float)i + 0.05;
    r = i % 2 == 1 ? or : ir; 
    p.x = sin(step) * r + x;
    p.y = cos(step) * r + y;
    pp_path_add_point(path, p);
  }  
  return poly;
}

pp_poly_t* gear(PP_COORD_TYPE x, PP_COORD_TYPE y, int c, PP_COORD_TYPE or, PP_COORD_TYPE ir, PP_COORD_TYPE t) {
  pp_poly_t *outer = f_gear(x, y, c, or, ir);
  pp_poly_t *inner = f_circ(x, y, ir - t);
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;  
}

pp_poly_t* line(PP_COORD_TYPE x1, PP_COORD_TYPE y1, PP_COORD_TYPE x2, PP_COORD_TYPE y2, PP_COORD_TYPE t) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);

  // create a normalised perpendicular vector
  pp_point_t v = {y2 - y1, x2 - x1};
  float mag = sqrt(v.x * v.x + v.y * v.y);
  t /= 2.0f; v.x /= mag; v.y /= mag; v.x *= -t; v.y *= t;
  pp_path_add_points(path, (pp_point_t[]){{x1 + v.x, y1 + v.y}, {x2 + v.x, y2 + v.y}, {x2 - v.x, y2 - v.y}, {x1 - v.x, y1 - v.y}}, 4);
  return poly; 
}

pp_poly_t *pie(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, float sa, float ea) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  pp_path_add_point(path, (pp_point_t){x, y});

  sa = sa * (M_PI / 180.0f);
  ea = ea * (M_PI / 180.0f);
  int s = _pp_max(8, r);
  float astep = (ea - sa) / s;
  for(float a = sa; a < ea; a+=astep) {
    pp_point_t p;
    p.x = sin(a) * r + x;
    p.y = cos(a) * r + y;
    pp_path_add_point(path, p);
  }

  return poly;
}


pp_poly_t *arc(PP_COORD_TYPE x, PP_COORD_TYPE y, PP_COORD_TYPE r, float sa, float ea, PP_COORD_TYPE thickness) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);

  sa = sa * (M_PI / 180.0f);
  ea = ea * (M_PI / 180.0f);

  int s = _pp_max(8, r);
  float astep = (ea - sa) / s;
  float a = sa;
  for(int i = 0; i <= s; i++) {
    pp_point_t p;
    p.x = sin(a) * r + x;
    p.y = cos(a) * r + y;
    pp_path_add_point(path, p);
    a += astep;
  }

  r -= thickness;
  a = ea;
  for(int i = 0; i <= s; i++) {
    pp_point_t p;
    p.x = sin(a) * r + x;
    p.y = cos(a) * r + y;
    pp_path_add_point(path, p);
    a -= astep;
  }

  return poly;
}

int main() { 
  pp_tile_callback(blend_tile);
  pp_antialias(PP_AA_X16);
  //pp_clip(50, 50, WIDTH - 400, HEIGHT - 400);
  pp_clip(0, 0, WIDTH, HEIGHT);

  for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      buffer[y][x] = ((x / 8) + (y / 8)) % 2 == 0 ? create_colour(16, 32, 48, 255) : create_colour(0, 0, 0, 255);
    }
  }

  PP_COORD_TYPE thickness = 20;
  PP_COORD_TYPE size = (1024 / 10) - 20;
  //pp_poly_t *poly = circle(0, 0, 256, 25);
  for(int y = 0; y < 5; y++) {
    for(int x = 0; x < 5; x++) {
      int i = x + y * 5;
      pp_poly_t *poly = NULL;
      switch(i) {
        case 0: {
          poly = f_rect(-size, -size, size * 2, size * 2);        
        }break;
        case 1: {
          poly = rect(-size, -size, size * 2, size * 2, thickness);
        }break;
        case 2: {
          poly = f_circ(0, 0, size);        
        }break;
        case 3: {
          poly = circ(0, 0, size, thickness);        
        }break;
        case 4: {          
          poly = f_reg(0, 0, size, 5);        
        }break;
        case 5: {          
          poly = reg(0, 0, size, 5, thickness);        
        }break;
        case 6: {
          poly = f_star(0, 0, 7, size, size * .75);        
        }break;
        case 7: {
          poly = star(0, 0, 7, size, size * .75, thickness);        
        }break;
        case 8: {
          poly = f_rrect(-size, -size, size * 2, size * 2, size * .5, size * .05, size * .15, size * .7);
        }break;
        case 9: {
          poly = rrect(-size, -size, size * 2, size * 2, size * .5, size * .05, size * .15, size * .7, thickness);
        }break;
        case 10: {
          poly = f_gear(0, 0, 10, size, size * .75);        
        }break;
        case 11: {
          poly = gear(0, 0, 10, size, size * .75, thickness);        
        }break;        
        case 12: {
          poly = pp_poly_new();
          for(int i = 0; i < 10; i++) {
            pp_poly_merge(poly, line(-size + i * (size / 5), -size, -size + i * (size / 5), size, i));        
          }
        }break;
        case 13: {
          poly = line(-size, 0, size, 0, thickness);        
          pp_poly_merge(poly, line(0, -size, 0, size, thickness));
          pp_poly_merge(poly, line(-size * .7, -size * .7, size * .7, size * .7, thickness));
          pp_poly_merge(poly, line(size * .7, -size * .7, -size * .7, size * .7, thickness));
        }break;        
        case 14: {
          poly = f_rrect(-size, -size, size * 2, size * 2, size * .5, size * .05, size * .15, size * .7);
          pp_poly_t *merge = gear(0, 0, 10, size * .8, size * .6, thickness);
          pp_poly_merge(poly, merge);
        }break;
        case 15: {
          poly = star(0, 0, 7, size * 0.95, size * .75, thickness);
          pp_poly_t *merge = circ(0, 0, size, thickness);        
          pp_poly_merge(poly, merge);
        }break;
        case 16: {
          poly = pie(0, 0, size, 30, 290);
        }break;
        case 17: {
          poly = arc(0, 0, size, 40, 260, thickness);
        }break;
      }

      if(poly) {
        pp_mat3_t t = pp_mat3_identity();
        int cell_size = (1024 / 5);

        pp_mat3_translate(&t, (cell_size / 2) + (x * cell_size), (cell_size / 2) + (y * cell_size));
        int angle = rand() % 360;
        pp_mat3_rotate(&t, angle);
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