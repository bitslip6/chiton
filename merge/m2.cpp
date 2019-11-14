#include <stdio.h>

#include <glog/logging.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "system.h"
#include "main.h"
#include "fb_driver.h"


int main(int argc, char *argv[]) {

    DisplayBuffers disp;

    const char *fb = "/dev/fb0";
    const char *font = "consolas.png";

    printf("main...\n");
    init_fonts();
    //fb_init(fb, font, &disp);//const char *file_name, const char *font_file, DisplayBuffers *disp) {
    printf("init!");


    return 0;
}

