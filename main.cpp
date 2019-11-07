
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "projection.h"
#include "geometry.h"
#include "main.h"
#include "font.h"

#include <wiringPi.h>
#include <softPwm.h>
#include <linux/i2c-dev.h>

#include <png.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include <pthread.h>

#define TOF_WIDTH 640
#define TOF_HEIGHT 480
#define TOF_FULL_HEIGHT 960
#define VIXEL_SZ 8
#define MAX_UI_ELM = 4

using namespace aditof;

// pointer to adi frame1
uint16_t *data1;
// pointer to adi frame2
uint16_t *data2;
// pointer to current_frame (data1 or data2)
uint16_t *data_ptr;

// pointer to drawing buffer
u_int16_t *buffer;
u_int16_t *fbm;

// pointer to text drawing buffer
char text_buff[25];
ScreenInfo fb_info;
bool data_frame = true;

// the demo thread  will run until the demo lock is released
pthread_mutex_t demo_lock;

// thet global config data
// TODO: rename DemoConf
DemoConf g_conf;

int edit_value = 0;

// global flag for all systems.  when set to false program should exit
bool running = true;


// defined lower down. thread that continusly draws 
// and calculated vixel data
void *draw_vixels(void *foo);

// display en error message on stderr and exit with an
// exit value of 1
void Panic(const char *msg) {
    perror(msg);
    exit(1);
}

//
// program entry point
int main(int argc, char *argv[]) {

    // google logging for ADI code (YUCK!)
    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;

    // the global config data
    // TODO, add read and write config file support.
    g_conf = { iron: true, depthon: true, bbon: true, fpson: true, senseon: true, sense: 5, max_depth: 4000, min_depth: 2100, bb_min_x: 20, bb_max_x: 60, bb_min_y: 10, bb_max_y: 48 };
    g_conf.vixel_w: TOF_WIDTH / VIXEL_SZ;
    g_conf.vixel_h: TOF_HEIGHT / VIXEL_SZ;


    // TODO: command line args...
    const char *fb_device = "/dev/fb1";
    const char *font_file = "consolas.png";

    // TODO: move this to a function that returns a
    // connected camera
    // Map the device to memory and frame buffer
    fbm = fb_init(fb_device, font_file);
    fb_info = fb_get_buffer_info();
    buffer = (u_int16_t *)malloc(fb_info.buffer_sz*2);
    data1 = (u_int16_t *)malloc(640*960*2);
    data2 = (u_int16_t *)malloc(640*960*2);
    bzero(buffer, fb_info.buffer_sz);
    if (fbm == NULL || buffer == NULL) {
        Panic("unable to allocate famebuffer or virtual buffer");
    }
    pthread_mutex_lock(&demo_lock);
    pthread_create(&demo_thread, NULL, draw_vixels, NULL);

    printf("The framebuffer and virtual buffer mapped successfully.\n");
    printf("allocated [%d] bytes for double buffer at [%x]\n", fb_info.buffer_sz, buffer);

    wiringPiSetupGpio() ;
    //pwmWrite(12, 100);
    //softPwmCreate (18, 1000, 1024) ;
    pthread_t draw_thread;
    data_ptr = (data_frame) ? data2 : data1;
    pthread_create(&draw_thread, NULL, draw_vixels, NULL);

    /*
    snprintf(text_buff, 25, "%dx%d %dbpp", fb_info.width, fb_info.height, fb_info.bpp);
    fb_write_string((char *)"FB: /dev/fb1", 1, 1, -1, fbm);
    fb_write_string(text_buff, 1, 32, -1, fbm);
    fb_write_string("threads", 1, 64, -1, fbm);
    fb_write_string((char *)"init camera...", 1, 96, -1, fbm);
    */


    //aditof::Camera *c = get_tof_camera();
    // TODO: figure out how to pass a reference to the camera successfully....
    //////////////////////////////////////////////////////////////////////////
    aditof::Status status = aditof::Status::OK;
    aditof::System system;
    status = system.initialize();
    if (status != aditof::Status::OK) {
        Panic("Could not initialize TOF system. ensure power and connection");
    }

    std::vector<aditof::Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        Panic("Could not find camera. ensure power and connection");
    }

    aditof::Camera *camera = cameras.front();
    status = camera->initialize();
    if (status != aditof::Status::OK) {
        Panic("Could not iniitalize camera. ensure power and connection");
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        Panic("no camera frame types. ensure power and connection");
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != aditof::Status::OK) {
        Panic("could not set camera frame type. ensure power and connection");
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        Panic("no camera modes available");
    }

    status = camera->setMode("medium");
    if (status != Status::OK) {
        Panic("could not set camera mode");
    }
    //////////////////////////////////////////////////////////////////////////

    aditof::Frame frame;
    struct timeval cam_s, cam_e;
    struct timeval fps_s, fps_e;



    int frame_ctr = 0;

    sprintf(ptr, "calculating...");
    gettimeofday(&fps_s, NULL); 
    //pthread_mutex_unlock(&data_lock);
    for (int i=0; i<2400; i++) {
        // calc time for loading frame data
        gettimeofday(&cam_s, NULL); 
        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            printf("skipped frame!");
        } else {
            // load the IR and DEPTH data from the camera into a new
            // pointer (will be freed / replaced on next call)
            u_int16_t *tmp;
            status = frame.getData(aditof::FrameDataType::RAW, &tmp);
            if (status != aditof::Status::OK) {
                Panic("Could not get frame data!\n");
            } else {
                data_ptr = (data_frame) ? data2 : data1;
                memcpy(data_ptr, tmp, 640*960*2);
                data_frame = !data_frame;
            }
        }
        gettimeofday(&cam_e, NULL); 
        gettimeofday(&fps_e, NULL); 


        // calcualte time since last fps capture
        double time_taken = (fps_e.tv_sec - fps_s.tv_sec) * 1e6; 
        time_taken = (time_taken + (fps_e.tv_usec - fps_s.tv_usec)) * 1e-6; 

        // calculate single frame time
        double frame_taken = (cam_e.tv_sec - cam_s.tv_sec) * 1e6; 
        frame_taken = (frame_taken + (cam_e.tv_usec - cam_s.tv_usec)) * 1e-6; 

        // count number of frames processed in a second
        frame_ctr++;
        // after 1 second update the display info
        if (time_taken >= 1.0) {
            printf(ptr, "%dfps S:%d,%s %d", frame_ctr, g_conf.sense, (g_conf.iron) ? "ON" :"OFF", g_conf.max_depth);
            //sprintf(ptr, "%dfps S:%d,%s %d", frame_ctr, g_conf.sense, (g_conf.iron) ? "ON" :"OFF", g_conf.max_depth);
            //printf("%s\n", ptr);
            // PTR NEEDS TO BE GLOBAL?
            frame_ctr = 0;
            gettimeofday(&fps_s, NULL); 
        }
    }

    fb_cleanup();
}

/**
 * handle ui button presses
 */
void ui_input(void *foo) {
    // get ui button
    uint btn = 1
    if (btn == BTN_SEL_UP) {
        edit_value = (edit_value -1) % MAX_UI_ELM;
        return;
    } else if (btn == BTN_SEL_DOWN) {
        edit_value = (edit_value +1) % MAX_UI_ELM;
        return;
    } else {
        // min Y
        if (edit_value == 1) {
            g_conf.bb_min_y += (btn == BTN_VAL_DOWN) -1 : +1;
        }
        // max Y
        else if (edit_value == 3) {
            g_conf.bb_max_y += (btn == BTN_VAL_DOWN) -1 : +1;
        }
        // min X
        else if (edit_value == 2) {
            g_conf.bb_min_x += (btn == BTN_VAL_DOWN) -1 : +1;
        }
        // max X
        else if (edit_value == 4) {
            g_conf.bb_max_x += (btn == BTN_VAL_DOWN) -1 : +1;
        }
        // min depth
        else if (edit_value == 5) {
            g_conf.min_depth += (btn == BTN_VAL_DOWN) -100 : +100;
            g_conf.mid_depth = g_conf.min_depth + 200;
        }
        // max depth
        else if (edit_value == 6) {
            g_conf.max_depth += (btn == BTN_VAL_DOWN) -100 : +100;
        }
        // number of vixels required to trigger
        else if (edit_value == 7) {
            g_conf.sense += (btn == BTN_VAL_DOWN) -1 : +1;
        }
    }
}


void *draw_vixels(void *foo) {
    uint vixels[g_conf.vixel_w * g_conf.vixel_h];
    bool last = data_frame;
    uint16_t *my_ptr = data_ptr;
    struct timeval draw_s, draw_e;


    // TODO: clean this part up..
    // skip 2 frames so both buffers are populated
    printf("waiting data frame 1\n");
    while(true) {
        if (data_frame == true) {
            usleep(10000);
            continue;
        }
        break;
    }
    printf("waiting data frame 2\n");
    while(true) {
        if (data_frame == false) {
            usleep(10000);
            continue;
        }
        break;
    }
    printf("waiting data frame 3\n");
    while(true) {
        if (data_frame == true) {
            usleep(10000);
            continue;
        }
        break;
    }

    printf("running ...\n");
    last = data_frame;
    while(running) {
        // wait for the next frame to be available...
        if (data_frame == last) {
            usleep(1000);
            continue;
        }

        // get a pointer to the just updated frame
        my_ptr = (data_frame) ? data1 : data2;
        last = data_frame;
        
        // draw the background IR data
        if (g_conf.iron) {
            draw_image_half_slow(TOF_WIDTH, TOF_FULL_HEIGHT, 0, 0, my_ptr, buffer);
        }

        // draw the foreground DEPTH data
        if (g_conf.depthon) {
            gettimeofday(&draw_s, NULL); 
            // clear out the vixel averaging data
            bzero(vixels, g_conf.vixel_w * g_conf.vixel_h);
            for (uint vy=0; vy<g_conf.vixel_h; vy++) {

                for (uint vx=0; vx<g_conf.vixel_w; vx++) {

                    int offt = vy * VIXEL_SZ * TOF_WIDTH;
                    int x = vx * VIXEL_SZ;
                    uint run = 0;
                    int voff = = vy * g_conf.vixel_w + vx;
                    for (uint h = VIXEL_SZ; h != 0; h--) {
                        run += my_ptr[offt + x];
                        run += my_ptr[offt + x + 1];
                        run += my_ptr[offt + x + 2];
                        run += my_ptr[offt + x + 3];
                        run += my_ptr[offt + x + 4];
                        run += my_ptr[offt + x + 5];
                        run += my_ptr[offt + x + 6];
                        run += my_ptr[offt + x + 7];
                        vixels[voff] += run/8;
                        offt += 640;
                    }
                    vixels[voff] /= 8;

                    // only draw vixels that are inside the view range
                    if (vixels[voff] >= 0 && vixels[voff] < g_conf.max_depth) {
                        //int diff = 12000 - 2005;
                        if (vixels[voff] < g_conf.min_depth) { vixels[voff] = g_conf.min_depth; }
                        u_int16_t color = ((g_conf.max_depth - vixels[voff]) / 39) << 5;

                        if (color > GREEN) { color = GREEN; }
                        if (color < 32) { color = 32; }

                        u_int8_t sz = 4;

                        // draw red vixels
                        if ((vixels[voff] <= g_conf.min_depth) && (vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel((vx*4)-2, (vy*4)-2, 4, 4, RED, buffer);
                        }
                        // draw green vixels
                        else {
                            fb_vixel((vx*4)-(sz/2), (vy*4)-(sz/2), sz, sz, color, buffer);
                        }
                    }

                    // draw the bounding box
                    if (g_conf.bbon) {
                        // draw each line separately and highlight the selected  WHITE 
                        if ((vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy == g_conf.bb_min_y)) {
                            fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 1) ? WHITE : RED, buffer);
                        }
                        else if ((vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy == g_conf.bb_max_y)) {
                            fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 3) ? WHITE : RED, buffer);
                        }
                        else if ((vx == g_conf.bb_min_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 2) ? WHITE : RED, buffer);
                        }
                        else if ((vx == g_conf.bb_max_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 4) ? WHITE : RED, buffer);
                        }
                    }
                }
            }
            gettimeofday(&draw_e, NULL); 
            double frame_taken = (draw_e.tv_sec - draw_s.tv_sec) * 1e6; 
            frame_taken = (frame_taken + (draw_e.tv_usec - draw_s.tv_usec)) * 1e-6; 
            printf("DRAW: [%f]sec\n", frame_taken);
        }

        //fb_write_string(ptr, 0, 205, -1, buffer);
        //sprintf(ptr, "%dfps S:%d,%s %d", frame_ctr, g_conf.sense, (g_conf.iron) ? "ON" :"OFF", g_conf.max_depth);

        fb_write_string(sprintf("S:%d", g_conf.sense), 0, 210, (edit_value == 7) ? YELLOW : -1, buffer); 

        memcpy(fbm, buffer, fb_info.buffer_sz);

        // after we render the buffer, we clear it for the next frame
        bzero(buffer, fb_info.buffer_sz);
    }
}