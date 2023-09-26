package main

/*
#define PP_IMPLEMENTATION
#include <../../../pretty-poly.h>

// wrap tile callback to dereference pointer
extern void blend_tile(pp_tile_t t);
void wrap_blend_tile(const pp_tile_t* t) {
  blend_tile(*t);
}
*/
import "C"
import (
	"runtime"
	"unsafe"
)

type point struct {
	x float32
	y float32
}
type path struct {
	points *point
	count  uint32
}
type polygon struct {
	paths *path
	count uint32
}

func main() {

	C.pp_tile_callback(C.pp_tile_callback_t(C.wrap_blend_tile))
	C.pp_antialias(C.PP_AA_X4)
	C.pp_clip(0, 0, 320, 240)

	points := []point{{-128, -128}, {128, -128}, {128, 128}, {-128, 128}}
	paths := []path{{points: &points[0], count: 4}}
	poly := polygon{paths: &paths[0], count: 1}

	var pinner runtime.Pinner
	pinner.Pin(&points[0])
	pinner.Pin(&paths[0])
	pinner.Pin(&poly)
	C.pp_render((*C.pp_poly_t)(unsafe.Pointer(&poly)))
	pinner.Unpin()

}
