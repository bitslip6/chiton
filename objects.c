#include <string.h>

#ifdef __cplusplus
    extern "C" {
        #include "objects.h"
        #include "fb_driver.h"
        #include "system.h"
    }
#else
    #include "objects.h"
    #include "fb_driver.h"
    #include "system.h"
#endif

/**
 *  draw the camera data to the linux frame buffer using tail call recursion
 */
u8 find_all_objects(GridCell *grid, BB *bbs, u16 x, u16 y, u8 bb_idx) {
    // dont calculate more than 40 bounding boxes...
    if (bb_idx > 40) { return bb_idx; }
    
    // get a reference to the current grid cell
    GridCell *g = &grid[GRID_OFF(x, y)];

    // we have a grid cell that is unmerged and has close objects
    if (g->merge == 0 && g->fill > 2) {
        // set the grid bounding box id
        g->merge = ++bb_idx;
        BB *b = &bbs[bb_idx];
        b->id = bb_idx;
        find_unmerged(g, grid, x, y, b, 0);
    }

    // advance the grid location to the next
    if (++x > GRID_X_SZ) {
        x = 0;
        if (++y > GRID_Y_SZ) {
            return bb_idx;
        }
    }

    // tail recurse
    return find_all_objects(grid, bbs, x, y, bb_idx);
}

/**
 * merge 2 cells, 
 */
GridCell *cell_merge(GridCell *g1, u8 x, u8 y, GridCell *grid) {
    if (x >= GRID_X_SZ || y >= GRID_Y_SZ) {
        return NULL;
    }


    // if the grid to merge is unmerged and has an object in it, test if it is close enough to merge and merge them
    GridCell *g2 = &grid[GRID_OFF(x, y)];
    if (g2->merge == 0 && g2->fill > 2 && g2->avgdeep > 10) {
        // merge if the depths are close...
        if (absi(g2->avgdeep - g1->avgdeep) < MIN_BB_DIFF_DEPTH) {
            return g2;
        }
    }

    return NULL;
}



/**
 * add grid cell g to bounding box bb
 */
void add_cell_to_bb(GridCell *g, BB *bb) {

    // update min/max bb values
    if (g->minx < bb->minx) { bb->minx = g->minx; }
    if (g->maxx > bb->maxx) { bb->maxx = g->maxx; }
    if (g->miny < bb->miny) { bb->miny = g->miny; }
    if (g->maxy > bb->maxy) { bb->maxy = g->maxy; }

    // merge or set the average depth
    if (bb->fill > 0) {
        bb->depth = ((bb->fill * bb->depth) + (g->fill * g->avgdeep)) / (g->fill + bb->fill);
    } else {
        bb->depth = g->avgdeep;
    }

    // set the grid as part of this BB
    g->merge = bb->id;
    bb->fill += g->fill;
}


/**
 * find an unmerged cell above, below, left or right of grid
 * todo: don't pass x, y get them from g1->gx, g1->gy
 */
int find_unmerged(GridCell *g1, GridCell *grid, u8 x, u8 y, BB *bb, int found) {

    // initial depth calc...
    if (bb->depth == 0) {
        bb->depth = g1->avgdeep;
    }

    // merge right cell
    GridCell *g2;
    if ((g2 = cell_merge(g1, x+1, y, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
    }
    // merge left cell
    if ((g2 = cell_merge(g1, x-1, y, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
    }
    // merge upper cell
    if ((g2 = cell_merge(g1, x, y-1, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
    }
    // merge lower cell
    if ((g2 = cell_merge(g1, x, y+1, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
    }

    return found;
}

/**
 * clear the grid cells 
 * TODO: tailcall recursion
 */ 
void clear_grid(GridCell *grid) {
    memset(grid, 0, sizeof(GridCell) * 32 * 32);

    for (int y=0; y<32; y++) {
        for (int x=0; x<32; x++) {
            GridCell *g = &grid[GRID_OFF(x, y)];
            g->minx = MAXU16;
            g->miny = MAXU16;
            g->gx = x;
            g->gy = y;
        }
    }
}


/**
 * clear the BB object 
 * TODO: tailcall recursion
 */ 
void clear_bb(BB *bbs) {
    memset(bbs, 0, sizeof(BB) * 32 * 32);

    for (int y=0; y<32; y++) {
        for (int x=0; x<32; x++) {
            BB *b = &bbs[GRID_OFF(x, y)];
            b->minx = MAXU16;
            b->miny = MAXU16;
        }
    }
}


/**
 * Iterate over all image pixels and calculate grid depth
 * todo: refactor this as a tail recursive function
 */
void calculate_grid(DisplayBuffers *disp, u16 *src, GridCell *grid) {

    // skip the IR frame and go the depth frame
    uint src_off = CAM_START;
    u8 grid_ctry = GRID_SZ;
    u8 grid_ctrx = GRID_SZ;
    u8 gridy = 0;
    GridCell *g = &grid[0];

    clear_grid(grid);

    for (uint y = 0; y < 240; y++) {

        // advance grid counter, on grid_sz count advance grid y pos
        if(--grid_ctry == 0) {
            grid_ctry = GRID_SZ;
            gridy++;
        }

        u8 gridx = 0;
        grid_ctrx = GRID_SZ;
        gridy = y / GRID_SZ;
        //printf(" 1:  %dx%d GRID: %dx%d = %d\n", 0, y, gridx, gridy, g->fill);
        for (uint x = 0; x < 320; x++) {

            // advance grid counter, on grid_sz count advance grid x pos
            if(--grid_ctrx == 0) {
                grid_ctrx = GRID_SZ;
                gridx++;
            }

            //gridx = x / GRID_SZ;
            g = &grid[GRID_OFF(x/GRID_SZ, gridy)];

            u16 col = average_depth(src, src_off);


            if (col > MIN_BB_DEPTH) {
                //printf(" %d.%d = %d\n", x, y, col);
                g->fill++;
                g->totaldeep += col;
                if (x > g->maxx) {
                    g->maxx = x;
                }
                if (x < g->minx) {
                    g->minx = x;
                }

                if (y > g->maxy) {
                    g->maxy = y;
                }
                if (y < g->miny) {
                    g->miny = y;
                }
            }
            // skip 2 pixels in src
            src_off += 2;
        }
        // skip 1 lines in src
        src_off += CAM_WIDTH;
    }
}
    

/**
 * tail recurse over the grid and calculate the average depth 
 */
void average_grid_depth(GridCell *grid, u16 offset) {
    if (offset <= GRID_NUM) {
        GridCell *g = &grid[offset];
        if (g->fill > 0) {
            g->avgdeep = g->totaldeep / g->fill;
        }
        average_grid_depth(grid, offset + 1);
    }
}

/**
 * draw the bounding grid based on min and max grid values
 */
void draw_grid(DisplayBuffers *disp, GridCell *grid, u16 offset) {
    if (offset <= GRID_NUM) {
        GridCell *g = &grid[offset];
        if (g->fill > 0) {
            draw_box(disp, g->minx, g->miny, g->maxx, g->maxy, BLUE);
        }
        draw_grid(disp, grid, offset + 1);
    }
}

/**
 * draw a label for a bounding box
 */
void draw_bb_label(DisplayBuffers *disp, BB *b) {
    // calculate
    char buff[32];
    u32 sz = (b->maxx - b->minx) * (b->maxy - b->miny);
    if (sz > 40) {
        u8 per = (b->fill / sz) * 100;
        if (b->minx > 160) {
            snprintf(buff, 32, "%04d / %d,%d", b->depth, b->minx, b->maxx);
        } else {
            snprintf(buff, 32, "%04d / %d,%d", b->depth, b->maxx, b->maxy);
        }
        snprintf(buff, 32, "%04d / %d", b->depth, b->maxy);
        //snprintf(buff, 32, "%dr, - %d", b->depth, b->fill);
        u8 len = strlen(buff);
        u8 xoff = min(max(0, b->minx -1), 320-(len * 7));
        u8 yoff = min(max(0, b->miny -9), 230);

        fb_tiny_string(disp, buff, Point2D({xoff, yoff}), 90);
    }
}




/**
 * tailcall recursive function to draw a list of bounding boxes and labels
 */
void draw_bounding_boxes(DisplayBuffers *disp, BB *bbs, u8 object_num) {
    // get a reference to the current bb
    BB *b = &bbs[object_num];
    u16 x1 = b->minx;
    u16 y1 = b->miny;
    u16 x2 = b->maxx;
    u16 y2 = b->maxy;
    // if the bb is in range
    if (x1 < FB_WIDTH && y1 < FB_HEIGHT && x2 < FB_WIDTH && y2 < FB_HEIGHT) { 
        draw_bb_label(disp, b);
        draw_box(disp, x1, y1, x2, y2, 90);
    }
     
    // draw the next bounding box if we have one
    if (object_num > 0) {
        draw_bounding_boxes(disp, bbs, object_num -1);
    }
}



/**
 * find all active bounding boxes
 */
u8 find_bounding(DisplayBuffers *disp, u_int16_t *src, GridCell *grid, BB *bbs) {

    clear_bb(bbs);
    calculate_grid(disp, src, grid);
    average_grid_depth(grid, 0);
    int num_objects = find_all_objects(grid, bbs, 0, 0, 0);

    if (num_objects > 0) {
        draw_bounding_boxes(disp, bbs, num_objects);
    }

    return num_objects;
}

