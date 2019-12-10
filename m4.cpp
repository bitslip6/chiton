
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
        #include "borg_driver.h"
    }
#else
    #include "input.h"
    #include "system.h"
    #include "objects.h"
    #include "fb_driver.h"
    #include "main.h"
    #include "borg_driver.h"
#endif



//andy modify


using namespace aditof;

db_t *data1;
db_t *data2;
bool data_frame = true;
char condition = 'W';
// TODO: add a second line here for drive code...
char global_text1[32];
char global_text2[32];
u8 floor_buffer = 0;
DemoConf *GCONF;

void *draw_vixels(void *foo);

void tap(int x, int y) {
    printf("screen tapped %dx%d\n", x, y);
}

// program entry point
int main(int argc, char *argv[]) {


    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;



    GCONF = (DemoConf *)malloc(sizeof(DemoConf));
    GCONF->running = 1;
    GCONF->bbon = 0;
    GCONF->max_depth = 600;
    GCONF->mid_depth = 1500;
    GCONF->min_depth = 3500;

    //system_init(argc, argv);
    DisplayBuffers *disp = fb_init((char *)"/dev/fb1");
    fb_large_string(disp, (char *)"Loading Camera...", Point2D({10, 10}), 40);
    render(disp, disp->buffer1);
    SigHandlers();

    // TODO: command line args...
    pthread_t draw_thread, ui_thread, drive_thread;

    // TODO: move this to a function that returns a
    // connected camera
    // Map the device to memory and frame buffer
        // TODO: change background logo to demo...
    //display_image("logo.png");
    //pthread_mutex_lock(&demo_lock);

    BorgInit();
    // fork the threads for driving and drawing...
    pthread_create(&draw_thread, NULL, draw_vixels, disp);
    pthread_create(&drive_thread, NULL, random_drive, disp);
    //pthread_create(&ui_thread, NULL, ui_input, disp);
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

    status = camera->setMode("medium");
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
    //char text_buff[32];

    data1 = (db_t *)malloc(FRAME_SIZE);
    data2 = (db_t *)malloc(FRAME_SIZE);


    // loop until !running and calculate FPS
    gettimeofday(&fps_s, NULL); 
    while (GCONF->running) {

        //usleep(100000);
        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            printf("skipped frame!\n");
            sprintf(global_text1, "frame skip");
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
    char buff[32];
    GridCell *grid = (GridCell *)malloc(sizeof(GridCell) * 32 * 32);
    BB *bbs = (BB *)malloc(sizeof(BB) * 32 * 32);


    // TODO: load MAX_FLOOR_BUFFERS here and calcualate and average out the buffer
    // TODO: need smoothing algorithm

    while(GCONF->running) {
        // spin wait for the next frame to be available...
        if (data_frame == last) {
            usleep(10000);
            continue;
        }
        // get a pointer to the just updated frame
        last = data_frame;

        // incriment the floor buffer number and offset
        if(floor_buffer < MAX_FLOOR_BUFFERS) { 
            floor_buffer++;
            disp->floor_off = disp->floor_sz * floor_buffer;
        }
        //printf("floor offset [%d]\n", disp->floor_off);
        
        // clear the object distance buffer...
        memset(disp->bufferd, 0, disp->sz);
        db_t *ptr = (data_frame) ? data1 : data2;
        // draw the raw frame data to the display buffer
        gauss_blur(ptr+CAM_START, disp->buffer2, 640, 480, 3);

        draw_camera(disp, ptr, 0, 0, CAM_START, 0);
        //draw_camera(disp, disp->buffer2, 0, 0, CAM_START, 0);
        if (GCONF->bbon > 0) {
            // find object bounding boxes
            int num_boxes = find_bounding(disp, ptr, grid, bbs);
            //int max_speed = calculate_max_speed(disp, bbs, num_boxes);

            clear_bounding_boxes(disp->buffer2, bbs, num_boxes);
            //int max_speed = 50;

            //printf("max speed %d, boxes: %d\n", max_speed, boxes);
        }

        // draw the current frame rate
        snprintf(buff, 32, "%d", GCONF->fps);
        fb_large_string(disp, buff, Point2D({10, 210}), PURPLE);
        fb_tiny_string(disp, (char *)"fps", Point2D({50, 230}), PURPLE);

        fb_large_string(disp, global_text1, Point2D({10,10}), PURPLE);
        fb_large_string(disp, global_text2, Point2D({10,50}), GREEN);

        int col = disp->buffer2[(120*320)+170];
        snprintf(buff, 32, "%d", col);
        u8 color = 1;
        if (col < 500) {
            color = 120;
        }
        fb_tiny_string(disp, buff, Point2D({140, 100}), color);


/*
        200 = 2m
        650 = 1m 
        1200 = 2ft
        3500 = 1ft

        RIGHT
        @4000,300x
        @1200,225x
        @600,215x

        LEFT
        @550,200x
        @1000,100x
        @4000,20x


        open
        @3400,45-225
        @1000,120-185
        @500,135-175



        ypos:

        15

        154,2000
        143,1200
        130,500
        132,585

        */




        // swap(copy) the double buffers
        render(disp, disp->buffer1);

        // wrap the floor buffer
        if(floor_buffer == MAX_FLOOR_BUFFERS) {
            floor_buffer = 0;
            //floor_buffer = MAX_FLOOR_BUFFERS + 2; // never run this code again..

            /*
            disp->floor_off;
            u32 last_floor_off = disp->floor_last + (FB_WIDTH * (FB_HEIGHT - FLOOR_HORIZON));
            //box_blur(&disp->buffer2[last_floor_off], disp->buffert, FB_WIDTH, FB_HEIGHT, 5);
            //gaus_blur();
            //gauss_blur(&disp->buffer2[last_floor_off], disp->buffert, FB_WIDTH, FB_HEIGHT, 5);
            gauss_blur(&disp->buffer2[last_floor_off], disp->buffert, FB_WIDTH, FB_HEIGHT - FLOOR_HORIZON, 2);
            //draw_image(FB_WIDTH, FB_HEIGHT, 0, 0, disp->buffert, disp);
            */

            u32 soff = CAM_START;

            // if Y 120-150 > 100
            //    Y 150-170 > 200
            //    Y 170-200 > 300
            //    Y 200-240 > 350

/*
            20-50 100
            50-70 200
            70-100 300
            100-240 350
*/

/*
            ptr = disp->buffer2;
            for (int i=FLOOR_HORIZON; i<FB_HEIGHT; i++) {
                //soff = CAM_START + (i * 2 * CAM_WIDTH);
                soff = 0 + (i * 2 * CAM_WIDTH);
                printf("[%d], %d/%d - 32: %d,  160: %d,  300: %d\n", 
                    CAM_START, i, soff,
                    ptr[soff+320],
                    ptr[soff+340],
                    ptr[soff+360]);
            }
            */


            //gauss_blur(disp->buffer2, disp->buffert, FB_WIDTH, FB_HEIGHT, 2);


            GCONF->bbon = 1;
            /*
            int soff = 0;//last_floor_off + FLOOR_HORIZON * FB_WIDTH;
            for (int y=0; y<FB_HEIGHT; y++) {
                for (int x=0; x<FB_WIDTH; x++) {
                    int off = (y*FB_WIDTH)+x;
                    disp->buffer1[off] = HsvToRgb(200, 180, (disp->buffert[off] * 0.1));
                }
            }

            render(disp, disp->buffer1);
            sleep(1);
            */
        }
    }

    return NULL;
}
    
