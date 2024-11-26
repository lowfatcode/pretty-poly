#include "float.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define PP_IMPLEMENTATION
#define PP_DEBUG
#include "pretty-poly.h"
#define PPP_IMPLEMENTATION
#include "pretty-poly-primitives.h"
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
  pp_poly_t *inner = ppp_circle((ppp_circle_def){x, y, ir - t});
  outer->paths->next = inner->paths;
  inner->paths = NULL;
  free(inner);
  return outer;  
  //return NULL;
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
          ppp_rect_def r = {-size, -size, size * 2, size * 2, 0, 0, 0, 0, 0};
          poly = ppp_rect(r);
        }break;
        case 1: {
          ppp_rect_def r = {-size, -size, size * 2, size * 2, thickness};
          poly = ppp_rect(r);
        }break;
        case 2: {
          ppp_circle_def c = {0, 0, size};
          poly = ppp_circle(c);
        }break;
        case 3: {
          ppp_circle_def c = {0, 0, size, thickness};
          poly = ppp_circle(c);
        }break;
        case 4: {          
          ppp_regular_def r = {0, 0, size, 5};
          poly = ppp_regular(r);
        }break;
        case 5: {          
          ppp_regular_def r = {0, 0, size, 5, thickness};
          poly = ppp_regular(r);
        }break;
        case 6: {
          ppp_star_def r = {0, 0, 7, size, size * 0.75f};
          poly = ppp_star(r);
        }break;
        case 7: {
          ppp_star_def r = {0, 0, 7, size, size * 0.75f, thickness};
          poly = ppp_star(r);
        }break;
        case 8: {
          ppp_rect_def r = {-size, -size, size * 2, size * 2, 0, size * .5, size * .05, size * .15, size * .7};
          poly = ppp_rect(r);
        }break;
        case 9: {
          ppp_rect_def r = {-size, -size, size * 2, size * 2,thickness, size * .5, size * .05, size * .15, size * .7};
          poly = ppp_rect(r);
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
            pp_poly_merge(poly, ppp_line((ppp_line_def){-size + i * (size / 5), -size, -size + i * (size / 5), size, i}));        
          }
        }break;
        case 13: {
          ppp_line_def r = {-size, 0, size, 0, thickness};
          poly = ppp_line(r);        
          pp_poly_merge(poly, ppp_line((ppp_line_def){0, -size, 0, size, thickness}));
          pp_poly_merge(poly, ppp_line((ppp_line_def){-size * .7, -size * .7, size * .7, size * .7, thickness}));
          pp_poly_merge(poly, ppp_line((ppp_line_def){size * .7, -size * .7, -size * .7, size * .7, thickness}));
        }break;        
        case 14: {
          // poly = f_rrect(-size, -size, size * 2, size * 2, size * .5, size * .05, size * .15, size * .7);
          // pp_poly_t *merge = gear(0, 0, 10, size * .8, size * .6, thickness);
          // pp_poly_merge(poly, merge);
        }break;
        case 15: {
          /*poly = star(0, 0, 7, size * 0.95, size * .75, thickness);
          pp_poly_t *merge = circ(0, 0, size, thickness);        
          pp_poly_merge(poly, merge);*/
        }break;
        case 16: {
          ppp_arc_def p = {0, 0, size, 0, 30, 290};
          poly = ppp_arc(p);
        }break;
        case 17: {
          ppp_arc_def p = {0, 0, size, thickness, 30, 290};
          poly = ppp_arc(p);
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