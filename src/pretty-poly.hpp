#include <cstdint>
#include <algorithm>

using namespace std;

#define PP_DEBUG

#ifdef PP_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

namespace pretty_poly {

  enum antialias_t {
    X1      = 0,
    X2      = 1,
    X4      = 2
  };

  struct rect_t {
    int x, y;
    int w, h;    

    rect_t() :
      x(0), y(0), w(0), h(0) {}

    rect_t(int x, int y, int w, int h) :
      x(x), y(y), w(w), h(h) {}

    rect_t intersection(const rect_t &compare) {
      return rect_t(
        max(this->x, compare.x),
        max(this->y, compare.y),
        max(0, min(int(this->x + this->w), int(compare.x + compare.w)) - max(this->x, compare.x)),
        max(0, min(int(this->y + this->h), int(compare.y + compare.h)) - max(this->y, compare.y))
      );
    }
  };

  struct tile_t {
    rect_t bounds;
    unsigned stride;
    void *data;

    tile_t() {};

    int get_value(int x, int y) const {
      uint8_t *p = static_cast<uint8_t*>(this->data) + x + y * this->stride;
      return (int)*p;
      // uint32_t *row = (uint32_t *)(&this->data) + ((this->stride / 4) * y);
      // int dword = x >> 5;
      // int shift = 31 - (x & 0b11111);
      // return (row[dword] >> shift) & 0b1;
    }
  };

  template<typename T>
  struct contour_t {
    T *points;
    unsigned count;

    contour_t(T *points, unsigned count) : points(points), count(count) {};
  };

  typedef void (*tile_callback_t)(const tile_t &tile);

  tile_callback_t tile_callback;
  constexpr uint32_t tile_size = 32;
  rect_t clip(0, 0, 320, 240);
  antialias_t antialias = X4;
  
  void set_options(tile_callback_t callback) {
    tile_callback = callback;
  }

  // dy step (returns 1, 0, or -1 if the supplied value is > 0, == 0, < 0)
  __attribute__((always_inline)) int sign(int v) {
    // assumes 32-bit int/unsigned
    return ((unsigned)-v >> 31) - ((unsigned)v >> 31);
  }

  // write out the tile bits
  void debug_tile(const tile_t &tile) {
    debug("  - tile %d, %d (%d x %d)\n", tile.bounds.x, tile.bounds.y, tile.bounds.w, tile.bounds.h);
    for(auto y = 0; y < tile.bounds.h; y++) {
      uint32_t *row = (uint32_t *)(&tile.data) + ((tile.stride / 4) * y);
      for(auto x = 0; x < tile.bounds.w; x++) {
        debug("%d", tile.get_value(x, y));
      }  
      debug("\n");              
    }
    debug("-----------------------\n");
  }

  // polygon node buffer handles at most 32 line intersections per scanline
  // is this enough for cjk/emoji? (requires a 4kB buffer)
  static int nodes[tile_size][32];
  static unsigned node_counts[tile_size];

  void add_line_segment_to_nodes(int sx, int sy, int ex, int ey) {
    // swap endpoints if line "pointing up", we do this because we
    // alway skip the last scanline (so that polygons can but cleanly
    // up against each other without overlap)
    if(ey < sy) {
      swap(sy, ey);
      swap(sx, ex);
    }

    int x = sx;
    int y = sy;
    int e = 0;

    int xinc = sign(ex - sx);
    int einc = abs(ex - sx) + 1;

    // todo: preclamp sy and ey (and therefore count) no need to perform
    // that test inside the loop
    int dy = ey - sy;
    int count = dy;
    debug("      + line segment from %d, %d to %d, %d\n", sx, sy, ex, ey);
    // loop over scanlines
    while(count--) {
      // consume accumulated error
      while(e > dy) {e -= dy; x += xinc;}

      if(y >= 0 && y < tile_size) {  
        // clamp node x value to tile bounds
        int nx = max(min(x, (int)tile_size), 0);        
        debug("      + adding node at %d, %d\n", x, y);
        // add node to node list
        nodes[y][node_counts[y]++] = nx;
      }

      // step to next scanline and accumulate error
      y++;
      e += einc;
    }
  }

  template<typename C>
  void build_nodes(const contour_t<C> &contour, const tile_t &tile) {
    // note: we need to super sample on the y-axis but can we use the fractional
    // coordinate for the x value?
    C *points = contour.points;
    int count = contour.count;

    // start with the last point to close the loop
    int lx = (points[(count - 1) * 2 + 0] << antialias) - tile.bounds.x;
    int ly = (points[(count - 1) * 2 + 1] << antialias) - tile.bounds.y;

    while(count--) {
      int x = ((*points++) << antialias) - tile.bounds.x;
      int y = ((*points++) << antialias) - tile.bounds.y;

      add_line_segment_to_nodes(lx, ly, x, y);
      
      lx = x; ly = y;
    };
  }

  void render_tile(const tile_t &tile) {
    // clear the tile data
    memset(tile.data, 0, tile.stride * tile.bounds.h);

    for(auto y = 0; y < tile.bounds.h; y++) {
      if(node_counts[y] == 0) {
        continue;
      }

      std::sort(&nodes[y][0], &nodes[y][0] + node_counts[y]);

      for(auto i = 0; i < node_counts[y]; i += 2) {
        int sx = nodes[y][i + 0];
        int ex = nodes[y][i + 1];

        if(sx == ex) {
          continue;
        }

        debug(" - render span at %d from %d to %d\n", y, sx, ex);

        uint8_t *p = static_cast<uint8_t*>(tile.data) + sx + y * tile.stride;
        int count = ex - sx;
        while(count--) {
          *p++ = 1;
        }       
      }
    }
  }

  template<typename T>
  void draw_polygon(T *points, unsigned count) {
    std::vector<contour_t<T>> contours;
    contour_t<T> c(points, count);
    contours.push_back(c);
    draw_polygon(contours);
  }
  
  template<typename T>
  void draw_polygon(std::vector<contour_t<T>> contours) {    
    static uint8_t tile_buffer[tile_size][tile_size];

    debug("> draw polygon with %lu contours\n", contours.size());

    // determine extreme bounds
    T minx = contours[0].points[0], maxx = minx;
    T miny = contours[0].points[1], maxy = miny;
  
    // TODO: apply arbitrary transform of points if requested
    // question:  is this a feature we want to add?

    // find min/max bounds of polygon
    for(auto &contour : contours) {
      for(auto i = 2; i < contour.count * 2; i += 2) {
        int cx = contour.points[i];
        int cy = contour.points[i + 1];
        minx = min(minx, cx); miny = min(miny, cy);
        maxx = max(maxx, cx); maxy = max(maxy, cy);
      }
    }

    // scale bounds for supersampling
    minx <<= antialias; maxx <<= antialias;
    miny <<= antialias; maxy <<= antialias;

    debug("  - bounds %d, %d -> %d, %d\n", minx, miny, maxx, maxy);
    debug("  - clip %d, %d (%d x %d)\n", clip.x, clip.y, clip.w, clip.h);

    // iterate over tiles
    debug("  - processing tiles\n");
    for(auto y = miny; y < maxy; y += tile_size) {
      for(auto x = minx; x < maxx; x += tile_size) {
        tile_t tile;
        // get intersection of tile with clip rect
        tile.bounds = rect_t(x, y, tile_size, tile_size).intersection(rect_t(clip.x << antialias, clip.y << antialias, clip.w << antialias, clip.h << antialias));
        debug("    : %d, %d (%d x %d)\n", tile.bounds.x, tile.bounds.y, tile.bounds.w, tile.bounds.h);
        // if no intersection then skip tile
        if(tile.bounds.w == 0 || tile.bounds.h == 0) {
          debug("    : empty when clipped, skipping\n");
          continue;
        }

        // set stride of tile in bytes
        tile.stride = tile_size;
        tile.data = (void *)tile_buffer;

        // build the nodes for each contour
        memset(node_counts, 0, sizeof(node_counts));

        for(auto &contour : contours) {
          debug("    : build nodes for contour\n");
          build_nodes(contour, tile);
        }

        debug("    : render the tile\n");
        // render the tile
        render_tile(tile);

        tile_callback(tile);
      }
    }
  }
}
