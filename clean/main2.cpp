
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

// #include "projection.h"
#include "geometry.h"
#include "main.h"
#include "font.h"

// #include <wiringPi.h>
// #include <softPwm.h>
#include <linux/i2c-dev.h>

#include <png.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include <pthread.h>


using namespace aditof;

/*

import RPi.GPIO as GPIO
import time

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD)

GPIO.setup(12, GPIO.OUT)
pwm = GPIO.PWM(12, 25000)
while True:
    pwm.start(99)
    time.sleep(5);
*/

// TODO: clear out all of these global variables....
// pointer to adi frame1
uint16_t *data1;
// pointer to adi frame2
uint16_t *data2;
// pointer to current_frame (data1 or data2)
uint16_t *data_ptr;


char condition = 'X';


u_int16_t *buffer;
u_int16_t *fbm;

// pointer to text drawing buffer
char text_buff[25];
char ui_buff[25];
ScreenInfo fb_info;
bool data_frame = true;

// the demo thread  will run until the demo lock is released
pthread_mutex_t demo_lock;

// the global config data
// TODO: rename DemoConf
// TODO: remove from global variable
DemoConf g_conf;

int edit_value = 0;

int pipe_fd[2];

// global flag for all systems.  when set to false program should exit
bool running = true;


// defined lower down. thread that continusly draws 
// and calculated vixel data
void *draw_vixels(void *foo);

// driving thread
void *drive_and_stop(void *foo);

// render routine
void draw_depth_data(u_int16_t *raw, u_int16_t *buffer);

// display en error message on stderr and exit with an
// exit value of 1
void Panic(const char *msg) {
    perror(msg);
    exit(1);
}

/**
 * read a digital pin with 5ms debounce time
 * reads until read values match with 5ms retry and 10 retrys
 */
int debounce_read(int pin) {
    int ctr, val = 10;

    while (ctr-- != 0) {
        val = digitalRead(pin);
        usleep(5000);
        if (val == digitalRead(pin)) {
            return val;
        }
        usleep(5000);
    }
    return val;
}

thread_local unsigned int seed = time(NULL); 

/**
 * blocks and turns a random direction with no leg or wall
 */
void random_direction() {

	// pick a random direction to turn
	//TODO:del int direction = rand_r(&seed) % 2;
	if (rand_r(&seed) % 2 == 1) {
		Left();
	} else {
		Right();
	}
	// turn for a bit...
	int min_turn_time = 700000 + ((rand_r(&seed) % 5) * 600000);
	usleep(min_turn_time);

	// keep turning until no wall or leg is found...
	int max_wait = 400;
	while(condition == 'W' || condition == 'L' && max_wait > 0) {
		usleep(20000);
		max_wait--;
	}

    // Walls everywhere!
	if (max_wait <= 0) {
        sprintf(text_buff, "unable to move");
		sleep(5);
        return;
	}

    // ahh, nothinng in front
    // turn just a bit more to get that out of the way
	usleep(800000);

	// stop and return
	AllStop();
}



/**
 * drive arround randomly and dont bump into things forever
void *random_drive(void *foo) {
    close(pipe_fd[1]);
    borg_init();

    sprintf(text_buff, "camera wait");
    while(condition == 'W') {
    	usleep(200000);
    }
 */
void *random_drive(void *foo) {
    close(pipe_fd[1]);
    borg_init();

    sprintf(text_buff, "camera wait");
    while(condition == 'W') {
    	usleep(200000);
    }

    // wait for button press
	sprintf(text_buff, (char  *)"press button");
	while(1) {
        if (debounce_read(17) == 0) {
            break;
        }
		usleep(100000);
	}

    // 3 second count down
    for (int i = 3; i != 0; i--) {
	    sprintf(text_buff, (char  *)"Run %d", i);
	    usleep(1000000);
    }

    // loop until running loop ends
    while(running) {
		for(int i=0; i<1000; i++) {
			usleep(10000);

			if (condition == 'W' || condition == 'L') {
				int j = 20;
				AllStop();
				// wait for the thing to get out of the way...
				while (condition == 'W' || condition == 'L' && j > 0) {
				    sprintf(text_buff, (char  *)"wait: %d", j);
					usleep(200000);
					// TODO: DO WE NEED THIS? AllStop();
					j--;
				}

				// it didn't get out of theway...
				if (j <= 0) {
					sprintf(text_buff, (char  *)"Redirect");
					random_direction();
                    // we found a new direction to move in, reset walk counter
					i = 1000;
				}
			}

            sprintf(text_buff, (char *)"Fwd %s", (condition == 'X') ? "fast" : "slow");
            Fwd(condition);
		}
		sprintf(text_buff, (char  *)"Redirect");
		random_direction();
	}

    AllStop();
}


/**
 * connect the object detection pipe
 */
void connect_pipe() {
    if (pipe(pipe_fd) == -1) {
        Panic(70, "unable to create unnamed IPC pipe");
    }
    printf("communication pipe created\n");
    write(pipe_fd[1], "S", 1);
}

/**
 * configure GPIO pins for button presses
 */
void setup_buttons() {

    wiringPiSetupGpio() ;
	printf("wiring pi setup\n");
    //pwmWrite(12, 100);
    //softPwmCreate (18, 500, 1024) ;
	//printf("backlight set to 500\n");

	pinMode(17, INPUT);
	printf("button 1 configured\n");
	pinMode(18, INPUT);
	printf("button 2 configured\n");
	pinMode(19, INPUT);
	printf("button 3 configured\n");
	pinMode(20, INPUT);
	printf("button 4 configured\n");
}


void system_init(int argc, char *argv[]) {
    // google logging for ADI code (YUCK!)
    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;
    (void)argc;

    condition = 'W';
    connect_pipe();
	setup_buttons();

    // the global config data
    // TODO, add read and write config file support.
    g_conf = { true, true,  true, true, true, true, 5, 2000,  391,  950,  80,  560,  60,  320,  VIXEL_W,  VIXEL_H };

    int opt;
    while((opt = getopt(argc, argv, "ibfg")) != -1) {
        switch(opt) {
            case 'i':
                g_conf.iron = true;
                break;
            case 'b':
                g_conf.bbon = true;
                break;
            case 'f':
                g_conf.fpson = true;
                break;
            case 'g':  
                g_conf.flooron = true; 
                break;  
        }
    }
    
    const char *fb_device = "/dev/fb1";
    const char *font_file = "consolas.png";
    fbm = fb_init(fb_device, font_file);
    fb_info = fb_get_buffer_info();
    buffer = (u_int16_t *)malloc(fb_info.buffer_sz*2);
    data1 = (u_int16_t *)malloc(640*960*2);
    data2 = (u_int16_t *)malloc(640*960*2);
    bzero(buffer, fb_info.buffer_sz);
    if (fbm == NULL || buffer == NULL) {
        Panic(71, "unable to allocate famebuffer or virtual buffer");
    }
    printf("The framebuffer and virtual buffer mapped successfully.\n");
    printf("allocated [%u] bytes for double buffer\n", fb_info.buffer_sz);


}


//
// program entry point
int main(int argc, char *argv[]) {


    system_init(argc, argv);

    // TODO: command line args...
    pthread_t draw_thread, ui_thread, drive_thread;

    // TODO: move this to a function that returns a
    // connected camera
    // Map the device to memory and frame buffer
        // TODO: change background logo to demo...
    display_image("logo.png");
    //pthread_mutex_lock(&demo_lock);

    // fork the threads for driving and drawing...
    pthread_create(&draw_thread, NULL, draw_vixels, NULL);
    pthread_create(&drive_thread, NULL, random_drive, NULL);
    pthread_create(&ui_thread, NULL, ui_input, NULL);


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
    //////////////////////////////////////////////////////////////////////////


    int fps, frame_ctr = 0;
    aditof::Frame frame;
    struct timeval fps_s, fps_e;


    // loop until !running and calculate FPS
    gettimeofday(&fps_s, NULL); 
    while (running) {

        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            printf("skipped frame!\n");
            sprintf(text_buff, "frame skip");
            usleep(5000);
        } else {
            // load the IR and DEPTH data from the camera into a new
            // pointer (will be freed / replaced on next call)
            u_int16_t *tmp;
            status = frame.getData(aditof::FrameDataType::RAW, &tmp);
            if (status != aditof::Status::OK) {
                Panic(87, "Could not get frame data!\n");
            } else {
                data_ptr = (data_frame) ? data2 : data1;
                memcpy(data_ptr, tmp, 640*960*2);
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
            fps = frame_ctr;
            frame_ctr = 0;
            gettimeofday(&fps_s, NULL); 
        }
    }

    fb_cleanup();
    free(buffer);
    free(data1);
    free(data2);
    printf("application finished\n");
}


void wait_for(BoolCallback *func, uint usec) {
    while(true) {
        if (!func()) {
            usleep(usec);
            continue;
        }
        break;
    }
}

bool dataframe_true() { return data_frame == true; }
bool dataframe_false() { return data_frame == false; }


void *draw_vixels(void *foo) {
    //uint vixels[g_conf.vixel_w * g_conf.vixel_h];
    uint vixels[VIXEL_W * VIXEL_H];
    bool last = data_frame;
    u_int16_t *my_ptr = data_ptr;
    struct timeval draw_s, draw_e;

    // silence unused warnings...
    if (1 == 2) { printf("%s\n", (char *)foo); }


    // TODO: clean this part up..
    // skip 2 frames so both buffers are populated
    printf("waiting for 3 data frames\n");
    wait_for(dataframe_true, 10000);
    wait_for(dataframe_false, 10000);
    wait_for(dataframe_true, 10000);
    printf("running ...\n");

    //close(pipe_fd[0]);

    last = data_frame;
    while(running) {
        // wait for the next frame to be available...
        if (data_frame == last) {
            usleep(10000);
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

            draw_depth_data(my_ptr, buffer);
            /*
            // clear out the vixel averaging data
            bzero(vixels, g_conf.vixel_w * g_conf.vixel_h);
            for (uint vy=0; vy<g_conf.vixel_h; vy++) {

                for (uint vx=0; vx<g_conf.vixel_w; vx++) {

                    int offt = (640*480) + (vy * VIXEL_SZ * TOF_WIDTH);
                    int x = vx * VIXEL_SZ;
                    uint run = 0;
                    int voff = vy * g_conf.vixel_w + vx;
                    for (uint h = VIXEL_SZ; h != 0; h--) {
                        run += my_ptr[offt + x];
                        run += my_ptr[offt + x + 1];
                        run += my_ptr[offt + x + 2];
                        run += my_ptr[offt + x + 3];
                        //run += my_ptr[offt + x + 4];
                        //run += my_ptr[offt + x + 5];
                        //run += my_ptr[offt + x + 6];
                        //run += my_ptr[offt + x + 7];
                        vixels[voff] += run/VIXEL_SZ;
                        offt += 640;
                    }
                    vixels[voff] /= VIXEL_SZ;

                    // only draw vixels that are inside the view range
                    if (vixels[voff] > 0 && vixels[voff] < g_conf.max_depth) {
                        //int diff = 12000 - 2005;
                        if (vixels[voff] < g_conf.min_depth) { vixels[voff] = g_conf.min_depth; }
                        u_int16_t color = ((g_conf.max_depth - (vixels[voff])) / 36) << 5;

                        if (color >= GREEN) { color = GREEN | 0x3800 | 0x7; } //0b1111100000000000; }
                        if (color < 32) { color = 32; }

                        u_int8_t sz = VIXEL_HALF;

                        // draw red vixels
                        if ((vixels[voff] <= g_conf.min_depth) && (vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel((vx*VIXEL_HALF)-VIXEL_HALF2, (vy*VIXEL_HALF)-VIXEL_HALF2, VIXEL_HALF, VIXEL_HALF, RED, buffer);
                        }
                        // draw green vixels
                        else {
                            fb_vixel((vx*VIXEL_HALF)-(sz/VIXEL_HALF2), (vy*VIXEL_HALF)-(sz/VIXEL_HALF2), sz, sz, color, buffer);
                        }
                    }

                    // draw the bounding box
                    if (g_conf.bbon) {
                        // draw each line separately and highlight the selected  WHITE 
                        if ((vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy == g_conf.bb_min_y)) {
                            fb_vixel_add((vx*VIXEL_HALF)-VIXEL_HALF2, (vy*VIXEL_HALF)-VIXEL_HALF2, VIXEL_HALF, VIXEL_HALF, (edit_value == 1) ? WHITE : RED, buffer);
                            //fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 1) ? WHITE : RED, buffer);
                        }
                        else if ((vx >= g_conf.bb_min_x && vx <= g_conf.bb_max_x) && (vy == g_conf.bb_max_y)) {
                            fb_vixel_add((vx*VIXEL_HALF)-VIXEL_HALF2, (vy*VIXEL_HALF)-VIXEL_HALF2, VIXEL_HALF, VIXEL_HALF, (edit_value == 3) ? WHITE : RED, buffer);
                            //fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 3) ? WHITE : RED, buffer);
                        }
                        else if ((vx == g_conf.bb_min_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel_add((vx*VIXEL_HALF)-VIXEL_HALF2, (vy*VIXEL_HALF)-VIXEL_HALF2, VIXEL_HALF, VIXEL_HALF, (edit_value == 2) ? WHITE : RED, buffer);
                            //fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 2) ? WHITE : RED, buffer);
                        }
                        else if ((vx == g_conf.bb_max_x) && (vy >= g_conf.bb_min_y && vy <= g_conf.bb_max_y)) {
                            fb_vixel_add((vx*VIXEL_HALF)-VIXEL_HALF2, (vy*VIXEL_HALF)-VIXEL_HALF2, VIXEL_HALF, VIXEL_HALF, (edit_value == 4) ? WHITE : RED, buffer);
                            //fb_vixel_add((vx*4)-2, (vy*4)-2, 4, 4, (edit_value == 4) ? WHITE : RED, buffer);
                        }
                    }
                }
            }
                */

            gettimeofday(&draw_e, NULL); 
            double frame_taken = (draw_e.tv_sec - draw_s.tv_sec) * 1e6; 
            frame_taken = (frame_taken + (draw_e.tv_usec - draw_s.tv_usec)) * 1e-6; 
            // printf("DRAW: [%f]sec\n", frame_taken);
        }

        //fb_write_string(ptr, 0, 205, -1, buffer);
        //sprintf(ptr, "%dfps S:%d,%s %d", frame_ctr, g_conf.sense, (g_conf.iron) ? "ON" :"OFF", g_conf.max_depth);

        fb_write_string(text_buff, 0, 210, (edit_value == 7) ? YELLOW : -1, buffer);
        fb_write_string(ui_buff, 0, 10, BLUE, buffer);
        // wait for func to return true, spin wait and test every usec seconds

        memcpy(fbm, buffer, fb_info.buffer_sz);
        // after we render the buffer, we clear it for the next frame
        bzero(buffer, fb_info.buffer_sz);
    }

    return NULL;
}

void draw_depth_data(u_int16_t *src, u_int16_t *buffer) {

    int dst_off, src_off = 0;
    dst_off = 0;//PIXEL_OFF(dx, dy);
    uint min = 0;
    uint max = 0;

    uint min_ctr = 0;
    uint mid_ctr = 0;
    for (uint y = 0; y < 480; y += 2)
    {

        // skip 1 vertical line in src
        src_off = (640 * y);

        for (uint x = 0; x < 640; x += 2)
        {
            //if (src[src_off + x] < min) { min = src[src_off+x]; }
            //if (src[src_off + x] > max) { max = src[src_off+x]; }

            uint col = src[src_off + x];
            col += src[src_off + x + 1];
            col += src[src_off + x + 640];
            col += src[src_off + x + 640 + 1];
            col /= 4 ;
            //col /= 4; // change 0-2048 to 0-32 and average
            //printf("[%d]\n", col);

            //col &= 0b11111000;
            //col = col >> 3;
            //if (col < 0) { col = BLUE + 1; }
            //if (col > BLUE)
            //{
            //    col = BLUE;
            //}

            if (col < g_conf.max_depth) {
				buffer[dst_off] = ((((g_conf.max_depth - col) / 26)) << 5) & 0b11111100000;

                if (col < g_conf.min_depth) {
                    if (x > g_conf.bb_min_x && x < g_conf.bb_max_x && y > g_conf.bb_min_y && y < g_conf.bb_max_y) {
                        buffer[dst_off] = (buffer[dst_off] >> 2) | RED;//(64 - ((col - g_conf.min_depth + 64)/ 26)) << 11;
                        //printf("min depth %d / [%d] MIN %d, MAX %d\n", src[src_off+x], g_conf.min_depth, min, max);
                        min_ctr++;
                    }
                } else if (col < g_conf.mid_depth) {
                    if (x > MIN_X && x < MAX_X && y > MIN_Y && y < MAX_Y) {
                        //buffer[dst_off] = (buffer[dst_off] >> 2) | RED;//(64 - ((col - g_conf.min_depth + 64)/ 26)) << 11;
                        //printf("min depth %d / [%d] MIN %d, MAX %d\n", src[src_off+x], g_conf.min_depth, min, max);
                        mid_ctr++;
                    }
                }
            } else {
                if (col > g_conf.max_depth) {
                    //printf("max depth %d [%d]\n", col, g_conf.max_depth);
                }
            }

            if (g_conf.bbon) {
                // draw each line separately and highlight the selected  WHITE 
                if ((x >= g_conf.bb_min_x && x <= g_conf.bb_max_x) && (y == g_conf.bb_min_y)) {
                    buffer[dst_off] |= RED;
                }
                else if ((x >= g_conf.bb_min_x && x <= g_conf.bb_max_x) && (y == g_conf.bb_max_y)) {
                    buffer[dst_off] |= RED;
                }
                else if ((x == g_conf.bb_min_x) && (y >= g_conf.bb_min_y && y <= g_conf.bb_max_y)) {
                    buffer[dst_off] |= RED;
                }
                else if ((x == g_conf.bb_max_x) && (y >= g_conf.bb_min_y && y <= g_conf.bb_max_y)) {
                    buffer[dst_off] |= RED;
                }
            }


            dst_off++;
        }

        // skip 1 vertical line in dst
        dst_off += FB_WIDTH - (640 / 2);
    }
    /*if (min_ctr > 20000 || mid_ctr > 20000) {
        write(pipe_fd[1], "W", 1);
        condition = 'W';
        //printf(" write: W\n");
        fb_write_string((char *)"WALL", 1, 1, 30, buffer);
    } else */
    if (min_ctr > 1300) {
        write(pipe_fd[1], "L", 1);
        condition = 'L';
        //printf(" write: L\n");
        fb_write_string((char *)"LEG", 1, 1, 30, buffer);
    } else if (mid_ctr > 5000) {
        write(pipe_fd[1], "C", 1);
        //printf(" write: C\n");
        condition = 'C';
    }
}
