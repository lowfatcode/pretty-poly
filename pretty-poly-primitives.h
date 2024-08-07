/*

  Pretty Poly 🦜 - super-sampling polygon renderer for low resource platforms.

  Jonathan Williamson, August 2022
  Examples, source, and more: https://github.com/lowfatcode/pretty-poly
  MIT License https://github.com/lowfatcode/pretty-poly/blob/main/LICENSE

  An easy way to render high quality graphics in embedded applications running 
  on resource constrained microcontrollers such as the Cortex M0 and up.         

    - Renders polygons: concave, self-intersecting, multi contour, holes, etc.
    - C11 header only library: simply copy the header file into your project
    - Tile based renderer: low memory footprint, cache coherency
    - Low memory usage: ~4kB of heap memory required
    - High speed on low resource platforms: optionally no floating point
    - Antialiasing modes: X1 (none), X4 and X16 super sampling
    - Bounds clipping: all results clipped to supplied clip rectangle
    - Pixel format agnostic: renders a "tile" to blend into your framebuffer
    - Support for hardware interpolators on rp2040 (thanks @MichaelBell!)

  Contributor bwaaaaaarks! 🦜

    @MichaelBell - lots of bug fixes, performance boosts, and suggestions. 
    @gadgetoid - integrating into the PicoVector library and testing.
    
*/

#ifndef PPP_INCLUDE_H
#define PPP_INCLUDE_H

#include "pretty-poly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  PP_COORD_TYPE x, y, w, h;      // coordinates
  PP_COORD_TYPE s;               // stroke thickness (0 == filled)
  PP_COORD_TYPE r1, r2, r3, r4;  // corner radii (r1 = top left then clockwise)
} ppp_rect_def;

typedef struct {
  PP_COORD_TYPE x, y;            // coordinates
  PP_COORD_TYPE r;               // radius
  int e;                         // edge count
  PP_COORD_TYPE s;               // stroke thickness (0 == filled)
} ppp_regular_def;

typedef struct {
  PP_COORD_TYPE x, y;            // coordinates
  PP_COORD_TYPE r;               // radius
  PP_COORD_TYPE s;               // stroke thickness (0 == filled)
} ppp_circle_def;

typedef struct {
  PP_COORD_TYPE x, y;            // coordinates
  PP_COORD_TYPE r;               // radius
  PP_COORD_TYPE s;               // stroke thickness (0 == filled)
  PP_COORD_TYPE f, t;            // angle from and to
} ppp_arc_def;

pp_poly_t* ppp_rect(ppp_rect_def d);
pp_poly_t* ppp_regular(ppp_regular_def d);
pp_poly_t* ppp_circle(ppp_circle_def d);
pp_poly_t* ppp_arc(ppp_arc_def d);

#ifdef __cplusplus
}
#endif

#ifdef PPP_IMPLEMENTATION

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

void _ppp_rrect_corner(pp_path_t *path, PP_COORD_TYPE cx, PP_COORD_TYPE cy, PP_COORD_TYPE r, int q) {
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

void _ppp_rrect_path(pp_path_t *path, ppp_rect_def d) {
  d.r1 == 0 ? pp_path_add_point(path, (pp_point_t){d.x, d.y})             : _ppp_rrect_corner(path, d.x + d.r1, d.y + d.r1, d.r1, 3);
  d.r2 == 0 ? pp_path_add_point(path, (pp_point_t){d.x + d.w, d.y})       : _ppp_rrect_corner(path, d.x + d.w - d.r2, d.y + d.r2, d.r2, 2);
  d.r3 == 0 ? pp_path_add_point(path, (pp_point_t){d.x + d.w, d.y + d.h}) : _ppp_rrect_corner(path, d.x + d.w - d.r3, d.y + d.h - d.r3, d.r3, 1);
  d.r4 == 0 ? pp_path_add_point(path, (pp_point_t){d.x, d.y})             : _ppp_rrect_corner(path, d.x + d.r4, d.y + d.h - d.r4, d.r4, 0);
}

pp_poly_t* ppp_rect(ppp_rect_def d) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  if(d.r1 == 0.0f && d.r2 ==  0.0f && d.r3 ==  0.0f && d.r4 == 0.0f) { // non rounded rect
    pp_path_add_points(path, (pp_point_t[]){{d.x, d.y}, {d.x + d.w, d.y}, {d.x + d.w, d.y + d.h}, {d.x, d.y + d.h}}, 4);
    if(d.s != 0) { // stroked, not filled
      d.x += d.s; d.y += d.s; d.w -= 2 * d.s; d.h -= 2 * d.s;
      pp_path_t *inner = pp_poly_add_path(poly);
      pp_path_add_points(inner, (pp_point_t[]){{d.x, d.y}, {d.x + d.w, d.y}, {d.x + d.w, d.y + d.h}, {d.x, d.y + d.h}}, 4);      
    }
  }else{ // rounded rect
    _ppp_rrect_path(path, d);
    if(d.s != 0) { // stroked, not filled
      d.x += d.s; d.y += d.s; d.w -= 2 * d.s; d.h -= 2 * d.s;
      d.r1 = _pp_max(0, d.r1 - d.s);
      d.r2 = _pp_max(0, d.r2 - d.s);
      d.r3 = _pp_max(0, d.r3 - d.s);
      d.r4 = _pp_max(0, d.r4 - d.s);
      pp_path_t *inner = pp_poly_add_path(poly);
      _ppp_rrect_path(inner, d);
    }
  }
  return poly;
}

pp_poly_t* ppp_regular(ppp_regular_def d) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  pp_path_t *inner = d.s != 0.0f ? pp_poly_add_path(poly) : NULL;
  for(int i = 0; i < d.e; i++) {
    float theta = ((M_PI * 2.0f) / (float)d.e) * (float)i;
    pp_path_add_point(path, (pp_point_t){sin(theta) * d.r + d.x, cos(theta) * d.r + d.y});
    if(inner) {
      pp_path_add_point(inner, (pp_point_t){sin(theta) * (d.r - d.s) + d.x, cos(theta) * (d.r - d.s) + d.y});
    }
  }
  return poly;
}

pp_poly_t* ppp_circle(ppp_circle_def d) {
  int e = _pp_max(8, d.r); // edge count
  ppp_regular_def r = {d.x, d.y, d.r, e, d.s};
  return ppp_regular(r);
}

pp_poly_t* ppp_arc(ppp_arc_def d) {
  pp_poly_t *poly = pp_poly_new();
  pp_path_t *path = pp_poly_add_path(poly);
  pp_path_t *inner = d.s == 0.0f ? NULL : calloc(1, sizeof(pp_path_t));

  // no thickness, so add centre point to make pie shape
  if(!inner) pp_path_add_point(path, (pp_point_t){d.x, d.y});

  d.f = d.f * (M_PI / 180.0f); d.t = d.t * (M_PI / 180.0f); // to radians
  int s = _pp_max(8, d.r); float astep = (d.t - d.f) / s; float a = d.f;
  for(int i = 0; i <= s; i++) {
    pp_path_add_point(path, (pp_point_t){sin(a) * d.r + d.x, cos(a) * d.r + d.y});
    if(inner) {
      pp_path_add_point(inner, (pp_point_t){sin(d.t - (a - d.f)) * (d.r - d.s) + d.x, cos(d.t - (a - d.f)) * (d.r - d.s) + d.y});
    }
    a += astep;
  }

  if(inner) { // append the inner path
    pp_path_add_points(path, inner->points, inner->count);
    free(inner->points); free(inner);
  }

  return poly;
}

#endif // PPP_IMPLEMENTATION

#endif // PPP_INCLUDE_H