#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <vector>
using namespace std;

#include "pretty-poly.hpp"
using namespace pretty_poly;

constexpr uint32_t WIDTH = 128;
constexpr uint32_t HEIGHT = 128;
uint8_t buf[WIDTH][HEIGHT];

void callback(const tile_t &tile) {
  debug_tile(tile);
  for(auto y = 0; y < tile.bounds.h; y+=4) {
    for(auto x = 0; x < tile.bounds.w; x+=4) {      
      // call your blend function
      //blend(framebuffer, tile.x + x, tile.y + y, tile.get_value(x, y), colour);
      uint8_t shade = 0;
      if(tile.get_value(x, y) == 1) {
        shade = 100;
      }
      if(tile.get_value(x, y) == 2) {
        shade = 200;
      }

      int v = 0;
      for(int sx = 0; sx < 4; sx++) {
        for(int sy = 0; sy < 4; sy++) {
          v += tile.get_value(x + sx, y + sy) * 255;
        }
      }
    
      v >>= 4;

      buf[(y + tile.bounds.y) / 4][(x + tile.bounds.x) / 4] = v;
    }
  }
}

int main() {
  //
  set_options(callback);
  
  // X4 (2x2) supersampling - defaults to detecting winding order
  // set_options(tile_callback, X4);
  clip = rect_t(0, 0, WIDTH, HEIGHT);
/*
  int triangle[6] = {
    5, 5, 10, 8, 7, 12
  };
  draw_polygon(triangle, 3);*/

  // for a single contour we can just pass the points in as an array
  int a_outer[46] = { // outer edge of letter "a" (x1, y1, x2, y2, ...)
    36, 51, 34, 46, 21, 52, 16, 51, 8, 47, 2, 35, 6, 27, 15, 20, 28, 17, 34, 17, 34, 14, 27, 7, 21, 13, 4, 13, 4, 10, 11, 4, 21, 0, 33, 0, 43, 4, 51, 14, 51, 38, 53, 50, 53, 51
  };

  // but for shapes with holes we need to pass in multiple contours
  int a_inner[12] = { // inner hole of letter "a" (x1, y1, x2, y2, ...)
    25, 40, 34, 35, 34, 26, 29, 26, 24, 27, 20, 34    
  };
/*
  for(int i = 0; i < 46; i++) {
    a_outer[i] /= 6;
  }
  for(int i = 0; i < 12; i++) {
    a_inner[i] /= 6;
  }*/

  contour_t<int> c1(a_outer, 23);
  contour_t<int> c2(a_inner, 6);
  vector<contour_t<int>> contours = {c1, c2};

  draw_polygon(contours);

  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 1, (void *)buf, WIDTH);
  
  return 0;
}