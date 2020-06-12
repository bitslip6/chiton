#include "util.h"
#include "draw.h"
#include "3d.h"
#include "3dshapes.h"

Vec3_s16_t *Shape_Box3D(u16 width, u16 height, u16 depth, u8 num_points, u8 fill_mode, u16 *all_points) {

    *all_points = 8;
    if (fill_mode == FILL_MODE_EDGES) {
        *all_points = MAX(8, (num_points * num_points * 6));
    } else if (fill_mode == FILL_MODE_FULL) {
        *all_points = num_points * num_points * num_points;
    }
    Vec3_s16_t *box = NEWMEM(Vec3_s16_t, *all_points);

    u16 xi=(width*2)/num_points;
    u16 yi=(height*2)/num_points;
    u16 zi=(depth*2)/num_points;

    //printf("INC [%dx%dx%d], [%d.%d.%d]\n", -width, -height, -depth, xi, yi, zi); 
    //return box;

    u16 index=0;
    u8 edge_faces=0;
    for (s16 x=-width; x<=width; x+=xi) {
        for (s16 y=-height; y<=height; y+=yi) {
            for (s16 z=-depth; z<=depth; z+=zi) {
                edge_faces = 0;
                if (z == -depth || z == depth) { edge_faces++; }
                if (x == -width || x == width) { edge_faces++; }
                if (y == -height || y == height) { edge_faces++; }


                if (edge_faces < 3) {
                    if (fill_mode == FILL_MODE_CORNERS) { continue; }
                    if (fill_mode == FILL_MODE_EDGES && edge_faces < 2) { continue; }
                }
                
                if (index < *all_points) {
                    box[index].d[VEC_X] = x;
                    box[index].d[VEC_Y] = y;
                    box[index].d[VEC_Z] = z;
                    //printf("point %d.[%d] [%dx%dx%d]\n", index, edge_faces, x, y, z);
                    index++;
                } else {
                    //printf("BEYOND %d.[%d] [%dx%dx%d]\n", index, edge_faces, x, y, z);
                }
            }
        }
    }
    *all_points = index;

    return box;
}
