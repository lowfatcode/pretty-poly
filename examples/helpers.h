// some common helper functions for the pretty poly examples
#pragma once

#include <stdint.h>

typedef union {
  struct __attribute__((__packed__)) {
    uint8_t a;
    uint8_t b;
    uint8_t g;
    uint8_t r;
  } rgba;
  uint32_t c;
} colour;

__attribute__((always_inline)) uint32_t alpha(uint32_t sa, uint32_t da) {
  return (sa * (da + 1)) >> 8;
}

__attribute__((always_inline)) uint8_t blend_channel(uint8_t s, uint8_t d, uint8_t a) {
  return d + ((a * (s - d) + 127) >> 8);
}

colour blend(colour dest, colour src) {

  uint16_t a = alpha(src.rgba.a, dest.rgba.a);

  if(src.rgba.a == 0) return dest;
  if(src.rgba.a == 255) return src;

  colour result;
  result.rgba.r = blend_channel(src.rgba.r, dest.rgba.r, src.rgba.a);
  result.rgba.g = blend_channel(src.rgba.g, dest.rgba.g, src.rgba.a);
  result.rgba.b = blend_channel(src.rgba.b, dest.rgba.b, src.rgba.a);
  result.rgba.a = dest.rgba.a > src.rgba.a ? dest.rgba.a : src.rgba.a;

  return result;
}

colour create_colour(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (colour){ .rgba.r = r, .rgba.g = g, .rgba.b = b, .rgba.a = a };
}