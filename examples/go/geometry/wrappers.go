package main

/*
	#include <../../../pretty-poly.h>
*/
import "C"

//export wrapped_blend_tile_callback_go
func wrapped_blend_tile_callback_go(t C.pp_tile_t) {
	blend_tile(t)
}
