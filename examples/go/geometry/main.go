package main

/*
#include <../../../pretty-poly.h>

// wrap tile callback to dereference pointer
extern void blend_tile(pp_tile_t t);
void wrap_blend_tile(const pp_tile_t* t) {
  blend_tile(*t);
}

#cgo CFLAGS: -DPP_IMPLEMENTATION
*/
import "C"

//export blend_tile
func blend_tile(t C.pp_tile_t) {
	print("in tile callback")
}

func main() {

	C.pp_tile_callback(C.pp_tile_callback_t(C.wrap_blend_tile))
	C.pp_antialias(C.PP_AA_X4)
	C.pp_clip(0, 0, 320, 240)

	p := C.pp_point_t{x: 1, y: 2}
	p.x = 10
	print(p.x)

}
