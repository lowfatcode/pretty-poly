// Pretty Poly 🦜 - super-sampling polygon renderer for low resource platforms.
// Jonathan Williamson, August 2022
// Examples, source, and more: https://github.com/lowfatcode/pretty-poly
// MIT License https://github.com/lowfatcode/pretty-poly/blob/main/LICENSE
// 
// An easy way to render high quality text in embedded applications running 
// on resource constrained microcontrollers such as the Cortex M0 and up.         
//
//   - renders polygons: concave, self-intersecting, multi contour, holes, etc.
//   - C17 header only library: simply copy the header file into your project
//   - tile based renderer: low memory footprint, cache coherency
//   - low memory usage: ~4kB of heap memory required
//   - high speed on low resource platforms: optionally no floating point
//   - antialiasing modes: X1 (none), X4 and X16 super sampling
//   - bounds clipping: all results clipped to supplied clip rectangle
//   - pixel format agnostic: renders a "tile" to blend into your framebuffer
//   - support for hardware interpolators on rp2040 (thanks @MichaelBell!)

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#ifndef PP_COORD_TYPE
#define PP_COORD_TYPE float
#endif

#ifndef PP_MAX_INTERSECTIONS
#define PP_MAX_INTERSECTIONS 16
#endif

#ifndef PP_TILE_BUFFER_SIZE
#define PP_TILE_BUFFER_SIZE 1024
#endif

#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
#define USE_RP2040_INTERP
#include "hardware/interp.h"
#endif

#ifdef PP_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif


#ifdef __cplusplus
extern "C" {
#endif

// 3x3 matrix type allows for optional transformation of polygon during render
typedef struct {
  float v00, v10, v20, v01, v11, v21, v02, v12, v22;
} pp_mat3_t;
static pp_mat3_t pp_mat3_identity();
static pp_mat3_t pp_mat3_rotation(float a);
static pp_mat3_t pp_mat3_translation(float x, float y);
static pp_mat3_t pp_mat3_scale(float x, float y);
static pp_mat3_t pp_mat3_mul(pp_mat3_t m1, pp_mat3_t m2);

// point type used to hold polygon vertex coordinates
typedef struct {
  PP_COORD_TYPE x, y;
} pp_point_t;
static pp_point_t pp_point_add(pp_point_t p1, pp_point_t p2);
static pp_point_t pp_point_sub(pp_point_t p1, pp_point_t p2);
static pp_point_t pp_point_mul(pp_point_t p1, pp_point_t p2);
static pp_point_t pp_point_div(pp_point_t p1, pp_point_t p2);
static pp_point_t pp_point_transform(pp_point_t p, pp_mat3_t *m);

// rect type
typedef struct {
  int32_t x, y, w, h;    
} pp_rect_t;
bool pp_rect_empty(pp_rect_t r);
static pp_rect_t pp_rect_intersection(pp_rect_t r1, pp_rect_t r2);
static pp_rect_t pp_rect_merge(pp_rect_t r1, pp_rect_t r2);
static pp_rect_t pp_rect_transform(pp_rect_t r, pp_mat3_t *m);

// antialias levels
typedef enum {PP_AA_NONE = 0, PP_AA_X4 = 1, PP_AA_X16 = 2} pp_antialias_t;

typedef struct {
  pp_rect_t bounds;
  uint32_t stride;
  uint8_t *data;
} pp_tile_t;

typedef struct {
  pp_point_t *points;
  uint32_t point_count;
} pp_contour_t;

typedef struct {
  pp_contour_t *contours;
  uint32_t contour_count;
} pp_polygon_t;

// user settings
typedef void (*pp_tile_callback_t)(const pp_tile_t tile);

pp_rect_t           _pp_clip;
pp_tile_callback_t  _pp_tile_callback;
pp_antialias_t      _pp_antialias;
pp_mat3_t          *_pp_transform;


static void pp_clip(pp_rect_t clip);
static void pp_tile_callback(pp_tile_callback_t callback);
static void pp_antialias(pp_antialias_t antialias);
static void pp_transform(pp_mat3_t *transform);

#ifdef __cplusplus
}
#endif

// helpers
int _pp_max(int32_t a, int32_t b) { return a > b ? a : b; }
int _pp_min(int32_t a, int32_t b) { return a < b ? a : b; }

// pp_mat3_t implementation
static pp_mat3_t pp_mat3_identity() {
  pp_mat3_t m; memset(&m, 0, sizeof(pp_mat3_t)); m.v00 = m.v11 = m.v22 = 1.0f; return m;}
static pp_mat3_t pp_mat3_rotation(float a) {
  float c = cosf(a), s = sinf(a); pp_mat3_t r = pp_mat3_identity();
  r.v00 = c; r.v01 = s; r.v10 = -s; r.v11 = c; return r;}
static pp_mat3_t pp_mat3_translation(float x, float y) {
  pp_mat3_t r = pp_mat3_identity(); r.v02 = x; r.v12 = y; return r;}
static pp_mat3_t pp_mat3_scale(float x, float y) {
  pp_mat3_t r = pp_mat3_identity(); r.v00 = x; r.v11 = y; return r;}
static pp_mat3_t pp_mat3_mul(pp_mat3_t m1, pp_mat3_t m2) {
  pp_mat3_t r;
  r.v00 = m1.v00 * m2.v00 + m1.v01 * m2.v10 + m1.v02 * m2.v20;
  r.v01 = m1.v00 * m2.v01 + m1.v01 * m2.v11 + m1.v02 * m2.v21;
  r.v02 = m1.v00 * m2.v02 + m1.v01 * m2.v12 + m1.v02 * m2.v22;
  r.v10 = m1.v10 * m2.v00 + m1.v11 * m2.v10 + m1.v12 * m2.v20;
  r.v11 = m1.v10 * m2.v01 + m1.v11 * m2.v11 + m1.v12 * m2.v21;
  r.v12 = m1.v10 * m2.v02 + m1.v11 * m2.v12 + m1.v12 * m2.v22;
  r.v20 = m1.v20 * m2.v00 + m1.v21 * m2.v10 + m1.v22 * m2.v20;
  r.v21 = m1.v20 * m2.v01 + m1.v21 * m2.v11 + m1.v22 * m2.v21;
  r.v22 = m1.v20 * m2.v02 + m1.v21 * m2.v12 + m1.v22 * m2.v22;    
  return r;
}

// pp_point_t implementation
static pp_point_t pp_point_add(pp_point_t p1, pp_point_t p2) {
  return (pp_point_t){.x = p1.x + p2.x, .y = p1.y + p2.y};
}
static pp_point_t pp_point_sub(pp_point_t p1, pp_point_t p2) {
  return (pp_point_t){.x = p1.x - p2.x, .y = p1.y - p2.y};
}
static pp_point_t pp_point_mul(pp_point_t p1, pp_point_t p2) {
  return (pp_point_t){.x = p1.x * p2.x, .y = p1.y * p2.y};
}
static pp_point_t pp_point_div(pp_point_t p1, pp_point_t p2) {
  return (pp_point_t){.x = p1.x / p2.x, .y = p1.y / p2.y};
}
static pp_point_t pp_point_transform(pp_point_t p, pp_mat3_t *m) {
  return (pp_point_t){
    .x = (m->v00 * p.x + m->v01 * p.y + m->v02),
    .y = (m->v10 * p.x + m->v11 * p.y + m->v12)
  };
}

// pp_rect_t implementation
bool pp_rect_empty(pp_rect_t r) {
  return r.w == 0 || r.h == 0;
}
pp_rect_t pp_rect_intersection(pp_rect_t r1, pp_rect_t r2) {
  return (pp_rect_t){
    .x = _pp_max(r1.x, r2.x), .y = _pp_max(r1.y, r2.y),
    .w = _pp_max(0, _pp_min(r1.x + r1.w, r2.x + r2.w) - _pp_max(r1.x, r2.x)),
    .h = _pp_max(0, _pp_min(r1.y + r1.h, r2.y + r2.h) - _pp_max(r1.y, r2.y))
  };
}
pp_rect_t pp_rect_merge(pp_rect_t r1, pp_rect_t r2) {
  return (pp_rect_t){
    .x = _pp_min(r1.x, r2.x), 
    .y = _pp_min(r1.y, r2.y),
    .w = _pp_max(r1.x + r1.w, r2.x + r2.w) - _pp_min(r1.x, r2.x),
    .h = _pp_max(r1.y + r1.h, r2.y + r2.h) - _pp_min(r1.y, r2.y)
  };
}
pp_rect_t pp_rect_transform(pp_rect_t r, pp_mat3_t *m) {
  pp_point_t tl = (pp_point_t){.x = (PP_COORD_TYPE)r.x, .y = (PP_COORD_TYPE)r.y};
  pp_point_t tr = (pp_point_t){.x = (PP_COORD_TYPE)r.x + (PP_COORD_TYPE)r.w, .y = (PP_COORD_TYPE)r.y};
  pp_point_t bl = (pp_point_t){.x = (PP_COORD_TYPE)r.x, .y = (PP_COORD_TYPE)r.y + (PP_COORD_TYPE)r.h};
  pp_point_t br = (pp_point_t){.x = (PP_COORD_TYPE)r.x + (PP_COORD_TYPE)r.w, .y = (PP_COORD_TYPE)r.y + (PP_COORD_TYPE)r.h};

  tl = pp_point_transform(tl, m);
  tr = pp_point_transform(tr, m);
  bl = pp_point_transform(bl, m);
  br = pp_point_transform(br, m);

  PP_COORD_TYPE minx = _pp_min(tl.x, _pp_min(tr.x, _pp_min(bl.x, br.x)));
  PP_COORD_TYPE miny = _pp_min(tl.y, _pp_min(tr.y, _pp_min(bl.y, br.y)));
  PP_COORD_TYPE maxx = _pp_max(tl.x, _pp_max(tr.x, _pp_max(bl.x, br.x)));
  PP_COORD_TYPE maxy = _pp_max(tl.y, _pp_max(tr.y, _pp_max(bl.y, br.y)));

  return (pp_rect_t){.x = (int32_t)minx, .y = (int32_t)miny, .w = (int32_t)(maxx - minx), .h = (int32_t)(maxy - miny)};
}

// pp_tile_t implementation
int pp_tile_get_value(const pp_tile_t *tile, const int x, const int y) {
  return tile->data[x + y * tile->stride] * (255 >> _pp_antialias >> _pp_antialias);
}

// pp_contour_t implementation
pp_rect_t pp_contour_bounds(const pp_contour_t c) {
  int minx = c.points[0].x, maxx = minx;
  int miny = c.points[0].y, maxy = miny;
  for(uint32_t i = 1; i < c.point_count; i++) {
    minx = _pp_min(minx, c.points[i].x);
    miny = _pp_min(miny, c.points[i].y);
    maxx = _pp_max(maxx, c.points[i].x); 
    maxy = _pp_max(maxy, c.points[i].y);
  }
  return (pp_rect_t){.x = minx, .y = miny, .w = maxx - minx, .h = maxy - miny};
}

pp_rect_t pp_polygon_bounds(pp_polygon_t p) {
  if(p.contour_count == 0) {return (pp_rect_t){};}
  pp_rect_t b = pp_contour_bounds(p.contours[0]);
  for(uint32_t i = 1; i < p.contour_count; i++) {
    b = pp_rect_merge(b, pp_contour_bounds(p.contours[i]));
  }
  return b;
}

// buffer that each tile is rendered into before callback
// allocate one extra byte to allow a small optimization in the row renderer
const uint32_t tile_buffer_size = PP_TILE_BUFFER_SIZE;
uint8_t tile_buffer[tile_buffer_size + 1];

// polygon node buffer handles at most 16 line intersections per scanline
// is this enough for cjk/emoji? (requires a 2kB buffer)
const uint32_t node_buffer_size = PP_MAX_INTERSECTIONS * 2;
int32_t nodes[node_buffer_size][PP_MAX_INTERSECTIONS * 2];
uint32_t node_counts[node_buffer_size];

// default tile bounds to X1 antialiasing
pp_rect_t tile_bounds;

void pp_clip(pp_rect_t clip) {
  _pp_clip = clip;
}

void pp_tile_callback(pp_tile_callback_t callback) {
  _pp_tile_callback = callback;
}

void pp_antialias(pp_antialias_t antialias) {
  _pp_antialias = antialias;
  // recalculate the tile size for rendering based on antialiasing level
  int tile_height = node_buffer_size >> _pp_antialias;
  tile_bounds = (pp_rect_t){.x = 0, .y = 0, .w = (int)(tile_buffer_size / tile_height), .h = tile_height};
}

void pp_transform(pp_mat3_t *transform) {
  _pp_transform = transform;
}


// dy step (returns 1, 0, or -1 if the supplied value is > 0, == 0, < 0)
int32_t sign(int32_t v) {
  // assumes 32-bit int/unsigned
  return ((uint32_t)-v >> 31) - ((uint32_t)v >> 31);
}

// write out the tile bits
void debug_tile(const pp_tile_t *tile) {
  debug("  - tile %d, %d (%d x %d)\n", tile->bounds.x, tile->bounds.y, tile->bounds.w, tile->bounds.h);
  for(int32_t y = 0; y < tile->bounds.h; y++) {
    debug("[%3d]: ", y);
    for(int32_t x = 0; x < tile->bounds.w; x++) {
      debug("%02x", pp_tile_get_value(tile, x, y));
    }  
    debug("\n");              
  }
  debug("-----------------------\n");
}

void add_line_segment_to_nodes(const pp_point_t start, const pp_point_t end) {
  int32_t sx = start.x, sy = start.y, ex = end.x, ey = end.y;

  if(ey < sy) {
    // swap endpoints if line "pointing up", we do this because we
    // alway skip the last scanline (so that polygons can but cleanly
    // up against each other without overlap)
    int32_t ty = sy; sy = ey; ey = ty;
    int32_t tx = sx; sx = ex; ex = tx;
  }

  // Early out if line is completely outside the tile, or has no lines
  if (ey < 0 || sy >= (int)node_buffer_size || sy == ey) return;

  debug("      + line segment from %d, %d to %d, %d\n", sx, sy, ex, ey);

  // Determine how many in-bounds lines to render
  int y = _pp_max(0, sy);
  int count = _pp_min((int)node_buffer_size, ey) - y;

  // Handle cases where x is completely off to one side or other
  if (_pp_max(sx, ex) <= 0) {
    while (count--) {
      nodes[y][node_counts[y]++] = 0;
      ++y;
    }
    return;
  }

  const int full_tile_width = (tile_bounds.w << _pp_antialias);
  if (_pp_min(sx, ex) >= full_tile_width) {
    while (count--) {
      nodes[y][node_counts[y]++] = full_tile_width;
      ++y;
    }
    return;
  }

  // Normal case
  int x = sx;
  int e = 0;

  const int xinc = sign(ex - sx);
  const int einc = abs(ex - sx) + 1;
  const int dy = ey - sy;

  // If sy < 0 jump to the start, note this does use a divide
  // but potentially saves many wasted loops below, so is likely worth it.
  if (sy < 0) {
    e = einc * -sy;
    int xjump = e / dy;
    e -= dy * xjump;
    x += xinc * xjump;
  }

#ifdef USE_RP2040_INTERP
  interp1->base[1] = full_tile_width;
  interp1->accum[0] = x;

  // loop over scanlines
  while(count--) {
    // consume accumulated error
    while(e > dy) {e -= dy; interp1->add_raw[0] = xinc;}

    // clamp node x value to tile bounds
    const int nx = interp1->peek[0];
    debug("      + adding node at %d, %d\n", x, y);
    // add node to node list
    nodes[y][node_counts[y]++] = nx;

    // step to next scanline and accumulate error
    y++;
    e += einc;
  }
#else
  // loop over scanlines
  while(count--) {
    // consume accumulated error
    while(e > dy) {e -= dy; x += xinc;}

    // clamp node x value to tile bounds
    int nx = _pp_max(_pp_min(x, full_tile_width), 0);        
    debug("      + adding node at %d, %d\n", x, y);
    // add node to node list
    nodes[y][node_counts[y]++] = nx;

    // step to next scanline and accumulate error
    y++;
    e += einc;
  }
#endif
}

void build_nodes(const pp_contour_t contour, const pp_tile_t tile) {
  PP_COORD_TYPE aa_scale = (PP_COORD_TYPE)(1 << _pp_antialias);

  pp_point_t tile_origin = (pp_point_t) {
    .x = tile.bounds.x * aa_scale,
    .y = tile.bounds.y * aa_scale
  };

  // start with the last point to close the loop
  pp_point_t last = {
    .x = (contour.points[contour.point_count - 1].x) * aa_scale,
    .y = (contour.points[contour.point_count - 1].y) * aa_scale
  };

  if(_pp_transform) {
    last = pp_point_transform(last, _pp_transform);
  }

  last = pp_point_sub(last, tile_origin);

  for(uint32_t i = 0; i < contour.point_count; i++) {
    pp_point_t point = {
      .x = (contour.points[i].x) * aa_scale,
      .y = (contour.points[i].y) * aa_scale
    };

    if(_pp_transform) {
      point = pp_point_transform(point, _pp_transform);
    }

    point = pp_point_sub(point, tile_origin);

    add_line_segment_to_nodes(last, point);
    
    last = point;
  }
}

int compare_nodes(const void* a, const void* b) {
  return *((int*)a) - *((int*)b);
}

void render_nodes(const pp_tile_t tile, pp_rect_t *bounds) {
  int maxy = -1;
  bounds->y = 0;
  bounds->x = tile.bounds.w;
  int maxx = 0;
  PP_COORD_TYPE aa_scale = (PP_COORD_TYPE)(1 << _pp_antialias);
  int anitialias_mask = (1 << _pp_antialias) - 1;

  for(uint32_t y = 0; y < node_buffer_size; y++) {
    if(node_counts[y] == 0) {
      if (y == bounds->y) ++bounds->y;
      continue;
    }

    qsort(&nodes[y][0], node_counts[y], sizeof(int), compare_nodes);

    unsigned char* row_data = &tile.data[(y >> _pp_antialias) * tile.stride];
    bool rendered_any = false;
    for(uint32_t i = 0; i < node_counts[y]; i += 2) {
      int sx = nodes[y][i + 0];
      int ex = nodes[y][i + 1];

      if(sx == ex) {
        continue;
      }

      rendered_any = true;

      maxx = _pp_max((ex - 1) >> _pp_antialias, maxx);

      debug(" - render span at %d from %d to %d\n", y, sx, ex);

      if (_pp_antialias) {
        int ax = sx / aa_scale;
        const int aex = ex / aa_scale;

        bounds->x = _pp_min(ax, bounds->x);

        if (ax == aex) {
          row_data[ax] += ex - sx;
          continue;
        }

        row_data[ax] += aa_scale - (sx & anitialias_mask);
        for(ax++; ax < aex; ax++) {
          row_data[ax] += aa_scale;
        }

        // This might add 0 to the byte after the end of the row, we pad the tile data
        // by 1 byte to ensure that is OK
        row_data[ax] += ex & anitialias_mask;
      } else {
        bounds->x = _pp_min(sx, bounds->x);
        for(int x = sx; x < ex; x++) {
          row_data[x]++;
        }       
      }
    }

    if (rendered_any) {
      debug("  - rendered line %d\n", y);
      maxy = y;
    }
    else if (y == bounds->y) {
      debug(" - render nothing on line %d\n", y);
      ++bounds->y;
    }
  }

  bounds->y >>= _pp_antialias;
  maxy >>= _pp_antialias;
  bounds->w = (maxx >= bounds->x) ? maxx + 1 - bounds->x : 0;
  bounds->h = (maxy >= bounds->y) ? maxy + 1 - bounds->y : 0;
  debug(" - rendered tile bounds %d, %d (%d x %d)\n", bounds->x, bounds->y, bounds->w, bounds->h);
}

void draw_polygon(const pp_polygon_t polygon) {

  debug("> draw polygon with %u contours\n", polygon.contour_count);

  if(polygon.contour_count == 0) {
    return;
  }

  // determine extreme bounds
  pp_rect_t polygon_bounds = pp_polygon_bounds(polygon);

  if(_pp_transform) {
    polygon_bounds = pp_rect_transform(polygon_bounds, _pp_transform);
  }

  debug("  - bounds %d, %d (%d x %d)\n", polygon_bounds.x, polygon_bounds.y, polygon_bounds.w, polygon_bounds.h);
  debug("  - clip %d, %d (%d x %d)\n", _pp_clip.x, _pp_clip.y, _pp_clip.w, _pp_clip.h);

#ifdef USE_RP2040_INTERP
  interp_hw_save_t interp1_save;
  interp_save(interp1, &interp1_save);

  interp_config cfg = interp_default_config();
  interp_config_set_clamp(&cfg, true);
  interp_config_set_signed(&cfg, true);
  interp_set_config(interp1, 0, &cfg);
  interp1->base[0] = 0;
#endif

  // iterate over tiles
  debug("  - processing tiles\n");
  for(int32_t y = polygon_bounds.y; y < polygon_bounds.y + polygon_bounds.h; y += tile_bounds.h) {
    for(int32_t x = polygon_bounds.x; x < polygon_bounds.x + polygon_bounds.w; x += tile_bounds.w) {
      pp_tile_t tile = {
        .bounds = pp_rect_intersection((pp_rect_t){.x = x, .y = y, .w = tile_bounds.w, .h = tile_bounds.h}, _pp_clip),
        .stride = (uint32_t)tile_bounds.w,
        .data = tile_buffer
      };
      debug("    : %d, %d (%d x %d)\n", tile.bounds.x, tile.bounds.y, tile.bounds.w, tile.bounds.h);

      // if no intersection then skip tile
      if(pp_rect_empty(tile.bounds)) {
        debug("    : empty when clipped, skipping\n");
        continue;
      }

      // clear existing tile data and nodes
      memset(node_counts, 0, sizeof(node_counts));
      memset(tile.data, 0, tile_buffer_size);

      // build the nodes for each contour
      for(uint32_t i = 0; i < polygon.contour_count; i++) {
        pp_contour_t contour = polygon.contours[i];
        debug("    : build nodes for contour\n");
        build_nodes(contour, tile);
      }

      debug("    : render the tile\n");
      // render the tile
      pp_rect_t bounds;
      render_nodes(tile, &bounds);

      tile.data += bounds.x + tile.stride * bounds.y;
      bounds.x += tile.bounds.x;
      bounds.y += tile.bounds.y;
      debug(" - adjusted render tile bounds %d, %d (%d x %d)\n", bounds.x, bounds.y, bounds.w, bounds.h);
      debug(" - tile bounds %d, %d (%d x %d)\n", tile.bounds.x, tile.bounds.y, tile.bounds.w, tile.bounds.h);
      tile.bounds = pp_rect_intersection(bounds, tile.bounds);
      if (pp_rect_empty(tile.bounds)) {
        continue;
      }

      _pp_tile_callback(tile);
    }
  }

#ifdef USE_RP2040_INTERP
  interp_restore(interp1, &interp1_save);
#endif
}
