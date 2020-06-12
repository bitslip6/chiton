#ifndef __SHAPES3D_H
#define __SHAPES3D_H 1

#include <stdbool.h>
#include "util.h"
#include "3d.h"

#define FILL_MODE_FULL 1
#define FILL_MODE_EDGES 2
#define FILL_MODE_CORNERS 3

Vec3_s16_t *Shape_Box3D(u16 width, u16 height, u16 depth, u8 num_points, u8 fill_mode, u16 *all_points);

#endif
