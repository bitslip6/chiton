#include "util.h"
#include "demos.h"
#include "image.h"
#include "system.hpp"
#include <math.h>
#include <stdio.h>

GlobalOpts *GOPTS;
DisplayBuffers *GDISP;

void test_find_laser(u16 x1, u16 y1) {

    printf("test find laser begin %d/%d!\n", x1, y1);
    GOPTS->center_x = 320;
    GOPTS->center_y = 240;
    //u16 x1 = 10;
    //u16 y1 = 10;
    u8 quad = find_laser(x1, y1);
    printf("quadrant: [%d]\n", quad);
}

int main(int arc, char **argv) {

    sys::SystemInit(640, 480);
    printf("test begin!\n");
    test_find_laser(300, 250);
}




