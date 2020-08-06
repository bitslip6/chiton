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
#include "image.h"
#include "util.h"
#include "system.h"
#include "webserver.h"

#include <stdio.h>
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

std::string GMODE;
GlobalOpts *GOPTS;
DisplayBuffers *GDISP;
time_t __HANDY_BENCH;
struct timeval __UTIL_T1, __UTIL_T2;


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
int setting_fn(std::vector<std::string> params, int socket) {
    std::cout << "call setting with: " << params[0] << std::endl;
    std::string data;
    int resp = 404;
    // change color rotation
    if (params[0].compare(std::string("degree")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            GOPTS->color_rotation = atof(params[2].c_str());
            resp = 200;
        }
        data = "{\"value\":"+std::to_string(GOPTS->color_rotation)+"}";
    }

    // change camera enable
    if (params[0].compare(std::string("camera")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            GOPTS->enable = (params[2].compare("enable") == 0);
            resp = 200;
        }
        data = "{\"value\":"+std::to_string(GOPTS->enable)+"}";
    }

    // change camera depth/ir
    if (params[0].compare(std::string("depth")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            GOPTS->depth_frame = (params[2].compare("ir") == 0);
            resp = 200;
        }
        data = "{\"value\":"+std::to_string(GOPTS->depth_frame)+"}";
    }

    // change the camera mode
    if (params[0].compare(std::string("mode")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            resp = 200;
            GOPTS->depth_frame = (params[2].compare("depth") == 0);
            std::cout << "de: " << GOPTS->depth_frame << std::endl;
        }
        data = "{\"value\":"+std::to_string(GOPTS->depth_frame)+"}";
    }


    // change camera max value (used in hsv conversion)
    if (params[0].compare(std::string("max")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            resp = 200;
            GOPTS->max_cam_value = std::stoi(params[2]);
            RGB8 c;
            uint16_t t=1;
            depth_to_hsv(&t, &c, true);
            printf("set max cam value to %d\n", GOPTS->max_cam_value);
        }
        data = "{\"value\":"+std::to_string(GOPTS->max_cam_value)+"}";
    }


    std::string raw = "HTTP/1.1 "+std::to_string(resp)+" OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        +std::to_string(data.length()) + "\n\n" + data;
    send_headers(raw, socket);

    return resp;
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

	/*
    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        LOG(ERROR) << "no camera modes available!";
        return 0;
    }
	*/

    std::string lastmode = "far";
    //bool lastmode = GOPTS->depth_frame;
    status = camera->setMode(lastmode);//modes.front());
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not set camera mode!";
        return 0;
    }

    aditof::Frame frame;

	std::cout << "just set mode" << std::endl;
    
	//std::cout << " --  set mode: " << GOPTS->depth_frame << std::endl;
	//lastmode = GOPTS->depth_frame;
	//status = camera->setMode("medium");

    while(GOPTS->running) {


		// std::cout << "r frame" << std::endl;
        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not request frame!";
            return 0;
        }

		std::cout << "raw toggle" << std::endl;
        if (raw_toggle) {
            while (dlock == 1) { usleep(10000); printf("dlock 1!\n"); }
            status = frame.getData((GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR, &raw_data1);
        } else {
            while (dlock == 2) { usleep(10000); printf("dlock 2!\n"); }
            status = frame.getData((GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR, &raw_data2);
        }

        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not get frame data!";
            return 0;
        }

		// std::cout << "raw toggle enable" << std::endl;
        raw_data_wptr = (raw_toggle) ? raw_data1 : raw_data2; 
        jpeg_data_wptr = (raw_toggle) ? jpeg_data1 : jpeg_data2;
        convert_to_rgb(raw_data_wptr, samples, 640*480*2, 2);


		//std::cout << "raw toggle disable" << std::endl;
        if (raw_toggle) {
            while (jlock == 1) { usleep(5000); printf("jlock 1!\n"); }
			if (jpeg_data1 != NULL) { printf("free 1\n"); free(jpeg_data1); }
			else { printf("no free 1\n"); }
			long foo = (long)&jpeg_data1;
			//printf("jpeg data1 [%d] [%d] [%d]\n", jpeg_data1, &jpeg_data1, foo);
        	write_jpeg(samples,foo, &jlenptr);
        	jpeg_data_rptr = jpeg_data1;
        } else {
            while (jlock == 2) { usleep(5000); printf("jlock 2!\n"); }
			if (jpeg_data2 != NULL) { printf("free 2\n"); free(jpeg_data2); }
			else { printf("no free 2\n"); }
			long foo = (long)&jpeg_data2;
        	write_jpeg(samples, foo, &jlenptr);
        	jpeg_data_rptr = jpeg_data2;
        }


		//std::cout << "write jpeg" << std::endl;

        raw_data_rptr = raw_data_wptr;
        jlen2 = jlenptr;


        raw_toggle = !raw_toggle;
        ready = true;
		usleep(50000);
    }
    ready = true;
    while (GOPTS->running) {
        
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
    GMODE = "far";
    samples = (unsigned char *)calloc(640*480*sizeof(uint16_t), 3);
    jpeg_data_wptr = jpeg_data2;
    jlenptr = jlen1;
    depth_to_hsv(&t, &c, true);

	//jpeg_data1 = (unsigned char*)NEWMEM(u8, 1024*1024);
	//jpeg_data2 = (unsigned char*)NEWMEM(u8, 1024*1024);

    // start the camera thread
    std::thread camera_thread(run_camera);
        
    printf("waiting for camera to start ...\n");
    while(!ready) {
        usleep(50000);
    }
    printf("starting web server ...\n");

    std::map<std::string, IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["setting"] = setting_fn;
	printf("web server start\n");
    web_server(PORT, fnMap);
	printf("web server finish!\n");
    camera_thread.join();
    printf("server stopped! ...\n");
}
