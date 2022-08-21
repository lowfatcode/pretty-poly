<img width="167" alt="pretty-poly" src="https://user-images.githubusercontent.com/297930/185790543-262a7f97-5de3-4795-b2aa-c52afcbe0f3c.png">

# Pretty Poly - super-sampling polygon renderer for low resource platforms.

## Why?

To generate high quality vector graphics, including text, on relatively low dot pitch displays it is necessary to use anti-aliasing. This applies especially to displays often attached to microcontrollers like LED matrices and small LCD screens.

Microcontrollers generally can't drive very high resolution displays (beyond 320x240 or so) because they lack the high speed peripherals and rarely have enough memory to store such a large framebuffer. 

Anti-aliasing is a method that provides a huge quality boost at these lower resolutions.

> Pretty Poly was initially created for [Pretty Alright Fonts](https://github.com/lowfatcode/pretty-alright-fonts) but it has many other uses so is broken out as a sibling project that can be used in isolation.

Pretty Poly provides an antialiased, pixel format agnostic, complex polygon drawing routine designed specifically for use on microcontrollers.

## Approach

Pretty Poly is a tile-based renderer that calls back into your code with each tile of output so that it can be blended into the framebuffer. This allows it to use minimal memory and focus purely on drawing polygons without worrying about what format the framebuffer is.

Features:

- render complex polygons (concave, self-intersecting, holey, etc.)
- header only library - simply copy the header file into your project
- minimal memory usage - only requires ~2kB for tile buffer and node lists which is statically allocated
- no divide operation used during render
- floating point, fixed point, and integer variants
- 4x and 16x super sampling support (or not antialiasing if preffered)
- automatic winding order detection for outlines and holes
- supply a clip rectangle and all results will be fully clipped meaing you can avoid extra bounds checks
- pixel format agnostic - renders polygons as a "mask" for you to blend into your framebuffer

## Using Pretty Poly

Pretty Poly is a header only C++ library.

Making use of it is as easy as copying `pretty-poly.hpp` into your project and including it in the source 
file where you need to access the methods.

First of all you must call `set_options()` to configure your callback function and any other settings you
wish to use and then you can call `draw_polygon` at any time passing in one or more contours of points.

A basic example might look like:

```c++
void tile_callback(const tile_t &tile) {
  // process the tile image data here - see below for details
}

int main() {

  // defaults to detecting winding order
  set_options(tile_callback, X4);

  // for a single contour we can just pass the points in as an array
  int a_outer[46] = { // outer edge of letter "a" (x1, y1, x2, y2, ...)
    36, 0, 34, -5, 21, 1, 16, 0, 8, -4, 2, -16, 6, -24, 15, -31, 28, -34, 
    34, -34, 34, -37, 27, -44, 21, -38, 4, -38, 4, -41, 11, -47, 21, -51,
    33, -51, 43, -47, 51, -37, 51, -13, 53, -1, 53, 0
  };
  draw_polygon(a_outer, 23);

  // but for shapes with holes we need to pass in multiple contours
  int a_inner[12] = { // inner hole of letter "a" (x1, y1, x2, y2, ...)
    25, -11, 34, -16, 34, -25, 29, -25, 24, -24, 20, -17
  };
  draw_polygon(a_outer, 23, a_inner, 6);

  return 0;
}
```

See the [`set_options`](#setoptionscallback-antialiasing-flags) section for details on what different options are supported.

## Implementing the tile renderer callback

Your tile renderer callback function will be passed a const reference to a
`tile_t` object which contains all of the information needed to blend the
tile with your framebuffer.

```c++
void my_callback(const tile_t &tile) {
  // process the tile image data here
}
```

### The `tile_t` object

The only parameter your callback function will recieve is a `tile_t` object.

|size (bytes)|name|type|notes|
|--:|---|---|---|
|`4`|`x`|unsigned integer|x offset of tile in framebuffer|
|`4`|`y`|unsigned integer|y offset of tile in framebuffer|
|`4`|`w`|unsigned integer|width of tile data|
|`4`|`h`|unsigned integer|height of tile data|
|`4`|`stride`|unsigned integer|width of row in bytes|
|`4`|`data`|void *|pointer to start of tile image data|

> `tile_t` objects will always be clipped against your supplied clip rectangle
> so it is not necessary for you to check bounds again when rendering.

The `w` and `h` properties represent the valid part of the tile data. This is
not necessarily the size of the tiles used by the renderer. For example if you
draw a very small polygon it may only occupy a portion of the tile, or if a 
tile is clipped against your supplied clip rectangle it will only contain 
partial data.

You must only ever process and draw the data in the tile starting at `0, 0` in 
the upper-left corner and ending at `w, h` in the lower-right corner.

`stride` allows you to find the start of each row in the tile data.


### Using `tile_t.get_value()` - the slower, easier, option

Pretty Poly provides a simple way to get the value of a specific coordinate of the tile.


```c++
// Simpler (but slower) example callback
// -------------------------------------
// the `tile` object provides a `get_value()` method which always returns a 
// value between 0..255 - this is slower that reading the tile data directly 
// (since we need a function call per pixel) but can be helpful to get up 
// and running more quickly.
void callback(const tile_t &tile) {
  for(auto y = 0; y < tile.h; y++) {
    for(auto x = 0; x < tile.w; x++) {      
      // call your blend function
      blend(framebuffer, tile.x + x, tile.y + y, tile.get_value(x, y), colour);
    }
  }
}
```

If this is fast enough for your usecase then congratulations! 🥳 You have 
just saved future you from a lot of headaches and debugging.

The other technique, while faster, involves lots of bit twiddly and shifting
and oh my.

### Using `tile_t.data` directly - much faster, but more complicated

With the faster implementation you need to handle the raw tile data (at whatever bit depth you requested super sampling) and walk through the data pixel by pixel extracting the value along the way. This is a lot faster than using the `get_value()` helper function as it avoids making a function call for every pixel.

You can also potentially optimise in other ways:

- read the buffer in larger chunks (32 bits at a time for example)
- check if the next byte, word, or dword is 0 and skip multiple pixels in one go
- equally check if the value is 0xff, 0xffff, etc and write multiple opaque pixels in one go
- scale `value` to better match your framebuffer format
- scale `value` in other ways (not necessarily linear!) to apply effects

```c++
// Example code
// ------------
// if we we're using X4 sampling then our buffer has two bits of data per
// pixel. this is not the fastest way to process the tile image data but 
// it's relatively straightforward to understand.
void callback(const tile_t &tile) {
  static uint8_t alpha[4] {0, 85, 170, 255}; // map sampling results to alpha values
  uint8_t *data = (uint8_t *)tile.data;
  for(auto y = 0; y < tile.h; y++) {
    // get pointer to start of row
    uint8_t *row = &data[tile.stride * y];
    for(auto x = 0; x < tile.w; x++) {
      // work out index of byte in row
      uint byte = x >> 2;
      // work out shift of bits in byte
      uint shift = 6 - (x & 0b11) * 2;
      // value will contain 0..3 (0 = transparent, 1..2 blend levels, 3 = opaque)
      uint value = (row[byte] >> shift) & 0b11;      
      // call your blend function
      blend(framebuffer, tile.x + x, tile.y + y, alpha[value], colour);
    }
  }
}




```

- Include the header only library in your source file `#include "pretty-poly.hpp"`
### `set_options(callback, [antialiasing], [flags])`

`antialias` sets the level of super sampling to be used:
  - `X1` no antialiasing
  - `X4` 4x super sampling (2x2 grid)
  - `X16` 16x super sampling (4x4 grid)

`flags` is one or more of:
  - `WINDING`: detect winding order of polygons (combines with `WINDING_NORMAL` and `WINDING_REVERSE`)
    - Combine with:
    - `WINDING_NORMAL`: clockwise for outlines, anti-clockwise for holes (default)
    - `REVERSE_WINDING`: anti-clockwise for outlines, clockwise for holes
  - `IGNORE_WINDING`: all polygons are treated as outlines


- You call `draw_polygon(points)` passing in the points of the polygon, the antialiasing level, and flags
- Pretty Poly callsb
