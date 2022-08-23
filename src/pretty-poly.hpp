#include <cstdint>
#include <algorithm>

using namespace std;

#ifdef PP_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

namespace pretty_poly {

  enum antialias_t {X1 = 0, X2 = 1, X4 = 2};

  struct rect_t {
    int x, y, w, h;    

    rect_t() : x(0), y(0), w(0), h(0) {}
    rect_t(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}

    bool empty() const {return this->w == 0 && this->h == 0;}

    rect_t intersection(const rect_t &c) {
      return rect_t(
        max(this->x, c.x), max(this->y, c.y),
        max(0, min(int(this->x + this->w), int(c.x + c.w)) - max(this->x, c.x)),
        max(0, min(int(this->y + this->h), int(c.y + c.h)) - max(this->y, c.y))
      );
    }
  };

  struct tile_t {
    rect_t bounds;
    unsigned stride;
    uint8_t *data;

    tile_t() {};

    int get_value(int x, int y) const {
      return this->data[x + y * this->stride];
    }
  };

  template<typename T> struct contour_t {
    T *points;
    unsigned count;
    contour_t(T *points, unsigned count) : points(points), count(count) {};
  };

  typedef void (*tile_callback_t)(const tile_t &tile);

  // buffer that each tile is rendered into before callback
  constexpr unsigned tile_buffer_size = 1024;
  uint8_t tile_buffer[tile_buffer_size];

  // polygon node buffer handles at most 16 line intersections per scanline
  // is this enough for cjk/emoji? (requires a 2kB buffer)
  constexpr unsigned node_buffer_size = 32;
  int nodes[node_buffer_size][16];
  unsigned node_counts[node_buffer_size];

  // default tile bounds to X1 antialiasing
  rect_t tile_bounds(0, 0, tile_buffer_size / node_buffer_size, node_buffer_size);

  // user settings
  namespace settings {
    rect_t clip(0, 0, 320, 240);
    tile_callback_t callback;
    antialias_t antialias = X1;
  }  

  void set_options(tile_callback_t callback, antialias_t antialias, rect_t clip) {
    settings::callback = callback;
    settings::antialias = antialias;
    settings::clip = clip;

    // recalculate the tile size for rendering based on antialiasing level
    int tile_height = node_buffer_size >> antialias;
    tile_bounds = rect_t(0, 0, tile_buffer_size / tile_height, tile_height);
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


  void add_line_segment_to_nodes(int sx, int sy, int ex, int ey) {
    // swap endpoints if line "pointing up", we do this because we
    // alway skip the last scanline (so that polygons can but cleanly
    // up against each other without overlap)
    if(ey < sy) {
      swap(sy, ey);
      swap(sx, ex);
    }

    sx <<= settings::antialias;
    ex <<= settings::antialias;
    sy <<= settings::antialias;
    ey <<= settings::antialias;

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

      if(y >= 0 && y < node_buffer_size) {  
        // clamp node x value to tile bounds
        int nx = max(min(x, (int)(tile_bounds.w << settings::antialias)), 0);        
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
    int lx = points[(count - 1) * 2 + 0] - tile.bounds.x;
    int ly = points[(count - 1) * 2 + 1] - tile.bounds.y;

    while(count--) {
      int x = (*points++) - tile.bounds.x;
      int y = (*points++) - tile.bounds.y;

      add_line_segment_to_nodes(lx, ly, x, y);
      
      lx = x; ly = y;
    };
  }

  void render_nodes(const tile_t &tile) {
    for(auto y = 0; y < node_buffer_size; y++) {
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

        for(int x = sx; x < ex; x++) {
          tile.data[(x >> settings::antialias) + (y >> settings::antialias) * tile.stride]++;
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

    debug("> draw polygon with %lu contours\n", contours.size());

    // determine extreme bounds
    T minx = contours[0].points[0], maxx = minx;
    T miny = contours[0].points[1], maxy = miny;
  
    // TODO: apply arbitrary transform of points if requested
    // question:  is this a feature we want to add to pretty poly?

    // find min/max bounds of polygon
    for(auto &contour : contours) {
      for(auto i = 2; i < contour.count * 2; i += 2) {
        int cx = contour.points[i];
        int cy = contour.points[i + 1];
        minx = min(minx, cx); miny = min(miny, cy);
        maxx = max(maxx, cx); maxy = max(maxy, cy);
      }
    }

    debug("  - bounds %d, %d -> %d, %d\n", minx, miny, maxx, maxy);
    debug("  - clip %d, %d (%d x %d)\n", settings::clip.x, settings::clip.y, settings::clip.w, settings::clip.h);

    // iterate over tiles
    debug("  - processing tiles\n");
    for(auto y = miny; y < maxy; y += tile_bounds.h) {
      for(auto x = minx; x < maxx; x += tile_bounds.w) {
        tile_t tile;
        tile.bounds = rect_t(x, y, tile_bounds.w, tile_bounds.h).intersection(settings::clip);
        tile.stride = tile_bounds.w;
        tile.data = tile_buffer;
        debug("    : %d, %d (%d x %d)\n", tile.bounds.x, tile.bounds.y, tile.bounds.w, tile.bounds.h);

        // if no intersection then skip tile
        if(tile.bounds.empty()) {
          debug("    : empty when clipped, skipping\n");
          continue;
        }

        // clear existing tile data and nodes
        memset(node_counts, 0, sizeof(node_counts));
        memset(tile.data, 0, tile_buffer_size);

        // build the nodes for each contour
        for(auto &contour : contours) {
          debug("    : build nodes for contour\n");
          build_nodes(contour, tile);
        }

        debug("    : render the tile\n");
        // render the tile
        render_nodes(tile);

        settings::callback(tile);
      }
    }
  }
}
