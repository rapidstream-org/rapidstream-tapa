#ifndef DILATE_H
#define DILATE_H

#define GRID_ROWS 4096
#define GRID_COLS 4096

#define KERNEL_COUNT 15
#define PART_ROWS GRID_ROWS / KERNEL_COUNT

#define ITERATION 512

#include "ap_int.h"
#include <inttypes.h>
#define DWIDTH 512
#define INTERFACE_WIDTH ap_uint<DWIDTH>
	const int WIDTH_FACTOR = DWIDTH/32;
#define PARA_FACTOR 16

#define STAGE_COUNT 1
#define TOP_APPEND 0
#define BOTTOM_APPEND 1026
#define OVERLAP_TOP_OVERHEAD 0
#define OVERLAP_BOTTOM_OVERHEAD 524286
#define DECRE_TOP_APPEND 0
#define DECRE_BOTTOM_APPEND 1026
#endif
