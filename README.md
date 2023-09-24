<img src="logo.png" alt="Kiwi standing on oval" width="300px">

# Pretty Poly - A super-sampling complex polygon renderer for low resource platforms. 🦜

- [Why?](#why)
- [Approach](#approach)
- [Using Pretty Poly](#using-pretty-poly)
  - [Primitive shapes](#primitive-shapes)
  - [Defining your polygons](#defining-your-polygons)
  - [Clipping](#clipping)
  - [Antialiasing](#antialiasing)
  - [Transformations](#transformations)
  - [Rendering](#rendering)
  - [Implementing the tile renderer callback](#implementing-the-tile-renderer-callback)
- [Types](#types)
  - [`tile_callback_t`](#tile_callback_t)
  - [`pp_tile_t`](#pp_tile_t)
  - [`rect_t`](#rect_t)
  - [`pp_antialias_t`](#pp_antialias_t)
- [Performance considerations](#performance-considerations)
  - [CPU speed](#cpu-speed)
  - [Antialiasing](#antialiasing-1)
  - [Coordinate type](#coordinate-type)
  - [Tile size](#tile-size)
- [Memory usage](#memory-usage)


## Why?

Microcontrollers generally can't drive very high resolution displays (beyond 
320x240 or so) because they lack the high speed peripherals and rarely have 
enough memory to store such a large framebuffer. Super-samplaing (AKA 
anti-aliasing) is a tool that can provide a huge quality boost at these lower 
display pitches.

> Fun fact: The Pretty Poly logo above is rendered by Pretty Poly! It is a 
> single polygon with eleven contours: the outline and ten holes making up the 
> lettering.

Microcontrollers are now powerful enough to perform the extra processing needed 
for super-sampling in realtime allowing high quality vector graphics, including 
text, on relatively low dot pitch displays like LED matrices and small LCD 
screens.

> Pretty Poly was originally part of  
> [Alright Fonts](https://github.com/lowfatcode/alright-fonts) but has many 
> other potential uses so is broken out and can be used separately.

Pretty Poly provides an antialiased, pixel format agnostic, complex polygon 
drawing engine designed specifically for use on resource-constrained 
microcontrollers.

## Approach

Pretty Poly is a tile-based renderer that calls back into your code with each 
tile of output so that it can be blended into your framebuffer.

This allows Pretty Poly to use very little memory (around 4kB total, statically 
allocated) and focus purely on drawing polygons without worrying about what the
pixel format of the framebuffer is.

Features:

- Renders polygons: concave, self-intersecting, multi contour, holes, etc.
- C17 header only library: simply copy the header file into your project
- Tile based renderer: low memory footprint, cache coherency
- Low memory usage: A few kilobytes of heap memory required
- High speed on low resource platforms: optionally no floating point
- Antialiasing modes: X1 (none), X4 and X16 super sampling
- Bounds clipping: all results clipped to supplied clip rectangle
- Pixel format agnostic: renders a "tile" to blend into your framebuffer
- Support for hardware interpolators on rp2040 (thanks @MichaelBell!)

## Using Pretty Poly

Pretty Poly is a header only C17 library.

Making use of it is as easy as copying `pretty-poly.h` into your project and 
including it in the source file where you need to access.

A basic example might look like:

```c
#include "pretty-poly.h"

void callback(const tile_t tile) {
  // TODO: process the tile data here - see below for details
}

int main() {
  // supply your tile blending callback function
  pp_tile_callback(callback); 

  // specificy the level of antialiasing
  pp_antialias(PP_AA_X4);

  // set the clip rectangle
  pp_clip((pp_rect_t){.x = 0, .y = 0, .w = WIDTH, .h = HEIGHT});

  // outline contour of letter "a" in Roboto Black
  pp_point_t a_outer[] = { // outline contour
    {36, 0}, {34, -5}, {21, 1}, {16, 0}, {8, -4}, {2, -16}, {6, -24}, 
    {15, -31}, {28, -34}, {34, -34}, {34, 37}, {27, -44}, {21, -38}, {4, -38}, 
    {4, -41}, {11, -47}, {21, -51}, {33, -51}, {43, -47}, {51, -37}, {51, -13}, 
    {53, -1}, {53, 0}
  };

  // outline of hole in letter "a" in Roboto Black
  pp_point_t a_inner[] = { // outline contour
    {25, -11}, {34, -16}, {34, -25}, {29, -25}, {24, -24}, {20, -17}
  });

  // create contours
  pp_contour_t a_contours[] = {
    {.points = a_outer, .point_count = sizeof(a_outer) / sizeof(pp_point_t)},
    {.points = a_inner, .point_count = sizeof(a_inner) / sizeof(pp_point_t)}
  };

  // create the multi contour polygon
  pp_polygon_t a_polygon = {
    .contours = a_contours, 
    .contour_count = sizeof(a_contours) / sizeof(pp_contour_t)
  };

  // draw the polygon
  draw_polygon(a_polygon);

  return 0;
}
```

### Primitive shapes


### Defining your polygons

Polygons are made up contours which are made up of points.

```c
  // other setup code here...
  // define a square polygon and render it
  pp_point_t points[] = {{-128, -128}, {128, -128}, {128, 128}, {-128, 128}};
  pp_contour_t contour = {.points = &points, .point_count = 4};
  pp_polygon_t polygon = {.contours = &contour, .contour_count = 1};
  pp_render(&polygon);

```

TODO: expand on this a lot...

### Clipping

All rendering will be clipped to the supplied coordinates meaning you do not
need to perform any bounds checking in your rendering callback function - 
normally you would set this to your screen bounds though it could also be used 
to limit drawing to a specific area of the screen.

```c
  // other setup code here...
  pp_clip(0, 0, 320, 240); // set clipping region to bounds of screen
  pp_render(&poly);        // render my poly
```

### Antialiasing

One of the most interesting features of Pretty Poly is the ability of its
rasteriser to antialias (AKA super-sample) the output - this is achieved by
rendering the polygon at a larger scale and then counting how many pixels
within each sampling area fall inside or outside of the polygon.

Antialiasing can have a big effect on performance since the rasteriser has to 
draw polygons either 4 or 16 times larger to achieve its sampling.

The supported antialiasing levels are:

  - `PP_AA_NONE`: no antialiasing
  - `PP_AA_X4`: 4x super-sampling (2x2 sample grid)
  - `PP_AA_X16`: 16x super-sampling (4x4 sample grid)

Example:

```c
  // other setup code here...
  pp_antialias(PP_AA_X4); // set 4x antialiasing
  pp_render(&poly);       // render my poly
```
  
### Transformations

During rendering you can optionally supply a transformation matrix which will
be applied to all geometry of the polygon being rendered. This is extremely
handy if you want to rotate, scale, or move the polygon.

```c
  // other setup code here...
  pp_mat3_t t = pp_mat3_identity(); // get a fresh identity matrix
  pp_transform(&t);                 // set transformation matrix
  
  pp_mat3_rotate(&t, 30);           // rotate by 30 degrees
  pp_render(&poly);                // render my poly

  // move "right" by 50 units, because `pp_transform()` took a pointer to
  // our matrix `t` we can modify the matrix and in doing so also modify
  // the transform applied by `pp_polygon``
  pp_mat3_translate(&t, 50, 0);     
  pp_render(&poly);                // render my poly again
```

There are a number of helper methods to create and manipulate matrices:

- `pp_mat3_identity()`: returns a new identity matrix
- `pp_mat3_rotate(*m, a);`: rotate `m` by `a` degrees
- `pp_mat3_rotate_rad(*m, a);`: rotate `m` by `a` radians
- `pp_mat3_translate(*m, x, y);`: translate `m` by `x`, `y` units
- `pp_mat3_scale(*m, x, y);`: scale `m` by `x`, `y` units
- `pp_mat3_mul(*m1, *m2);`: multiple `m1` by `m2`

### Rendering

Once you have setup your polygon, clipping, antialiasing, and transform set 
then a call to `pp_render` will do the rest. As each tile is processed it 
will be passed into your tile rendering callback function so that you can 
blend it into your framebuffer.

```c
  void blend_tile(const pp_tile_t *t) {
    // iterate over each pixel in the rendered tile
    for(int32_t y = t->y; y < t->y + t->h; y++) {
      for(int32_t x = t->x; x < t->x + t->w; x++) {     
        // get the "value" at x, y - this will be a value between 0 and 255
        // which can be used as an alpha value for your blend function
        uint8_t v = pp_tile_get(t, x, y));

        // call your blending function here
        buffer[y][x] = blend(buffer[y][x], v); // <- it might look like this
      }
    }
  }

  int main() {
    // other setup code here...
    pp_tile_callback(blend_tile);
  }
```

> Pretty Poly provides you the rasterised polygon information as a single
> 8-bit per pixel mask image. It doesn't care what format or bit depth your 
> framebuffer is allowing it to work any combination of software and hardware.

Your callback function will be called multiple times per polygon depending on 
the size and shape, the capacity of the tile buffer, and the level of 
antialiasing used.

  
### Implementing the tile renderer callback

Your tile renderer callback function will be passed a const reference to a 
[`tile_t`](#tile_t) object which contains all of the information needed to 
blend thetile with your framebuffer.

```c++
void my_callback(const tile_t &tile) {
  // process the tile image data here
}
```

> `tile_t` bounds are in framebuffer coordinate space and will always be 
> clipped against your supplied clip rectangle so it is not necessary for you 
> to check bounds again when rendering.

The `w` and `h` properties represent the valid part of the tile data. This is 
not necessarily the full size of the tiles used by the renderer. For example 
if you draw a very small polygon it may only occupy a portion of the tile, or 
if a tile is clipped against your supplied clip rectangle it will only contain 
partial data.

> You must only ever process and draw the data in the tile starting at `0, 0` 
> in the upper-left corner and ending at `w, h` in the lower-right corner.

There are two main approaches to implementing your callback function.

**1\. Using `get_value()` - the slower, easier, option**

Pretty Poly provides a simple way to get the value of a specific coordinate of 
the tile.

The `tile` object provides a `get_value()` method which always returns a value 
between `0` and `255` - this is slower that reading the tile data directly 
(since we need a function call per pixel) but can be helpful to get up and 
running more quickly.

```c++
void callback(const tile_t &tile) {
  for(auto y = 0; y < tile.h; y++) {
    for(auto x = 0; x < tile.w; x++) {      
      // call your blend function
      blend(framebuffer, tile.x + x, tile.y + y, tile.get_value(x, y), colour);
    }
  }
}
```

If this is fast enough for your usecase then congratulations! 🥳 You have just 
saved future you from some debugging 🤦.

**2\. Using `tile_t.data` directly - much faster, but more complicated**

With the faster implementation you need to handle the raw tile data. This is a 
lot faster than using the `get_value()` helper function as it avoids making a 
function call for every pixel.

You can also potentially optimise in other ways:

- read the buffer in larger chunks (32 bits at a time for example)
- check if the next word, or dword is 0 and skip multiple pixels in one go
- equally check if the value is 0xff, 0xffff, etc and write multiple opaque 
  pixels in one go
- scale `value` to better match your framebuffer format
- scale `value` in other ways (not necessarily linear!) to apply effects

Here we assume we're using X4 supersampling - this is not intended to show the 
fastest possible implementation but rather one that's relatively 
straightforward to understand.

```c++
void callback(const tile_t &tile) {
  // map to convert sampling results to alpha values used by our blend function
  static uint8_t alpha_map[5] = {0, 64, 128, 192, 255};

  // pointer to start of tile data
  uint8_t *p = tile.data;

  // iterate over the valid portion of tile data
  for(auto y = 0; y < tile.h; y++) {
    for(auto x = 0; x < tile.w; x++) {
      // get value, map to alpha, and advance data pointer
      uint8_t value = alpha_map[*p++];

      // blend pixel to your framebuffer
      blend(framebuffer, tile.x + x, tile.y + y, value, colour);
    }

    // advance to start of next row
    p += tile.stride - tile.w;
  }
}
```
## Types

### `tile_callback_t`

Callback function prototype.

```c++
typedef void (*pp_tile_callback_t)(const pp_tile_t *tile);
```

Create your own matching callback function to supply to `pp_tile_callback()` - 
for example:

```c++
void tile_render_callback(const pp_tile_t *tile) {
    // perform your framebuffer blending here
}
```

Note that on RP2040 interp1 is used by pretty poly.  If your callback uses 
interp1 it must save and restore the state.

### `pp_tile_t`

Information needed to blend a rendered tile into your framebuffer.

```c++
  struct tile_t {
    int32_t x, y, w, h;  // bounds of tile in framebuffer coordinates
    uint32_t stride;     // row stride of tile data
    uint8_t *data;       // pointer to start of mask data
  };
```

This object is passed into your callback function for each tile providing the 
area of the framebuffer to write to with the mask data needed for blending.

`uint8_t pp_tile_get_value(pp_tile_t *tile, int32_t x, int32_t y)`
`

Returns the value in the tile at `x`, `y `

### `rect_t`

Defines a rectangle with a top left corner, width, and height.

```c++
  struct pp_rect_t {
    int x; // left
    int y; // top
    int w; // width
    int h; // height

    rect_t()
    rect_t(int x, int y, int w, int h);

    bool empty();
    rect_t intersection(const rect_t &c);
    rect_t merge(const rect_t &c);
  };
```

Used to define clipping rectangle and tile bounds.

### `pp_antialias_t`

Enumeration of valid anti-aliasing modes.

```c++
enum antialias_t {
  PP_AA_NONE = 0, // no antialiasing
  PP_AA_X4   = 1, // 4x super sampling (2x2 grid)
  PP_AA_X16  = 2  // 16x super sampling (4x4 grid)
};
```
## Performance considerations

### CPU speed

In principle Pretty Poly can function on any speed of processor - even down to
KHz clock speeds (if you don't mind waiting!).

It runs really nicely on Cortex M0 and above - ideally at 50MHz+ with an 
instruction and data cache. On hardware at this level and above it can achieve 
surprisingly smooth animation of complex shapes in realtime.

> Originally Pretty Poly was developed for use on Pimoroni's RP2040 (a Cortex 
> M0+ @ 125MHz) based products.

### Antialiasing



### Coordinate type

By default Pretty Poly uses single precision `float` values to store 
coordinates allowing for sub pixel accuracy. On systems where floating point 
operations are too slow, or if you want to reduce the size of coordinates 
stored in memory you can override this setting by defining `PP_COORD_TYPE` 
before including `pretty-poly.h`.

For example:

```c
  #define PP_COORD_TYPE int16_t
  #include "pretty-poly.h"

  // points will be 4 bytes and have integer coordinates
  pp_point_t p = {.x = 314, .y = 159}; 
```

Using integer coordinates may result in some "jitter" if you animating shapes 
as their coordinates while have to snap to individual pixels.

_Note_: transformations using the `pp_mat3_t` struct will also us `float` 
operations regardless of what coordinate type you're using.

### Tile size

The more memory you can afford to assign to the tile buffer the better. 
Preparing and rasterising each tile has a fixed overhead so fewer, larger, 
tiles is always preferable.

I've found 4KB to be the sweet spot when trading off between speed and memory 
usage on the RP2040 but if you are working under tighter memory limitations you 
may wish to reduce the buffer size down to 1KB or even 256 bytes.

## Memory usage

By default Pretty Polly allocates a few buffers used for rendering tile data,
calculating scanline intersections, and maintaining state. The default 
configuration reserves about 6kB on the heap for this purpose.

> The default values have been selected as a good compromise between memory use
> and performance and we don't recommend changing them unless you have a good
> reason to!

You can reduce the amount of memory used at the cost of some performance by
defining the rasteriser parameters before including the Pretty Poly header 
file.

```c
#define PP_NODE_BUFFER_HEIGHT 16
#define PP_MAX_NODES_PER_SCANLINE 16
#define PP_TILE_BUFFER_SIZE 4096
#include "pretty-poly.h"
```

`PP_NODE_BUFFER_HEIGHT`  
Default: `16`

The maximum number of scanlines per tile - doesn't normally have a big impact 
on performance. Larger values will quickly consume more memory.

`PP_MAX_NODES_PER_SCANLINE`  
Default: `16`

The maximum number of line segments that can pass through any given scanline. 
You may need to increase this value if you have very complex polygons.

`PP_TILE_BUFFER_SIZE`  
Default: `4096`

The number of bytes to allocate for the tile buffer - in combination with the
current antialias setting and the `PP_NODE_BUFFER_HEIGHT` this will determine
the maximum width of rendererd tiles.