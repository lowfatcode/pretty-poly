package main

/*
	#include <../../../pretty-poly.h>
*/
import "C"

//export blend_tile
func blend_tile(t C.pp_tile_t) {
	print("in tile callback")
}
