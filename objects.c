#include <string.h>

#ifdef __cplusplus
    extern "C" {
        #include "objects.h"
        #include "fb_driver.h"
        #include "system.h"
    }
#else
    #include "objects.h"
    #include "fb_Driver.h"
    #include "system.h"
#endif



/*
signed char find_object(u16 x, u16 y, u16 dist, DetectObject (&objects)[50]) {
    for (u8 i=0; i<50; i++) {
        DetectObject *o = &objects[i];
        if (o->min_x == 0) {
            o->min_x = x;
            o->min_y = y;
            o->max_x = x+2;
            o->max_y = y+4;
            o->dist = dist;
            //o->dist = dstatic inline int absi(int x) { 
    return (x < 0) ? -x : x;
};

void inline draw_box(DisplayBuffers *disp, u16 x1, u16 y1, u16 x2, u16 y2, u8 hue) {
    fb_line(disp, x1, y1, x2, y1, hue, 250);
    fb_line(disp, x1, y2, x2, y2, hue, 250);
    fb_line(disp, x1, y1, x1, y2, hue, 250);
    fb_line(disp, x2, y1, x2, y2, hue, 250);
};

u16 inline average_depth(u16 *src, u32 src_off) {
    u16 col = src[src_off];
    col += src[src_off + 1];
    col += src[src_off + CAM_WIDTH];
    col += src[src_off + CAM_WIDTH + 1];
    return col / 4 ;
}

ist;
            //printf("new object at: X.%d, Y.%d\n", x, y);
            return i;
        }
        if (y >= o->min_y && y-2 <= o->max_y) {
            if (absi(o->dist - dist) < 100) {
                if (x+15 >= o->min_x && x-15 <= o->max_x) {
                    o->fill_cnt++;
                    return i;
                }
            } else {
                if (x+5 >= o->min_x && x-5 <= o->max_x) {
                    o->fill_cnt++;
                    return i;
                }
            }
        }
        if (x+5 >= o->min_x && x+5 <= o->max_x && y >= o->min_y -1 && y-2 <= o->max_y) {
            //printf("found object: %d X%d Y%d", i, x, y);
            o->fill_cnt++;
            return i;
        } else {
            //printf ("%d X:%dY:%d  X-> does not match min_x.%d max_x.%d min_y.%d max_y.%d\n", i, x, y, o->min_x, o->max_x, o->min_y, o->max_y);
        }

    return -1;
}
*/


/*
void compact_objects(int removeIdx, DetectObject (&objects)[50]) {
    DetectObject *o1 = &objects[removeIdx];
    o1->min_x = 0;
    for (u8 i=removeIdx+1; i<50; i++) {
        DetectObject *o2 = &objects[i];
        if (o2->min_x > 0) {
            o1->min_x = o2->min_x;
            o1->max_x = o2->max_x;
            o1->min_y = o2->min_y;
            o1->max_y = o2->max_y;
            o1->fill_cnt = o2->fill_cnt;
            o1->dist = o2->dist;
        }
        DetectObject *o1 = &objects[i];
    }
}
*/

/**
 * merge 2 cells, 
 */
GridCell *cell_merge(GridCell *g1, u8 x, u8 y, GridCell *grid) {
    if (x >= GRID_X_SZ || y >= GRID_Y_SZ) {
        return NULL;
    }

    GridCell *g2 = &grid[GRID_OFF(x, y)];
    if (g2->merge == 0) {
        //printf(" -> merge? %dx%d / %dx%d  %d_%d / %d_%d\n", g1->gx, g1->gy, x, y, g1->fill, g1->avgdeep, g2->fill, g2->avgdeep);
        //printf("G2: %dx%d  VS  %dx%d - deep %d/%d\n", x, y, g1->gx, g1->gy, g2->fill, g2->avgdeep);

        if (g2->avgdeep > 10) {
            //printf("   -> fill is: %d\n", g2->fill);

            // merge if the depths are close...
            if (absi(g2->avgdeep - g1->avgdeep) < MIN_BB_DIFF_DEPTH) {
                //printf ("   !! avg deep is %d/%d:\n", g1->avgdeep, g2->avgdeep);
                //g2->merge = g1->merge;
                return g2;
            } else {
                //printf ("   XX avg deep is %d/%d:\n", g1->avgdeep, g2->avgdeep);
            }
        }
    }
    /* else {
        printf("MERGED: %dx%d  - %dx%d\n", g1->gx, g1->gy, g2->gx, g2->gy);
    }
    */


    return NULL;
}


void add_cell_to_bb(GridCell *g, BB *bb) {
    if (g->minx < bb->minx) { bb->minx = g->minx; }
    if (g->maxx > bb->maxx) { bb->maxx = g->maxx; }
    if (g->miny < bb->miny) { bb->miny = g->miny; }
    if (g->maxy > bb->maxy) { bb->maxy = g->maxy; }

    //printf (" -- ADD %dx%d to %d\n", g->gx, g->gy, bb->id);

    if (bb->fill > 0) {
        bb->depth = ((bb->fill * bb->depth) + (g->fill * g->avgdeep)) / (g->fill + bb->fill);
    } else {
        bb->depth = g->avgdeep;
    }
    g->merge = bb->id;
    bb->fill += g->fill;
}


int find_unmerged(GridCell *g1, GridCell *grid, u8 x, u8 y, BB *bb, int found) {
    //printf("finding unmerged %dx%d id:%d\n", x, y, bb->id);

    // initial depth calc...
    if (bb->depth == 0) {
        bb->depth = g1->avgdeep;
    }

    // merge right cell
    GridCell *g2;
    if ((g2 = cell_merge(g1, x+1, y, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
        //printf(" - F x+1, %dx%d [%d]\n", x, y, found);
    }
    // merge left cell
    if ((g2 = cell_merge(g1, x-1, y, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
        //find_unmerged(g2, grid, x-1, y, bb, ++found);
        //printf(" - F x-1, %dx%d [%d]\n", x, y, found);
    }
    // merge upper cell
    if ((g2 = cell_merge(g1, x, y-1, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
        //find_unmerged(g2, grid, y-1, y, bb, ++found);
        //printf(" - F y-1, %dx%d [%d]\n", x, y, found);
    }
    // merge lower cell
    if ((g2 = cell_merge(g1, x, y+1, grid)) != NULL) {
        add_cell_to_bb(g2, bb);
        find_unmerged(g2, grid, g2->gx, g2->gy, bb, ++found);
        //find_unmerged(g2, grid, y+1, y, bb, ++found);
        //printf(" - F y+1, %dx%d [%d]\n", x, y, found);
    }

    return found;
}

/**
 * clear the grid cells 
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
 *  average the 4 pixels to the lower right of the location offset src my src_off
 */
/*
u16 inline average_depth(u16 *src, u32 src_off) {
    u16 col = src[src_off];
    col += src[src_off + 1];
    col += src[src_off + CAM_WIDTH];
    col += src[src_off + CAM_WIDTH + 1];
    return col / 4 ;
}
*/

/**
 * clear the BB object 
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
 * todo: refactor this as a tail recursive function
 */
void calculate_grid(u16 *src, GridCell *grid) {

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


                //printf(" 2:  %dx%d GRID: %dx%d = %d\n", x, y, gridx, gridy, g->fill);
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


u8 find_bounding(DisplayBuffers *disp, u_int16_t *src, GridCell *grid, BB *bbs) {

    double t = 25;
    int cnt = 0;

    struct timeval start, end;
    gettimeofday(&start, NULL);
    clear_bb(bbs);
    calculate_grid(src, grid);
    average_grid_depth(grid, 0);
                
    // merge bounding boxes
    u16 offset = 0;
    uint bb_idx=0;
    BB *b = &bbs[bb_idx];
    GridCell *g = &grid[0];
    int merged = 1;
    int max_ctr = 0;

    while (merged > 0 && max_ctr++ < 20) {
    for (int y=0; y<GRID_Y_SZ; y++) {
        for (int x=0; x<GRID_X_SZ; x++) {
            if (bb_idx > 40) { break; }
            g = &grid[GRID_OFF(x, y)];

            // skip if cell is already merged or does not contain an object
            if (g->merge != 0 || g->fill < 2) { continue; }
            // cell is unmerged, start a new bounding box
            g->merge = ++bb_idx;
            
            b = &bbs[bb_idx];
            b->id = bb_idx;
//int find_unmerged(GridCell *g1, GridCell *grid, u8 x, u8 y, BB *bb, int found) {
            //printf("ROOT SEARCH: %dx%d\n", x, y);
            merged += find_unmerged(g, grid, x, y, b, 0);
            //printf("%d,%d,  BOUNDING BOXES FOUND: [%d] idx:%d\n", x, y, merged, bb_idx);
            //bb_idx++;
            //b = &bbs[++bb_idx];
            //b->minx = 999;
            //b->miny = 999;
        }
    }
    }

    gettimeofday(&end, NULL);
    t = (end.tv_sec - start.tv_sec) * 1e6;
    t += (end.tv_usec - start.tv_usec) * 1e-6;

    printf("init bounding complete: %f sec\n", t);


    //printf(" -- merged bbs: %d / itr [%d]\n", bb_idx, max_ctr);

    for (; bb_idx >= 1; --bb_idx) {
        BB *b = &bbs[bb_idx];

        u16 x1 = b->minx;
        u16 y1 = b->miny;
        u16 x2 = b->maxx;
        u16 y2 = b->maxy;
        //printf("line: [%d] %d/%d . %d/%d\n", bb_idx, x1, y1, x2, y2);
        if (x1 > 320 || y1 > 240 || x2 > 320 || y2 > 240) { 
            printf("XY line out of range: [%d] 1:%d/%d . 2:%d/%d\n", bb_idx, x1, y1, x2, y2);
            continue;
        }


        draw_box(disp, b->minx, b->miny, b->maxx, b->maxy, 90);
    }

    //draw_grid(disp, grid, 0);


/*
    for (int y=0; y<15; y++) {
        for (int x=0; x<20; x++) {
            g = &grid[(y*20) + x];
            if (g->fill > 10 && g->minx < 999) {
                cnt++;

                u16 x1 = g->minx;
                u16 y1 = g->miny;
                u16 y2 = (y*grid_sz)+grid_sz;
                u16 x2 = (x*grid_sz)+grid_sz;
                printf("line: [%dx%d] %d/%d . %d/%d\n", x, y, x1, y1, x2, y2);
                fb_line(disp, x1, y1, x2, y1, 190, 25);
                fb_line(disp, x1, y2, x2, y2, 190, 25);
                fb_line(disp, x1, y1, x1, y2, 190, 25);
                fb_line(disp, x2, y1, x2, y2, 190, 25);
            }
        }
    }
    */
    
    return cnt;
}

