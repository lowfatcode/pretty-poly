package main

/*
#define PP_IMPLEMENTATION
#include <../../../pretty-poly.h>

// wrap tile callback to dereference pointer
extern void wrapped_blend_tile_callback_go(pp_tile_t t);
void wrapped_blend_tile_callback_c(const pp_tile_t* t) {
  wrapped_blend_tile_callback_go(*t);
}
*/
import "C"
import (
	"image/color"
	"log"
	"math"
	"runtime"
	"unsafe"

	"github.com/hajimehoshi/ebiten/v2"
)

type Game struct{}

func (g *Game) Update() error {
	return nil
}

var s *ebiten.Image
var angle float32
var pen color.NRGBA

func (g *Game) Draw(screen *ebiten.Image) {

	s = screen
	angle += 0.5

	points := []C.pp_point_t{{-32, -32}, {32, -32}, {32, 32}, {-32, 32}}
	paths := []C.pp_path_t{{points: &points[0], count: 4}}
	poly := C.pp_poly_t{paths: &paths[0], count: 1}
	var pinner runtime.Pinner
	pinner.Pin(&points[0])
	pinner.Pin(&paths[0])

	for i := float32(0); i < 20; i += 2 {
		t := C.pp_mat3_identity()
		C.pp_mat3_translate(&t, 80, 60)
		a := math.Sin(float64(angle+i)/50) * 360
		C.pp_mat3_rotate(&t, C.float(a))
		C.pp_mat3_scale(&t, C.float(1.5-i/20), C.float(1.5-i/20))
		C.pp_transform(&t)

		pen = color.NRGBA{uint8(i * 12), uint8(i * 5), uint8(i * 3), uint8(255)}

		C.pp_render((*C.pp_poly_t)(unsafe.Pointer(&poly)))
	}

	pinner.Unpin()

}

func (g *Game) Layout(outsideWidth, outsideHeight int) (screenWidth, screenHeight int) {
	return 160, 120
}

func alpha(sa uint32, da uint32) uint32 {
	return ((sa + 1) * (da)) >> 8
}

func blend_channel(s uint32, d uint32, a uint32) uint8 {
	return uint8(d + ((a*(s-d) + 127) >> 8))
}

func blend(dest color.RGBA, src color.RGBA) color.RGBA {
	if src.A == 0 {
		return dest
	}
	if src.A == 255 {
		return src
	}

	var result color.RGBA
	result.R = blend_channel(uint32(src.R), uint32(dest.R), uint32(src.A))
	result.G = blend_channel(uint32(src.G), uint32(dest.G), uint32(src.A))
	result.B = blend_channel(uint32(src.B), uint32(dest.B), uint32(src.A))
	result.A = max(dest.A, src.A)

	return result

}

func blend_tile(t C.pp_tile_t) {
	for y := int(t.y); y < int(t.y+t.h); y++ {
		for x := int(t.x); x < int(t.x+t.w); x++ {
			alpha := int(C.pp_tile_get(&t, C.int(x), C.int(y)))

			d := s.RGBA64At(x, y)
			da := color.RGBA{uint8(d.R >> 8), uint8(d.G >> 8), uint8(d.B >> 8), uint8(d.A >> 8)}

			pa := color.RGBA{pen.R, pen.G, pen.B, uint8(alpha)}
			c := blend(da, pa)

			s.Set(x, y, c)
		}
	}

}

func main() {

	C.pp_tile_callback(C.pp_tile_callback_t(C.wrapped_blend_tile_callback_c))
	C.pp_antialias(C.PP_AA_X4)
	C.pp_clip(0, 0, 160, 120)

	ebiten.SetWindowSize(640, 480)
	ebiten.SetWindowTitle("Pretty Poly Go Test")
	ebiten.SetWindowResizingMode(ebiten.WindowResizingModeEnabled)

	if err := ebiten.RunGame(&Game{}); err != nil {
		log.Fatal(err)
	}

}
