#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// #include <glog/logging.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include <pthread.h>


#ifdef __cplusplus
    extern "C" {
        #include "system.h"
        #include "input.h"
        #include "fb_driver.h"
        #include "main.h"
    }
#else
    #include "system.h"
    #include "input.h"
    #include "fb_driver.h"
    #include "main.h"
#endif

GlobalOpts GOPTS;

using namespace aditof;

void m2_tap(int sx, int sy) {
    printf("hit m2 tap callback! [%d/%d]\n", sx, sy);
}

void test_call() {
    printf("YOU TAPPED TEST!\n");
}



int main(int argc, char *argv[]) {

    const char *device_d = "/dev/fb1";
    const char *font_d = "consolas.png";

    int c;
    char *device = (char *)&device_d;
    char *font = (char *)&font_d;
    while((c = getopt(argc, argv, "d:")) != -1) {
        switch (c) {
            case 'd':
                if (optarg) {
                    device = optarg;
                }
                break;
        }
    }

    pthread_t ui_thread;

    GOPTS.running = 1;
    printf("running: [%d]\n", GOPTS.running);
    printf("open frame buffer [%s]\n", device);

    init_fonts();
    DisplayBuffers *disp = fb_init(device, font);
    fb_active_string(disp, "TEST2", 1, 1, 120, test_call);
/*
    pthread_create(&ui_thread, NULL, *ui_input, NULL);

    //void (*m2_tap_ptr)(int, int) = &m2_tap;
    //printf("callback addr [%x]\n", m2_tap_ptr);
    //(*m2_tap_ptr)(1,2);
    //add_tap_callback(m2_tap_ptr);

    GBUF = disp;
    //printf("W%d.H%d\n", disp->width, disp->height);
    //fb_aa_line(disp, 40, 40, 290, 200, 0x0000FF00);//int x0 , int y0 , int x1 , int y1, u_int32_t bcolor, DisplayBuffers *disp) {
    //fb_line(disp,  50, 50, 200, 100, 120, 250);//int x0 , int y0 , int x1 , int y1, u_int32_t bcolor, DisplayBuffers *disp) {
    fb_draw_buffer1(disp);

    printf("main sleeping 5...\n");
    */

    
    aditof::Status status = aditof::Status::OK;
    aditof::System system;
    status = system.initialize();
    if (status != aditof::Status::OK) {
        Panic(40, "Could not initialize TOF system. ensure power and connection");
    }

    std::vector<aditof::Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        Panic(40, "Could not find camera. ensure power and connection");
    }

    aditof::Camera *camera = cameras.front();
    status = camera->initialize();
    if (status != aditof::Status::OK) {
        Panic(40, "Could not iniitalize camera. ensure power and connection");
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        Panic(40, "no camera frame types. ensure power and connection");
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != aditof::Status::OK) {
        Panic(40, "could not set camera frame type. ensure power and connection");
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        Panic(40, "no camera modes available");
    }

    status = camera->setMode("far");
    if (status != Status::OK) {
        Panic(41, "could not set camera mode: far");
    }
    //////////////////////////////////////////////////////////////////////////



    
    //todo: rip image read code from ws8211
    //draw_image("consolas.png");


    //sleep(55);


    return 0;
}

