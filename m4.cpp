
#include <glog/logging.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <algorithm>
#include <fcntl.h>

#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <linux/i2c-dev.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <aditof/device_interface.h>

#include <pthread.h>

#ifdef __cplusplus
    extern "C" {
        #include "input.h"
        #include "system.h"
        #include "objects.h"
        #include "fb_driver.h"
        #include "main.h"
    }
#else
    #include "input.h"
    #include "system.h"
    #include "objects.h"
    #include "fb_driver.h"
    #include "main.h"
#endif



using namespace aditof;

db_t *data1;
db_t *data2;
bool data_frame = true;
DemoConf *GCONF;

void *draw_vixels(void *foo);


// program entry point
int main(int argc, char *argv[]) {


    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;



    GCONF = (DemoConf *)malloc(sizeof(DemoConf));
    GCONF->running = 1;

    //system_init(argc, argv);
    DisplayBuffers *disp = fb_init((char *)"/dev/fb1", (char *)"consolas.png");
    fb_write_string(disp, (char *)"loading camera...", 10, 10, 40);
    fb_draw_buffer1(disp);

    // TODO: command line args...
    pthread_t draw_thread, ui_thread;//, drive_thread;

    // TODO: move this to a function that returns a
    // connected camera
    // Map the device to memory and frame buffer
        // TODO: change background logo to demo...
    //display_image("logo.png");
    //pthread_mutex_lock(&demo_lock);

    // fork the threads for driving and drawing...
    pthread_create(&draw_thread, NULL, draw_vixels, disp);
    //pthread_create(&drive_thread, NULL, random_drive, NULL);
    pthread_create(&ui_thread, NULL, ui_input, NULL);
    //add_tap_callback(tap);


    // aditof::Camera *c = get_tof_camera();
    // TODO: figure out how to pass a reference to the camera successfully....
    //////////////////////////////////////////////////////////////////////////
    aditof::Status status = aditof::Status::OK;
    aditof::System system;
    status = system.initialize();
    if (status != aditof::Status::OK) {
        Panic(80, "Could not initialize TOF system. ensure power and connection");
    }

    std::vector<aditof::Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        Panic(81, "Could not find camera. ensure power and connection");
    }

    aditof::Camera *camera = cameras.front();
    status = camera->initialize();
    if (status != aditof::Status::OK) {
        Panic(82, "Could not iniitalize camera. ensure power and connection");
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        Panic(83, "no camera frame types. ensure power and connection");
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != aditof::Status::OK) {
        Panic(84, "could not set camera frame type. ensure power and connection");
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        Panic(85, "no camera modes available");
    }

    status = camera->setMode("far");
    if (status != Status::OK) {
        Panic(86, "could not set camera mode");
    }

    auto device = camera->getDevice();

    const size_t REGS_CNT = 5;
    uint16_t afeRegsAddr[REGS_CNT] = {0x4001, 0x7c22, 0xc34a, 0x4001, 0x7c22};
    uint16_t afeRegsVal[REGS_CNT] = {0x0006, 0x0004, 0, 0x0007, 0x0004};
    afeRegsVal[2] |= 50;
    afeRegsVal[2] |= 0x8000;
    device->writeAfeRegisters(afeRegsAddr, afeRegsVal, 5);
    // TO DO: This breaks things over USB. Works well on the target and
    // over ethernet.
    //m_ctrl->writeAFEregister(afeRegsAddr, afeRegsVal, 5);

    //////////////////////////////////////////////////////////////////////////


    int frame_ctr = 0;
    aditof::Frame frame;
    struct timeval fps_s, fps_e;
    db_t *data_ptr;
    char text_buff[32];

    data1 = (db_t *)malloc(FRAME_SIZE);
    data2 = (db_t *)malloc(FRAME_SIZE);


    // loop until !running and calculate FPS
    gettimeofday(&fps_s, NULL); 
    while (GCONF->running) {

        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            printf("skipped frame!\n");
            sprintf(text_buff, "frame skip");
            usleep(10000);
        } else {
            // load the IR and DEPTH data from the camera into a new
            // pointer (will be freed / replaced on next call)
            u_int16_t *tmp;
            status = frame.getData(aditof::FrameDataType::RAW, &tmp);
            if (status != aditof::Status::OK) {
                Panic(87, "Could not get frame data!\n");
            } else {
                //data_ptr = (data_frame) ? data2 : data1;
                // TODO: we only need to copy the IR frame, not all the frame
                memcpy((data_frame) ? data1 : data2, tmp, FRAME_SIZE);
                data_frame = !data_frame;
            }
        }
        gettimeofday(&fps_e, NULL); 

        // calcualte time since last fps capture
        double time_taken = (fps_e.tv_sec - fps_s.tv_sec) * 1e6; 
        time_taken = (time_taken + (fps_e.tv_usec - fps_s.tv_usec)) * 1e-6; 

        // count number of frames processed in a second
        frame_ctr++;
        // after 1 second update the display info
        if (time_taken >= 1.0) {
            gettimeofday(&fps_s, NULL); 
            GCONF->fps = frame_ctr;
            frame_ctr = 0;
        }
    }

    fb_cleanup(disp);
    free(data1);
    free(data2);
    printf("application finished\n");
}


bool dataframe_true() { return data_frame == true; }
bool dataframe_false() { return data_frame == false; }

void do_settings() {
    printf("SETTINGS!\n");
}

void tap(u16 x, u16 y) {
    printf("screen tapped %dx%d\n", x, y);
}

void *draw_vixels(void *foo) {

    DisplayBuffers *disp = (DisplayBuffers *) foo;


    // TODO: clean this part up..
    // skip 2 frames so both buffers are populated
    printf("waiting for 3 data frames\n");
    wait_for(dataframe_true, 10000);
    wait_for(dataframe_false, 10000);
    wait_for(dataframe_true, 10000);
    printf("running ...\n");
    bool last = data_frame;

    //close(pipe_fd[0]);

    GridCell *grid = (GridCell *)malloc(sizeof(GridCell) * 32 * 32);
    BB *bbs = (BB *)malloc(sizeof(BB) * 32 * 32);

    last = data_frame;
    while(GCONF->running) {
        // spin wait for the next frame to be available...
        if (data_frame == last) {
            usleep(5000);
            continue;
        }

        // get a pointer to the just updated frame
        //my_ptr = (data_frame) ? data1 : data2;
        last = data_frame;
        
        

        draw_camera(disp, (data_frame) ? data1 : data2, 0, 0, CAM_START, 0);

        char buff[32];
        snprintf(buff, 32, "%d fps", GCONF->fps);


        find_bounding(disp, (data_frame) ? data1 : data2, grid, bbs);//GridCell *grid, BB *bbs) {

        //fb_active_string(disp, (char *)"settings", 1, 1, 120, do_settings);
        fb_write_string(disp, buff, 0, 210, 100);
        fb_draw_buffer1(disp);


        // draw the background IR data
        //if (g_conf.iron) {
        //   draw_image_half_slow(TOF_WIDTH, TOF_FULL_HEIGHT, 0, 0, my_ptr, buffer);
        //}
    }

    return NULL;
}
    
