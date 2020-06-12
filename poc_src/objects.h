#ifndef __OBJECTS_H
#define __OBJECTS_H 1

#include <sys/types.h>
#include "system.h"
#include "fb_driver.h"


#define MIN_DEPTH 100
#define MIN_BB_DEPTH 200
#define MIN_BB_DIFF_DEPTH 500

#define GRID_SZ 16
#define GRID_X_SZ 20
#define GRID_Y_SZ 15
#define GRID_NUM (GRID_Y_SZ * GRID_X_SZ) 
#define MAXU16 65535
#define BB_MERGE_THRESHOLD 32 // : value = 255 / original value 
#define GRID_OFF(x, y) ((y * GRID_X_SZ) + x)

typedef struct GridCell {
    u8 gx;
    u8 gy;
    u16 minx;
    u16 maxx;
    u16 miny;
    u16 maxy;
    u16 fill;
    u32 totaldeep;
    u32 avgdeep;
    u32 maxdeep;
    u16 merge;
} GridCell;

typedef struct BB {
    u16 id;
    u16 minx;
    u16 maxx;
    u16 miny;
    u16 maxy;
    u16 depth;
    u16 maxdepth;
    u16 calc_sz;
    u32 fill;
} BB;

u8 find_bounding(DisplayBuffers *disp, u_int16_t *src, GridCell *grid, BB *bbs);

// INTERNAL FUNCTIONS:
int find_unmerged(GridCell *g1, GridCell *grid, u8 x, u8 y, BB *bb, int found);
u8 calculate_max_speed(DisplayBuffers *disp, BB *bbs, int num_objects);
void clear_bounding_boxes(db_t *buffer, BB *bbs, u8 object_num);

#endif
