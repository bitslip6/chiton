/******************************************************************************
 *
 * @file            main.cpp
 *
 * @brief           Main Chiton application front-end.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 SWYM, LLC and BitSlip6. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
#include "app.hpp"

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>


#include "system.h"

#define PORT 80 

DemoConf DCONF;
uint16_t *raw_data1;
uint16_t *raw_data2;
uint16_t *raw_data_wptr;
uint16_t *raw_data_rptr;
bool raw_toggle = false;
bool ready = false;
unsigned char *jpeg_data1 = NULL;
unsigned char *jpeg_data2 = NULL;
unsigned char *jpeg_data_wptr = NULL;
unsigned char *jpeg_data_rptr = NULL;

unsigned char *samples;

unsigned long jlen1 = 0;
unsigned long jlen2 = 0;
unsigned long jlenptr = 0;
unsigned long rlen1 = 0;
unsigned long rlen2 = 0;
unsigned long rlenptr = 0;
int jlock = 0;
int dlock = 0;

std::mode GMODE;

typedef int (*IntCallbackString)(std::vector<std::string>, int);

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------





/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket)
{
    if (params[0].compare(std::string("enable")) == 0) {
        GOPTS->enable = true;
        return 200;
    }
    if (params[0].compare(std::string("disable")) == 0) {
        GOPTS->enable = false;
        return 200;
    }
    if (params[0].compare(std::string("image")) == 0) {
        jlock = (raw_toggle) ? 1 : 2;
        bool okay = send_data(socket, std::string("image/jpeg"), jpeg_data_rptr, jlenptr);
        if (!okay) { printf("unable to send jpeg data\n"); }
        jlock = 0;
        return 0;
    }
    if (params[0].compare(std::string("raw")) == 0) {
        dlock = (raw_toggle) ? 1 : 2;
        bool okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) raw_data_rptr, 640*480*2);
        if (!okay) { printf("unable to send raw data\n"); }
        dlock = 0;
        return 0;
    }

    return 404;
}

/**
 * get the value of a pixel in the current data pointer 
 */
int value_fn(std::vector<std::string> params, int socket)
{
    if (params.size() < 4) {
        std::cout << "invalid request, get/value requires 2 additional parameters" << std::endl;
        return 500;
    }
    int x = std::stoi(params[2]);
    int y = std::stoi(params[3]);
    if (x>640||x<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 600" << std::endl;
        return 500;
    }
    if (y>480||y<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 480" << std::endl;
        return 500;
    }

    int offset = (y*640)+x;
    uint16_t v = (raw_toggle) ? raw_data1[offset] : raw_data2[offset];
    std::string resp = "{\"value\":"+std::to_string(v)+"}";

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(resp.length()) + "\n\n" + resp;

    send_headers(raw, socket);
    return 200;
}


/**
 * TODO: always return string data here ...
 */
std::string setting_fn(std::vector<std::string> params, int socket) {
    std::cout << "call setting with: " << params[0] << std::endl;
    std::string resp;
    // change color rotation
    if (params[0].compare(std::string("degree")) == 0) {
        if (params.size() > 2) {
            GOPTS->color_rotation = atof(params[2].c_str());
        }
        resp = "{\"value\":"+std::to_string(GOPTS->color_rotation)+"}";
    }

    // change camera enable
    if (params[0].compare(std::string("camera")) == 0) {
        if (params.size() > 2) {
            GOPTS->enable = (params[2].compare("enable") == 0);
        }
        resp = "{\"value\":"+std::to_string(GOPTS->enable)+"}";
    }

    // change camera depth/ir
    if (params[0].compare(std::string("depth")) == 0) {
        if (params.size() > 2) {
            GOPTS->depth_frame = (params[2].compare("ir") == 0);
            GOPTS->type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
        }
        resp = "{\"value\":"+std::to_string(GOPTS->depth_frame)+"}";
    }

    // change the camera mode
    if (params[0].compare(std::string("mode")) == 0) {
        if (params.size() > 2) {
            GMODE = params[2].copy();
            std::cout << "de: " << GOMODEPTS->mode << std::endl;
        }
        resp = "{\"value\":"+std::to_string(GOPTS->mode)+"}";
    }


    // change camera max value (used in hsv conversion)
    if (params[0].compare(std::string("max")) == 0) {
        if (params.size() > 2) {
            GOPTS->max_cam_value = std::stoi(params[2]);
            RGB8 c;
            uint16_t t=1;
            depth_to_hsv(&t, &c, true);
            printf("set max cam value to %d\n", GOPTS->max_cam_value);
        }
        resp = "{\"value\":"+std::to_string(GOPTS->max_cam_value)+"}";
    }


    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        +std::to_string(resp.length()) + "\n\n" + resp;
    send_headers(raw, socket);

    return 200;
}





/**
 * run the camera and convert data into jpeg data
 */
int run_camera() { 
    google::InitGoogleLogging("testapp");
    FLAGS_alsologtostderr = 1;

    aditof::Status status = aditof::Status::OK;

    aditof::System system;
    status = system.initialize();
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not initialize system!";
        return 0;
    }

    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        LOG(WARNING) << "No cameras found";
        return 0;
    }

    auto camera = cameras.front();
    
    status = camera->initialize();
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not initialize camera!";
        return 0;
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        std::cout << "no frame type avaialble!";
        return 0;
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not set camera frame type!";
        return 0;
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        LOG(ERROR) << "no camera modes available!";
        return 0;
    }

    std::string lastmode = "far";
    status = camera->setMode(lastmode);//modes.front());
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not set camera mode!";
        return 0;
    }

    aditof::Frame frame;

    while(GOPTS->running) {

        if (lastmode.compare(GOPTS->mode) != 0) {
            std::cout << " --  set mode: " << GOPTS->mode << std::endl;
            lastmode = GOPTS->mode;
            status = camera->setMode(lastmode);
        }

        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not request frame!";
            return 0;
        }

        if (raw_toggle) {
            while (dlock == 1) { usleep(10000); printf("dlock 1!\n"); }
            status = frame.getData(GOPTS->type, &raw_data1);
        } else {
            while (dlock == 2) { usleep(10000); printf("dlock 2!\n"); }
            status = frame.getData(GOPTS->type, &raw_data2);
        }

        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not get frame data!";
            return 0;
        }

        raw_data_wptr = (raw_toggle) ? raw_data1 : raw_data2; 
        jpeg_data_wptr = (raw_toggle) ? jpeg_data1 : jpeg_data2;
        convert_to_rgb(raw_data_wptr, samples, 640*480*2, 2);

        if (raw_toggle) {
            while (jlock == 1) { usleep(5000); printf("jlock 1!\n"); }
        } else {
            while (jlock == 2) { usleep(5000); printf("jlock 2!\n"); }
        }

        write_jpeg(samples, &jlenptr);

        raw_data_rptr = raw_data_wptr;
        jpeg_data_rptr = jpeg_data_wptr;
        jlen2 = jlenptr;


        raw_toggle = !raw_toggle;
        ready = true;
    }

    return 1;
}



int main()
{
    RGB8 c;
    uint16_t t=1;
    SystemInit(320, 240);
    GOPTS->max_cam_value = 3500;
    GOPTS->min_cam_value = 0;
    GOPTS->depth_frame = true;
    GOPTS->quality = 95;
    GOPTS->type = aditof::FrameDataType::DEPTH;
    GOPTS->mode = "far";
    samples = (unsigned char *)calloc(640*480*sizeof(uint16_t), 3);
    jpeg_data_wptr = jpeg_data2;
    jlenptr = jlen1;
    depth_to_hsv(&t, &c, true);

    // start the camera thread
    std::thread camera_thread(init_camera);
        
    printf("waiting for camera to start ...\n");
    while(!ready) {
        usleep(50000);
    }
    printf("starting web server ...\n");
    web_server();
    camera_thread.join();
    printf("server stopped! ...\n");
}
