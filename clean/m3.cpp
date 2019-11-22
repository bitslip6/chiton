#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// #include <glog/logging.h>

#include <string.h>
#include <aditof/camera.h>
#include <aditof/device_interface.h>
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
    char *device = (char *)device_d;
    char *font = (char *)font_d;
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
    pthread_create(&ui_thread, NULL, ui_input, NULL);

    printf("running: [%d] device [%s]\n", GOPTS.running, device);

    init_fonts();
    DisplayBuffers *disp = fb_init(device, font);
    printf("have disp!\n");

    //fb_line(disp, 10, 10, 310, 230, 120, 250);
    printf("drew line\n");
    void (*test_callback)();
    //test_callback = &test_call;
    fb_active_string(disp, "loading camera", 1, 1, 120, &test_callback);
    printf("drew string\n");

    unsigned char *setting_image = load_png("settings_24.png");

    fb_draw_buffer1(disp);
    /*
    printf("updated display\n");
    sleep(2);
    printf("fin\n");
    exit(0);
    */

/*
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

    aditof::Frame frame;
    aditof::DeviceInterface *adidevice = camera->getDevice();
    const size_t REGS_CNT = 5;
    uint16_t afeRegsAddr[REGS_CNT] = {0x4001, 0x7c22, 0xc34a, 0x4001,
                                      0x7c22};
    uint16_t afeRegsVal[REGS_CNT] = {0x0006, 0x0004, 0, 0x0007, 0x0004};

    afeRegsVal[2] |= 50;
    afeRegsVal[2] |= 0x8000;

    // TO DO: This breaks things over USB. Works well on the target and
    // over ethernet.
    adidevice->writeAfeRegisters(afeRegsAddr, afeRegsVal, 5); 


    DetectObject object_detects[50];// = DetectObject[50]

    for (int i=0; i<8; i++) {
        // clear out the detected objects
        memset(object_detects, 0, sizeof(DetectObject) * 50);
        status = camera->requestFrame(&frame);

        u_int16_t *tmp;
        if (status != aditof::Status::OK) {
            printf("skipped frame!\n");
            //sprintf(text_buff, "frame skip");
            usleep(5000);
        } else {
            // load the IR and DEPTH data from the camera into a new
            // pointer (will be freed / replaced on next call)
            status = frame.getData(aditof::FrameDataType::RAW, &tmp);
            if (status != aditof::Status::OK) {
                Panic(87, "Could not get frame data!\n");
            } else {
                int bloom_per = draw_depth_data(disp, tmp, object_detects);
               // u8 draw_depth_data(DisplayBuffers *disp, u_int16_t *src, DetectObject (&objs)[50]) {
                if (bloom_per > 25) {
                    fb_write_string(disp, "BLOOMING", 1, 210, 100);
                }
                //data_ptr = (data_frame) ? data2 : data1;
                //memcpy(data_ptr, tmp, 640*960*2);
                //data_frame = !data_frame;
            }
        }

        //uint16_t *tmp;
        //draw_depth_data(disp, tmp);
        
        draw_image_alpha(24, 24, 2, 2, setting_image, disp, 20, 200);
        fb_draw_buffer1(disp);
    }
    printf("finsihed\n");


    
    return 0;
}


void set_small_signal(int thresh, aditof::DeviceInterface *device) {

    const size_t REGS_CNT = 5;
    uint16_t afeRegsAddr[REGS_CNT] = {0x4001, 0x7c22, 0xc34a, 0x4001,
                                      0x7c22};
    uint16_t afeRegsVal[REGS_CNT] = {0x0006, 0x0004, 0, 0x0007, 0x0004};

    afeRegsVal[2] |= thresh;
    afeRegsVal[2] |= 0x8000;

    // TO DO: This breaks things over USB. Works well on the target and
    // over ethernet.
    device->writeAfeRegisters(afeRegsAddr, afeRegsVal, 5); 

}
