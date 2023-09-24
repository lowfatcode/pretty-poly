#include "ctype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "pretty-poly.h"
#include "helpers.h"

const int WIDTH = 1024;
const int HEIGHT = 1024;
colour buffer[1024][1024];

colour pen;
void set_pen(colour c) {
  pen = c;
}

void tile_render_callback(const pp_tile_t *t) {
  for(int32_t y = 0; y < t->h; y++) {
    for(int32_t x = 0; x < t->w; x++) {     
      colour alpha_pen = pen;
      alpha_pen.rgba.a = alpha(pen.rgba.a, pp_tile_get_value(t, x, y));
      buffer[y + t->y][x + t->x] = blend(buffer[y + t->y][x + t->x], alpha_pen);
    }
  }
}

// this is a terrible svg path parser, it only supports a tiny subset of
// features, indeed it just barely manages to extract this logo
// perhaps it could be fun to extend this in future - it would
// provide a simple way to import vector artwork into projects
pp_point_t point_on_cubic_bezier(float t, pp_point_t s, pp_point_t cp1, pp_point_t cp2, pp_point_t e) {
  float t2 = t * t;
  float t3 = t * t * t;
  return (pp_point_t) {
    .x = s.x + (-s.x * 3 + (s.x * 3 - s.x * t) * t) * t
          + (cp1.x * 3 + (cp1.x * -6 + cp1.x * 3 * t) * t) * t
          + (cp2.x * 3 - cp2.x * 3 * t) * t2
          + e.x * t3,
    .y = s.y + (-s.y * 3 + (s.y * 3 - s.y * t) * t) * t
          + (cp1.y * 3 + (cp1.y * -6 + cp1.y * 3 * t) * t) * t
          + (cp2.y * 3 - cp2.y * 3 * t) * t2
          + e.y * t3
  };
}

typedef enum {
  NONE, MOVE, MOVE_RELATIVE, CUBIC_BEZIER_RELATIVE
} command_t;

// list of supported svg path commands, can either be upper (absolute) or lower 
// (relative) case:
//   - M = moveto
//   - C = curveto
//   - Z = closepath
//
// not yet supported:
//   - L = lineto
//   - H = horizontal lineto
//   - V = vertical lineto
//   - S = smooth curveto
//   - Q = quadratic Bézier curve
//   - T = smooth quadratic Bézier curveto
//   - A = elliptical Arc


// extract the next token from the path
void get_next_token(char **caret, char *token) {
  // skip any leading whitespace
  while(isspace(**caret)) {
    (*caret)++;
  }
  
  // is the first char a supported command?
  if(strchr("mMcCzZ", **caret)) {
    // copy command as token to return and increment caret
    token[0] = **caret;
    token[1] = '\0';
    (*caret)++;
    return;
  }

  // is the first char an unsupported command?
  if(strchr("lLhHvVsSqQtTaA", **caret)) {
    assert(!"error: SVG path command not supported.");
  }

  // return the next number
  while(true) {
    if(**caret == ' ' || **caret == 'z' || **caret == '\n') {
      *token = '\0';
      break;
    }
    *token++ = **caret;
    (*caret)++;
  }

  return;
}

bool is_numeric(char *token) {
  while(*token != '\0') {
    if(!isdigit(*token) && *token != '-') { return false; }
    token++;
  }
  return true;
}

// cor this code, just don't even look at it
int get_svg_path_point_count(char *path) {
  char **caret = &path;

  command_t command = NONE;

  char token[16];
  int count = 0;
  while(true) {
    get_next_token(caret, token);

    // no more tokens, end of contour
    if(strlen(token) == 0) {
      break;
    }

    // if token is command then execute it and continue
    switch(token[0]) {
      case 'M': { command = MOVE; continue; } break;
      case 'm': { command = MOVE_RELATIVE; continue; } break;
      case 'c': { command = CUBIC_BEZIER_RELATIVE; continue; } break;
      case 'z': { return count; } break;
    }

    // extract control points and end point    
    if(command == CUBIC_BEZIER_RELATIVE) {
      get_next_token(caret, token);
      get_next_token(caret, token);
      get_next_token(caret, token);
      get_next_token(caret, token);
      get_next_token(caret, token);
      count += 4;
    }

    if(command == MOVE) {
      get_next_token(caret, token);
      count += 1;
    }

    if(command == MOVE_RELATIVE) {
      get_next_token(caret, token);
      count += 1;
    }
  }

  return count;
}

int get_svg_path_count(char *caret) {
  int count = 0;
  char *found = strchr(caret, 'z');
  while(found) {
    caret = found + 1;
    count++;
    found = strchr(caret, 'z');
  }
  return count;
}

// parses the contour starting at the caret and returns the points and point
// count
pp_point_t last;
pp_contour_t parse_svg_path_contour(char **caret) {  
  
  pp_contour_t result;
  result.point_count = get_svg_path_point_count(*caret);
  result.points = (pp_point_t *)malloc(sizeof(pp_point_t) * result.point_count);

  command_t command = NONE;

  debug("> start of contour\n");

  char token[16];
  int i = 0;
  while(true) {
    get_next_token(caret, token);

    // no more tokens, end of contour
    if(strlen(token) == 0) {
      break;
    }

    // if token is command then execute it and continue
    switch(token[0]) {
      case 'M': { command = MOVE; continue; } break;
      case 'm': { command = MOVE_RELATIVE; continue; } break;
      case 'c': { command = CUBIC_BEZIER_RELATIVE; continue; } break;
      case 'z': { return result; } break;
    }

    // extract control points and end point    
    if(command == CUBIC_BEZIER_RELATIVE) {
      pp_point_t c1;
      c1.x = atof(token) + last.x;
      get_next_token(caret, token);
      c1.y = atof(token) + last.y;

      pp_point_t c2;
      get_next_token(caret, token);
      c2.x = atof(token) + last.x;
      get_next_token(caret, token);
      c2.y = atof(token) + last.y;

      pp_point_t point;
      get_next_token(caret, token);
      point.x = atof(token) + last.x;
      get_next_token(caret, token);
      point.y = atof(token) + last.y;

      result.points[i++] = point_on_cubic_bezier(0.25, last, c1, c2, point);
      result.points[i++] = point_on_cubic_bezier(0.50, last, c1, c2, point);
      result.points[i++] = point_on_cubic_bezier(0.75, last, c1, c2, point);
      result.points[i++] = point;

      last = point;

      debug("  + %d, %d (bezier)\n", point.x, point.y);
    }

    if(command == MOVE) {
      pp_point_t point;
      point.x = atof(token);
      get_next_token(caret, token);
      point.y = atof(token);
      result.points[i++] = point;
      last = point;

      debug("  + %d, %d\n", point.x, point.y);
    }

    if(command == MOVE_RELATIVE) {
      pp_point_t point;
      point.x = atof(token) + last.x;
      get_next_token(caret, token);
      point.y = atof(token) + last.y;
      result.points[i++] = point;
      last = point;

      debug("  + %d, %d\n", point.x, point.y);
    }

  }

  return result;
}

pp_polygon_t parse_svg_path(char *path) {
  pp_polygon_t result;

  // allocate enough contours to store the path data
  result.contour_count = get_svg_path_count(path);
  result.contours = (pp_contour_t *)malloc(sizeof(pp_contour_t) * result.contour_count);

  // parse each contour
  char *caret = path;
  int i = 0;
  while(caret < path + strlen(path)) {
    result.contours[i] = parse_svg_path_contour(&caret);
    i++;
  }

  return result;
}

int main() {

  // setup pretty poly
  pp_tile_callback(tile_render_callback);
  pp_antialias(PP_AA_X16);
  pp_clip(0, 0, WIDTH, HEIGHT);

  

  /*for(int y = 0; y < HEIGHT; y++) {
    for(int x = 0; x < WIDTH; x++) {
      //buffer[x][y] = ((x / 8) + (y / 8)) % 2 == 0 ? create_colour(16, 32, 48, 255) : create_colour(0, 0, 0, 255);
      buffer[y][x] = create_colour_hsv((float)x / 1024.0f, 1.0f, 1.0f, 1.0f);
      //buffer[x][y] = create_colour(255, 255, 255, 255);
    }
  }*/

  // parse logo svg path into contours
  char logo_svg_path[] = "M3260 3933 c-168 -179 -274 -287 -503 -520 -248 -253 -248 -253 -1442 -253 -657 0 -1195 -3 -1195 -6 0 -9 124 -189 132 -192 5 -2 8 -9 8 -15 0 -6 9 -20 19 -31 11 -12 27 -35 38 -53 10 -18 31 -50 47 -73 29 -41 29 -41 -59 -173 -49 -73 -93 -133 -97 -135 -4 -2 -8 -9 -8 -14 0 -6 -17 -34 -38 -62 -21 -28 -42 -57 -46 -64 -6 -10 154 -12 805 -12 446 0 814 -1 816 -4 2 -2 -9 -23 -25 -47 -34 -51 -104 -188 -122 -239 -7 -19 -16 -44 -20 -55 -53 -128 -67 -261 -69 -641 -1 -117 -4 -164 -13 -171 -7 -6 -31 -13 -53 -16 -22 -3 -47 -8 -56 -12 -19 -7 -32 20 -50 110 -7 33 -13 61 -15 63 -6 8 -85 -51 -115 -86 -83 -97 -98 -161 -80 -347 20 -205 30 -241 83 -294 45 -46 99 -67 205 -80 126 -15 263 -65 396 -145 35 -20 113 -100 158 -161 24 -33 49 -66 56 -72 6 -7 13 -18 15 -24 3 -9 10 -9 27 0 12 7 20 19 17 26 -2 7 1 16 9 18 7 3 28 36 46 74 30 63 32 76 32 168 0 55 -4 111 -10 125 -6 14 -10 27 -9 30 0 3 -12 27 -28 54 -28 48 -28 48 11 90 82 86 150 228 169 351 6 43 17 61 78 130 39 44 73 82 76 85 43 43 192 269 185 280 -3 6 -2 10 4 10 27 0 190 372 210 480 5 30 13 87 17 125 4 39 8 75 9 80 1 6 3 30 3 55 2 45 2 45 734 43 403 -2 729 0 723 5 -5 4 -14 15 -20 24 -5 9 -65 98 -132 197 -68 99 -123 186 -123 192 0 7 6 20 14 28 8 9 69 97 135 196 122 180 122 180 -494 183 -564 2 -640 5 -606 26 4 3 11 23 15 44 3 21 13 61 21 88 8 27 31 108 51 179 20 72 45 162 55 200 11 39 28 102 39 140 10 39 23 87 30 108 6 21 10 43 8 48 -2 6 -32 -20 -68 -58z m-2188 -993 c-3 -149 1 -152 43 -24 14 43 35 98 46 122 20 43 35 50 87 36 19 -6 22 -11 17 -35 -4 -15 -15 -46 -26 -68 -10 -22 -19 -46 -19 -54 0 -7 -4 -17 -9 -23 -5 -5 -16 -29 -24 -54 -15 -45 -15 -45 18 -82 40 -43 51 -98 41 -195 -12 -112 -50 -143 -177 -143 -43 0 -81 5 -84 10 -8 13 -14 476 -7 578 5 73 5 73 51 70 46 -3 46 -3 43 -138z m476 107 c2 -10 1 -29 -2 -43 -6 -22 -11 -24 -70 -24 -63 0 -64 0 -70 -31 -3 -17 -6 -62 -6 -100 0 -69 0 -69 56 -69 55 0 55 0 52 -42 -3 -43 -3 -43 -55 -46 -53 -3 -53 -3 -53 -93 0 -89 0 -89 70 -89 70 0 70 0 70 -39 0 -48 -4 -50 -127 -50 -69 0 -94 4 -104 15 -9 11 -10 51 -5 162 4 81 7 187 6 236 -2 135 9 234 27 238 8 2 59 1 112 -2 83 -4 96 -8 99 -23z m820 21 c8 -8 12 -53 12 -132 1 -104 12 -189 33 -246 4 -8 8 -28 11 -45 15 -90 19 -111 26 -120 5 -5 10 -30 12 -55 3 -44 3 -45 -30 -48 -44 -4 -59 10 -67 61 -3 23 -10 60 -15 82 -5 22 -12 62 -15 89 -8 59 -21 50 -34 -24 -14 -80 -39 -189 -46 -200 -8 -14 -58 -13 -72 1 -12 12 -9 56 7 94 4 11 15 52 25 90 9 39 26 106 37 150 13 48 23 122 25 185 2 58 6 111 8 118 6 15 67 16 83 0z m869 -32 c43 -46 47 -85 36 -334 -8 -192 -11 -211 -31 -240 -43 -59 -157 -65 -212 -11 -37 37 -43 100 -36 357 6 182 7 195 28 222 30 36 71 50 133 45 41 -4 56 -10 82 -39z m485 -86 c3 -75 15 -159 28 -210 12 -47 26 -105 31 -130 5 -25 14 -65 20 -90 20 -85 18 -95 -19 -98 -41 -4 -62 12 -62 48 0 15 -6 45 -13 66 -7 22 -13 44 -13 49 0 6 -4 39 -9 75 -8 65 -8 65 -25 -5 -10 -38 -23 -101 -31 -140 -7 -38 -19 -76 -25 -82 -16 -17 -59 -17 -72 0 -11 13 -8 31 48 242 23 85 34 156 40 245 10 156 12 162 59 158 36 -3 36 -3 43 -128z m-2979 78 c3 -24 4 -82 3 -129 -3 -87 -3 -87 43 -92 106 -13 134 -53 135 -195 0 -93 -16 -142 -54 -162 -28 -15 -193 -30 -205 -19 -6 6 -11 132 -13 324 -4 315 -4 315 41 315 45 0 45 0 50 -42z m1007 -238 c0 -280 0 -280 46 -280 45 0 45 0 42 -42 -3 -43 -3 -43 -121 -46 -140 -3 -157 3 -157 58 0 40 0 40 45 40 45 0 45 0 46 68 1 37 3 123 5 192 2 69 3 162 4 208 0 82 0 82 45 82 45 0 45 0 45 -280z m312 3 c3 -278 3 -278 46 -281 43 -3 43 -3 40 -45 -3 -42 -3 -42 -118 -45 -134 -3 -170 7 -170 47 0 37 17 51 62 51 43 0 40 -26 39 305 -1 77 2 164 5 193 6 52 6 52 50 52 44 0 44 0 46 -277z m698 148 c0 -128 0 -128 58 -134 50 -5 61 -10 89 -42 24 -28 33 -50 39 -92 16 -130 -14 -214 -84 -232 -40 -11 -168 -18 -175 -11 -8 8 -18 626 -11 633 4 4 25 7 46 7 38 0 38 0 38 -129z m815 84 c0 -40 0 -40 -66 -43 -66 -3 -66 -3 -72 -160 -3 -86 -6 -208 -7 -271 0 -63 -3 -118 -6 -123 -3 -4 -23 -8 -45 -8 -39 0 -39 0 -39 325 0 326 0 326 118 323 117 -3 117 -3 117 -43z";
  pp_polygon_t polygon = parse_svg_path(logo_svg_path);

  for(int i = 240; i <= 360; i += 10) {
    // determine extreme bounds and scaling factor to fit on canvas
    pp_rect_t bounds = pp_polygon_bounds(&polygon);
    float scale = ((float)WIDTH / (float)bounds.w) * (i / 400.0f);
    pp_mat3_t t = pp_mat3_identity();
    pp_mat3_translate(&t, 512, 512);
    pp_mat3_scale(&t, scale, scale);
    pp_mat3_rotate(&t, i);
    pp_mat3_translate(&t, -bounds.w / 2, -bounds.h / 2);

    if(i == 360) {
      pp_mat3_t st = t;      
      pp_mat3_translate(&st, 90, 90);
      pp_transform(&st);
      set_pen(create_colour(20, 30, 40, 160));
      draw_polygon(&polygon);
      
      set_pen(create_colour(234, 178, 163, 255));
    }else{
      float hue = (float)(i - 240) / 120.0f;
      float alpha = (((float)(i - 240) / 120.0f) / 2.0f) + 0.5f;
      set_pen(create_colour_hsv(hue, 1.0f, 1.0f, alpha));
    }    

    pp_transform(&t);
    draw_polygon(&polygon);
  }


  stbi_write_png("/tmp/out.png", WIDTH, HEIGHT, 4, (void *)buffer, WIDTH * sizeof(uint32_t));
  
  return 0;
}