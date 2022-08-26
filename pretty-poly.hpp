#include <cstdint>
#include <algorithm>

using namespace std;

#ifdef PP_DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

namespace pretty_poly {

  enum antialias_t {NONE = 0, X4 = 1, X16 = 2};

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

    rect_t merge(const rect_t &c) {
      return rect_t(
        min(this->x, c.x),
        min(this->y, c.y),
        max(this->x + this->w, c.x + c.w) - min(this->x, c.x),
        max(this->y + this->h, c.y + c.h) - min(this->y, c.y)
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
    vector<T> points;
    contour_t() {}
    contour_t(vector<T> points) : points(points) {};

    rect_t bounds() {
      T minx = this->points[0], maxx = minx;
      T miny = this->points[1], maxy = miny;
      for(auto i = 2; i < this->points.size(); i += 2) {
        int cx = this->points[i + 0];
        int cy = this->points[i + 1];
        minx = min(minx, cx); miny = min(miny, cy);
        maxx = max(maxx, cx); maxy = max(maxy, cy);
      }
      return rect_t(minx, miny, maxx - minx, maxy - miny);
    }
  };

  typedef void (*tile_callback_t)(const tile_t &tile);

  // buffer that each tile is rendered into before callback
  constexpr unsigned tile_buffer_size = 1024;
  uint8_t tile_buffer[tile_buffer_size];

  // polygon node buffer handles at most 16 line intersections per scanline
  // is this enough for cjk/emoji? (requires a 2kB buffer)
  constexpr unsigned node_buffer_size = 32;
  int nodes[node_buffer_size][32];
  unsigned node_counts[node_buffer_size];

  // default tile bounds to X1 antialiasing
  rect_t tile_bounds(0, 0, tile_buffer_size / node_buffer_size, node_buffer_size);

  // user settings
  namespace settings {
    rect_t clip(0, 0, 320, 240);
    tile_callback_t callback;
    antialias_t antialias = antialias_t::NONE;
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
    // start with the last point to close the loop
    int lx = contour.points[contour.points.size() - 2] - tile.bounds.x;
    int ly = contour.points[contour.points.size() - 1] - tile.bounds.y;

    for(int i = 0; i < contour.points.size(); i += 2) {
      int x = contour.points[i + 0] - tile.bounds.x;
      int y = contour.points[i + 1] - tile.bounds.y;

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
    rect_t polygon_bounds = contours[0].bounds();
    for(auto &contour : contours) {
      polygon_bounds = polygon_bounds.merge(contour.bounds());
    }

 
    // TODO: apply arbitrary transform of points if requested
    // question:  is this a feature we want to add to pretty poly?


    debug("  - bounds %d, %d (%d x %d)\n", polygon_bounds.x, polygon_bounds.y, polygon_bounds.w, polygon_bounds.h);
    debug("  - clip %d, %d (%d x %d)\n", settings::clip.x, settings::clip.y, settings::clip.w, settings::clip.h);

    // iterate over tiles
    debug("  - processing tiles\n");
    for(auto y = polygon_bounds.y; y < polygon_bounds.y + polygon_bounds.h; y += tile_bounds.h) {
      for(auto x = polygon_bounds.x; x < polygon_bounds.x + polygon_bounds.w; x += tile_bounds.w) {
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
