#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <vector>
#include <map>
#include <charconv>
#include <string_view>

// #define PP_DEBUG

#include "pretty-poly.hpp"

using namespace std;
using namespace pretty_poly;

constexpr uint32_t WIDTH = 256;
constexpr uint32_t HEIGHT = 256;
uint32_t buf[WIDTH][HEIGHT];

void callback(const tile_t &tile) {
  debug_tile(tile);
  
  for(auto y = 0; y < tile.bounds.h; y++) {
    for(auto x = 0; x < tile.bounds.w; x++) {     
      uint8_t alpha = tile.get_value(x, y) * (255 >> settings::antialias >> settings::antialias); 
      uint8_t r = (234 * alpha) / 255;
      uint8_t g = (78 * alpha) / 255;
      uint8_t b = (63 * alpha) / 255;
      buf[y + tile.bounds.y][x + tile.bounds.x] = r << 24 | g << 16 | b << 8 | 0xff;
    }
  }
}

// this is a terrible svg path parser, it only supports a tiny subset of
// features, indeed it just barely manages to extract this logo
// perhaps it could be fun to extend this in future - it would
// provide a simple way to import vector artwork into projects

enum command_t {
  NONE, MOVE, MOVE_RELATIVE, CUBIC_BEZIER_RELATIVE
};

// supported path commands
map<char, command_t> command_map = {
  {'M', MOVE},
  {'m', MOVE_RELATIVE},
  {'c', CUBIC_BEZIER_RELATIVE}
};

// determine if this token has a command and if so return it
command_t check_for_command(string_view token) {
  char c = token[0];
  if(command_map.count(c)) {
    return command_map[c];
  }
  return command_t::NONE;
}

// return true if this token ends with a path close command
bool check_for_close_path(string_view token) {
  return token[token.size() - 1] == 'z';
}

// extract the next token from the path
string_view get_next_token(string_view &path) {
  size_t split_at = path.find(" ");
  if(split_at == string_view::npos) {
    // no more tokens to parse, get outta here
    return "";
  }  
  string_view token = path.substr(0, split_at);
  path = path.substr(token.size() + 1);  
  return token;
}

int token_to_int(string_view t) {
  int r; from_chars(t.data(), t.data() + t.size(), r); return r;
};

vector<contour_t<int>> parse_svg_path(string_view path) {
  command_t command = command_t::NONE;

  vector<contour_t<int>> contours;
  contours.resize(1);

  int lx, ly; // last x and y
  while(path.size() > 0) {
    string_view x_token = get_next_token(path);
    if(x_token.size() == 0) {
      break;
    }

    if(check_for_command(x_token) != command_t::NONE) {
      // new path command
      printf("change command to %d\n", command);
      command = check_for_command(x_token);
      x_token = x_token.substr(1); // chop the processed command off
    }

    string_view y_token = get_next_token(path);

    // extract control points and end point    
    int c1x, c1y, c2x, c2y;
    if(command == CUBIC_BEZIER_RELATIVE) {
      c1x = token_to_int(x_token) + lx;
      c1y = token_to_int(y_token) + ly;
      c2x = token_to_int(get_next_token(path)) + lx;
      c2y = token_to_int(get_next_token(path)) + ly;
      x_token = get_next_token(path);
      y_token = get_next_token(path);
    }

    bool close_path = check_for_close_path(y_token);
    if(close_path) {
      y_token = y_token.substr(0, y_token.size() - 1);
    }

    int x = token_to_int(x_token);
    int y = token_to_int(y_token);

    if(command == MOVE) {
      contours[contours.size() - 1].points.push_back(x);
      contours[contours.size() - 1].points.push_back(y);
    }

    if(command == MOVE_RELATIVE) {
      x += lx;
      y += ly;
      contours[contours.size() - 1].points.push_back(x);
      contours[contours.size() - 1].points.push_back(y);
    }

    if(command == CUBIC_BEZIER_RELATIVE) {
      x += lx;
      y += ly;

      int mpx = (lx + x + c1x + c2x) / 4;
      int mpy = (ly + y + c1y + c2y) / 4;
      contours[contours.size() - 1].points.push_back(mpx);
      contours[contours.size() - 1].points.push_back(mpy);      

      contours[contours.size() - 1].points.push_back(x);
      contours[contours.size() - 1].points.push_back(y);      
    }

    printf("+ %d, %d\n", x, y);

    if(close_path) {
      printf("close path\n");
      contours.resize(contours.size() + 1);
    }

    lx = x;
    ly = y;
  }

  return contours;
}

int main() {

  // setup polygon renderer  
  set_options(callback, X16, {0, 0, WIDTH, HEIGHT});
  
  // parse logo svg path into contours
  string_view logo_path{"M3260 3933 c-168 -179 -274 -287 -503 -520 -248 -253 -248 -253 -1442 -253 -657 0 -1195 -3 -1195 -6 0 -9 124 -189 132 -192 5 -2 8 -9 8 -15 0 -6 9 -20 19 -31 11 -12 27 -35 38 -53 10 -18 31 -50 47 -73 29 -41 29 -41 -59 -173 -49 -73 -93 -133 -97 -135 -4 -2 -8 -9 -8 -14 0 -6 -17 -34 -38 -62 -21 -28 -42 -57 -46 -64 -6 -10 154 -12 805 -12 446 0 814 -1 816 -4 2 -2 -9 -23 -25 -47 -34 -51 -104 -188 -122 -239 -7 -19 -16 -44 -20 -55 -53 -128 -67 -261 -69 -641 -1 -117 -4 -164 -13 -171 -7 -6 -31 -13 -53 -16 -22 -3 -47 -8 -56 -12 -19 -7 -32 20 -50 110 -7 33 -13 61 -15 63 -6 8 -85 -51 -115 -86 -83 -97 -98 -161 -80 -347 20 -205 30 -241 83 -294 45 -46 99 -67 205 -80 126 -15 263 -65 396 -145 35 -20 113 -100 158 -161 24 -33 49 -66 56 -72 6 -7 13 -18 15 -24 3 -9 10 -9 27 0 12 7 20 19 17 26 -2 7 1 16 9 18 7 3 28 36 46 74 30 63 32 76 32 168 0 55 -4 111 -10 125 -6 14 -10 27 -9 30 0 3 -12 27 -28 54 -28 48 -28 48 11 90 82 86 150 228 169 351 6 43 17 61 78 130 39 44 73 82 76 85 43 43 192 269 185 280 -3 6 -2 10 4 10 27 0 190 372 210 480 5 30 13 87 17 125 4 39 8 75 9 80 1 6 3 30 3 55 2 45 2 45 734 43 403 -2 729 0 723 5 -5 4 -14 15 -20 24 -5 9 -65 98 -132 197 -68 99 -123 186 -123 192 0 7 6 20 14 28 8 9 69 97 135 196 122 180 122 180 -494 183 -564 2 -640 5 -606 26 4 3 11 23 15 44 3 21 13 61 21 88 8 27 31 108 51 179 20 72 45 162 55 200 11 39 28 102 39 140 10 39 23 87 30 108 6 21 10 43 8 48 -2 6 -32 -20 -68 -58z m-2188 -993 c-3 -149 1 -152 43 -24 14 43 35 98 46 122 20 43 35 50 87 36 19 -6 22 -11 17 -35 -4 -15 -15 -46 -26 -68 -10 -22 -19 -46 -19 -54 0 -7 -4 -17 -9 -23 -5 -5 -16 -29 -24 -54 -15 -45 -15 -45 18 -82 40 -43 51 -98 41 -195 -12 -112 -50 -143 -177 -143 -43 0 -81 5 -84 10 -8 13 -14 476 -7 578 5 73 5 73 51 70 46 -3 46 -3 43 -138z m476 107 c2 -10 1 -29 -2 -43 -6 -22 -11 -24 -70 -24 -63 0 -64 0 -70 -31 -3 -17 -6 -62 -6 -100 0 -69 0 -69 56 -69 55 0 55 0 52 -42 -3 -43 -3 -43 -55 -46 -53 -3 -53 -3 -53 -93 0 -89 0 -89 70 -89 70 0 70 0 70 -39 0 -48 -4 -50 -127 -50 -69 0 -94 4 -104 15 -9 11 -10 51 -5 162 4 81 7 187 6 236 -2 135 9 234 27 238 8 2 59 1 112 -2 83 -4 96 -8 99 -23z m820 21 c8 -8 12 -53 12 -132 1 -104 12 -189 33 -246 4 -8 8 -28 11 -45 15 -90 19 -111 26 -120 5 -5 10 -30 12 -55 3 -44 3 -45 -30 -48 -44 -4 -59 10 -67 61 -3 23 -10 60 -15 82 -5 22 -12 62 -15 89 -8 59 -21 50 -34 -24 -14 -80 -39 -189 -46 -200 -8 -14 -58 -13 -72 1 -12 12 -9 56 7 94 4 11 15 52 25 90 9 39 26 106 37 150 13 48 23 122 25 185 2 58 6 111 8 118 6 15 67 16 83 0z m869 -32 c43 -46 47 -85 36 -334 -8 -192 -11 -211 -31 -240 -43 -59 -157 -65 -212 -11 -37 37 -43 100 -36 357 6 182 7 195 28 222 30 36 71 50 133 45 41 -4 56 -10 82 -39z m485 -86 c3 -75 15 -159 28 -210 12 -47 26 -105 31 -130 5 -25 14 -65 20 -90 20 -85 18 -95 -19 -98 -41 -4 -62 12 -62 48 0 15 -6 45 -13 66 -7 22 -13 44 -13 49 0 6 -4 39 -9 75 -8 65 -8 65 -25 -5 -10 -38 -23 -101 -31 -140 -7 -38 -19 -76 -25 -82 -16 -17 -59 -17 -72 0 -11 13 -8 31 48 242 23 85 34 156 40 245 10 156 12 162 59 158 36 -3 36 -3 43 -128z m-2979 78 c3 -24 4 -82 3 -129 -3 -87 -3 -87 43 -92 106 -13 134 -53 135 -195 0 -93 -16 -142 -54 -162 -28 -15 -193 -30 -205 -19 -6 6 -11 132 -13 324 -4 315 -4 315 41 315 45 0 45 0 50 -42z m1007 -238 c0 -280 0 -280 46 -280 45 0 45 0 42 -42 -3 -43 -3 -43 -121 -46 -140 -3 -157 3 -157 58 0 40 0 40 45 40 45 0 45 0 46 68 1 37 3 123 5 192 2 69 3 162 4 208 0 82 0 82 45 82 45 0 45 0 45 -280z m312 3 c3 -278 3 -278 46 -281 43 -3 43 -3 40 -45 -3 -42 -3 -42 -118 -45 -134 -3 -170 7 -170 47 0 37 17 51 62 51 43 0 40 -26 39 305 -1 77 2 164 5 193 6 52 6 52 50 52 44 0 44 0 46 -277z m698 148 c0 -128 0 -128 58 -134 50 -5 61 -10 89 -42 24 -28 33 -50 39 -92 16 -130 -14 -214 -84 -232 -40 -11 -168 -18 -175 -11 -8 8 -18 626 -11 633 4 4 25 7 46 7 38 0 38 0 38 -129z m815 84 c0 -40 0 -40 -66 -43 -66 -3 -66 -3 -72 -160 -3 -86 -6 -208 -7 -271 0 -63 -3 -118 -6 -123 -3 -4 -23 -8 -45 -8 -39 0 -39 0 -39 325 0 326 0 326 118 323 117 -3 117 -3 117 -43z"};
  vector<contour_t<int>> contours = parse_svg_path(logo_path);

  // determine extreme bounds
  rect_t bounds = contours[0].bounds();
  for(auto &contour : contours) {
    bounds = bounds.merge(contour.bounds());
  }

  // scale contours to same size as canvas
  int scale = bounds.w > bounds.h ? bounds.w : bounds.h;  
  for(auto &contour : contours) {
    for(auto i = 0; i < contour.points.size(); i += 2) {
      contour.points[i + 0] = ((contour.points[i + 0] - bounds.x) * WIDTH) / scale;
      contour.points[i + 1] = ((contour.points[i + 1] - bounds.y) * WIDTH) / scale;
    }
  }

  draw_polygon(contours);

  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buf, WIDTH * sizeof(uint32_t));
  
  return 0;
}