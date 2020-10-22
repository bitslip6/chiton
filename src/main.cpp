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
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <unistd.h>
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
#include "app.hpp"
#include "image.h"
#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "system.hpp"
#include "webserver.h"


#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
 
  
#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>

#include "system.h"
#define PORT 80 

DemoConf DCONF;
std::mutex raw_data_mtx;
std::shared_ptr<aditof::Camera> camera;
uint16_t *copy;
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

// TODO: move to common file for testing.  remove dead code
std::string GMODE;
GlobalOpts *GOPTS;
DisplayBuffers *GDISP;
time_t __HANDY_BENCH;
struct timeval __UTIL_T1, __UTIL_T2;


float get_vixel_temp_c() {
    char buf[BUFSIZ];
    memset(buf, 0, 10);
    int ret = utils::i2c_read_close(utils::i2c_open(2, 0x49), buf, 3);
    if (ret == 3) {
        return utils::tmp108_to_c(utils::tmp108_convert(buf));
    }

    return 0.0f;
}

float get_cpu_temp_c() {
    std::ifstream thermalfile ("/sys/class/thermal/thermal_zone0/temp");
    if (thermalfile.is_open()) {
        std::string line;
        std::getline(thermalfile, line);
        int temp = std::stoi(line);
        thermalfile.close();
        return temp/1000.0f;
    }

    return 0.0f;
}




/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket) {
    //std::cout << "camera fn begin" << std::endl;
    
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
        //std::cout << "camera image send" << std::endl;
        bool okay = send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr);
        jlock = 0;
        //std::cout << "send data" << okay << std::endl;
        if (!okay) { printf("unable to send jpeg data\n"); return -1; }
        return 0;
    }
    if (params[0].compare(std::string("raw")) == 0) {
        dlock = 1;//(raw_toggle) ? 1 : 2;
        bool okay = false;
        raw_data_mtx.lock();
        printf("SEND DATA 1\n");
        okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) copy, 640*480*2);
        printf("SEND DATA 2\n");
        raw_data_mtx.unlock();
        dlock = 0;
        /*
        if (max_wait > 0) {
        } else {
            okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) raw_data1, 0);
        }
        */
            /*
        if (max_wait > 0 && raw_toggle) {
            okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) raw_data_rptr, 640*480*2);
        } else if (max_wait > 0) {
            okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) raw_data2, 640*480*2);
        }
        */
        // TODO: need to iterate over all raw_boxes lines and 
        //auto raw_boxes = calculate_box(raw_data_rptr);
        if (!okay) { printf("unable to send raw data\n"); return -1; }
        return 0;
    }

    return 404;
}

/**
 * get the value of a pixel in the current data pointer 
 */
int value_fn(std::vector<std::string> params, int socket) {
    if (params.size() < 4) {
        std::cout << "invalid request, get/value requires 2 additional parameters" << std::endl;
        return 500;
    }
    int x = std::stoi(params[2]);
    int y = std::stoi(params[3]);
    if (x>640||x<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 640: " << x << std::endl;
        return 500;
    }
    if (y>480||y<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 480: " << y << std::endl;
        return 500;
    }

    int offset = (y*640)+x;
    GOPTS->value_offset = offset;
    GOPTS->target_x = x;
    GOPTS->target_y = y;

    std::string resp = "{\"value\":"+std::to_string(GOPTS->value)+"}";

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(resp.length()) + "\n\n" + resp;

    send_headers(raw, socket);
    return 200;
}

int calc_distance(int x1, int y1, int x2, int y2)
{
    float dist = std::sqrt((float)(x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    return (int)dist;
}

uint calculate_noise(uint16_t *src)
{
    //printf("init data\n");
    std::vector<uint16_t> window;
    //printf("resize\n");
    window.resize(32);
    //printf("data ptr\n");
    uint16_t *w = window.data();
    //printf("data set\n");

    uint off = 0;
    uint noise = 0;
    uint signal = 0;

    for (int y = 150; y < 475; y++)
    {
        //printf("l1\n");
        off = y * 640;
        //printf("l2 [%d]\n", off);
        for (int x = 4; x < 636; x++)
        {
            //printf("l3 [%d]\n", x);

            int dist = calc_distance(320, 175, x, y);

            // only count pixels in the "the band"
            if (dist < 70 || dist > 250)
            {
                continue;
            }
            signal++;

            int idx = 0;
            for (int i = -1; i <= 1; i++)
            {
                //printf("i [%d] \n", i);
                w[idx++] = src[off + x + i - 640];
                w[idx++] = src[off + x + i];
                w[idx++] = src[off + x + i + 640];
            }

            //printf("loop end!\n");
            std::sort(window.begin(), window.begin() + 9);
            //printf("sort!\n");

            /*
            if (x==320 && y>380 && y<420) {
                for (int i=0;i<9;i++) {
                    printf(" %d", w[i]);
                }
                printf("\n");
            }
            */

            int diff = std::abs(src[off + x] - w[4]);
            noise += (diff > 20) ? 1 : 0;
        }
    }

    //printf("NOISE DONE sig[%d] noise[%d] r[%lf]\n", signal, noise, (double)(noise/signal));
    return noise;
}

/**
 * TODO: create a setting map function and move these to settings.cpp
 */
int position_fn(std::vector<std::string> params, int socket) {
    std::string data;
    int resp = 404;
    // quadrant read data
    if (params[0].compare(std::string("quadrant")) == 0) {
        resp = 500;
        float at = 0.0f;
        if (params.size() > 3) {
            int x = stoi(params[2]);
            int y = stoi(params[3]);
            at = find_laser(x, y);
        }
        data = "{\"quad\":" + 
            std::to_string(GOPTS->color_rotation) + 
            ", \"arctan\": "+std::to_string(at)+"}";
    }

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    send_headers(raw, socket);

    return resp;
}


/**
 * TODO: create a setting map function and move these to settings.cpp
 */
int setting_fn(std::vector<std::string> params, int socket)
{
    std::string data;
    int resp = 404;
    // change color rotation
    if (params[0].compare(std::string("degree")) == 0)
    {
        resp = 500;
        if (params.size() > 2) {
            GOPTS->color_rotation = atof(params[2].c_str());
            resp = 200;
        }
        data = "{\"value\":" + std::to_string(GOPTS->color_rotation) + "}";
    }

   // change color rotation
    if (params[0].compare(std::string("quality")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            GOPTS->quality = atoi(params[2].c_str());
            resp = 200;
        }
        data = "{\"value\":" + std::to_string(GOPTS->quality) + "}";
    }

    // change color rotation
    if (params[0].compare(std::string("version")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            std::string cmd = "./upgrade.sh" + params[2];
            system(cmd.c_str());
            GOPTS->running = false;
            resp = 2001;
        }
        data = "{\"value\":" + params[2] + "}";
    }

    // calculate image statistics
    if (params[0].compare(std::string("statstype")) == 0)
    {
        resp = 500;
        int stats_value = 1234;//debug value;

        //std::for_each(std::begin(params), std::end(params), [](std::string s) {std::cout << s << std::endl;});
        utils::print_params("stats:", params);
        
        if (params.size() > 6)
        {
            //GOPTS->enable = (params[2].compare("mean") == 0);
            GOPTS->stats_x1 = std::stoi(params[3]);
            GOPTS->stats_y1 = std::stoi(params[4]);
            GOPTS->stats_x2 = std::stoi(params[5]);
            GOPTS->stats_y2 = std::stoi(params[6]);
            std::cout << params[2] << std::endl;
            std::cout << GOPTS->stats_x1 << std::endl;
            std::cout << GOPTS->stats_y1 << std::endl;
            std::cout << GOPTS->stats_x2 << std::endl;
            std::cout << GOPTS->stats_y2 << std::endl;
            resp = 200;
        }
        data = "{\"value\":" + std::to_string(stats_value) + "}";
    }

    // change camera enable
    if (params[0].compare(std::string("camera")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            GOPTS->enable = (params[2].compare("enable") == 0);
            resp = 200;
        }
        data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
    }

    // change camera depth/ir
    if (params[0].compare(std::string("depth")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            std::cout << " set IR/DEPTH: " << params[2] << std::endl;
            GOPTS->depth_frame = (params[2].compare("ir") == 0);
            std::cout << "depth frame: " << GOPTS->depth_frame << std::endl;
            resp = 200;
        }
        data = "{\"value\":" + std::to_string(GOPTS->depth_frame) + "}";
    }

    // change the camera mode
    if (params[0].compare(std::string("mode")) == 0)
    {
        resp = 500;

        std::for_each(std::begin(params), std::end(params), [](std::string s) {std::cout << "camera mode: " << s << std::endl;});

        if (params.size() > 2)
        {
            std::copy(std::begin(params[2]), std::end(params[2]), std::begin(GOPTS->mode));
            GOPTS->mode[params[2].length()] = '\0';
        }

        std::ostringstream os;
        os << "{\"value\":" << GOPTS->mode << "}";
        data = os.str();
    }

    // change the camera mode
    if (params[0].compare(std::string("colormode")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            std::copy(std::begin(params[2]), std::end(params[2]), std::begin(GOPTS->mode));
            GOPTS->color_mode = std::stoi(params[2]); //(GOPTS->color_mode == 1) ? 2 : 1;
        }

        std::ostringstream os;
        os << "{\"value\":" << GOPTS->mode << "}";
        data = os.str();
    }

    // change camera max value (used in hsv conversion)
    if (params[0].compare(std::string("max")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            resp = 200;
            GOPTS->max_cam_value = std::stoi(params[2]);
            RGB8 c;
            uint16_t t = 1;
            depth_to_hsv(&t, &c, true);
            printf("set max cam value to %d\n", GOPTS->max_cam_value);
        }
        data = "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
    }

    // change camera max value (used in hsv conversion)
    if (params[0].compare(std::string("min")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            resp = 200;
            GOPTS->min_cam_value = std::stoi(params[2]);
            RGB8 c;
            uint16_t t = 1;
            depth_to_hsv(&t, &c, true);
            printf("set min cam value to %d\n", GOPTS->min_cam_value);
        }
        data = "{\"value\":" + std::to_string(GOPTS->min_cam_value) + "}";
    }

    // change camera gain value
    if (params[0].compare(std::string("gain")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            resp = 200;
            GOPTS->gain = std::stof(params[2] + "." + params[3]);
            camera->forceGain(GOPTS->gain, GOPTS->offset, 6000);
            printf("set gain value to %f\n", GOPTS->gain);
        }
        data = "{\"value\":" + std::to_string(GOPTS->gain) + "}";
    }

    // change camera gain value
    if (params[0].compare(std::string("offset")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            resp = 200;
            GOPTS->offset = std::stof(params[2]);
            camera->forceGain(GOPTS->gain, GOPTS->offset, 6000);
            printf("set offset value to %f\n", GOPTS->offset);
        }
        data = "{\"value\":" + std::to_string(GOPTS->offset) + "}";
    }

    // change camera gain value
    if (params[0].compare(std::string("geom")) == 0) {
        resp = 500;
        if (params.size() > 2) {
            resp = 200;
            GOPTS->geometry = (params[2] == "on");
            camera->setControl("geometry_correction", params[2]);
            printf("set offset value to %f\n", GOPTS->offset);
        }
        data = "{\"value\":" + std::to_string(GOPTS->offset) + "}";
    }


    // change camera gain value
    /*
    if (params[0].compare(std::string("correction")) == 0) {
        resp = 500;
        data = "{\"value\":\"built geom calib cache\"}";
    }
    */


    // change camera max value (used in hsv conversion)
    if (params[0].compare(std::string("noisereduction")) == 0)
    {
        resp = 500;
        if (params.size() > 2)
        {
            resp = 200;
            GOPTS->noise_reduction = std::stoi(params[2]);
            printf("set noise reduction to: [%d]\n", GOPTS->noise_reduction);
        }
        data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
    }

    if (params[0].compare(std::string("noise")) == 0)
    {
        resp = 200;
        raw_data_mtx.lock();
        uint noise = calculate_noise(copy);
        raw_data_mtx.unlock();
        data = "{\"noise\":" + std::to_string(noise) + ", \"ratio\":" + std::to_string(1.0f - (float)noise / (float)100527) + "}";
    }


    if (params[0].compare(std::string("temp")) == 0)
    {
        resp = 200;
        float tempcpu = get_cpu_temp_c();
        float tempvix = get_vixel_temp_c();
        data = "{\"cputemp\":" + std::to_string(tempcpu) + ",\"vixeltemp\":" + std::to_string(tempvix) + "}";
    }

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
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

    camera = cameras.front();
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

    // this is set to depth_ir.  depth/ir _only modes are not supported
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

    std::string lastmode = "near";
    
    aditof::Frame frame;
    status = camera->setMode(GOPTS->mode);
    if (status != aditof::Status::OK) {
        LOG(ERROR) << "Could not set camera mode!";
        return 0;
    }

    
    //camera->forceGain(GOPTS->gain, GOPTS->offset);
    
//


    std::cout << "just set mode: " << GOPTS->mode << std::endl;
    
    //std::cout << " --  set mode: " << GOPTS->depth_frame << std::endl;
    //lastmode = GOPTS->depth_frame;
    //status = camera->setMode("near");

    aditof::FrameDataType type;
    //std::function<int(int, int, int, int)> calc_dist = calc_distance;
    int mdist = calc_distance(100, 100, 300, 200);
    printf("100x100 - 300x200 = [%d]\n", mdist);
    // std::function<int(int, int, int, int)> *calc_dist2 = utils::memoize(calc_dist);
    // auto calc_dist2 = utils::memoize(&calc_distance);


    copy = (uint16_t *)NEWMEM(uint16_t, 640*480);
    while(GOPTS->running) {

        if (GOPTS->depth_frame) {
            //printf("set depth frame\n");
            //status = camera->setFrameType(frameTypes.front());
            type = aditof::FrameDataType::DEPTH;
        } else {
            //printf("set IR frame\n");
            //status = camera->setFrameType(frameTypes.front());
            type = aditof::FrameDataType::IR;
        }

        /*
        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not set frame type!";
            GOPTS->running = false;
            return 0;
        }
        */


        // camera->setControl("noise_reduction_threshold", std::to_string(GOPTS->noise_reduction));

        // std::cout << "r frame" << std::endl;
        status = camera->requestFrame(&frame);
        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not request frame!";
            GOPTS->running = false;
            return 0;
        }



        while (dlock != 0) { usleep(10000); }
        status = frame.getData(type, &raw_data1);
        GOPTS->value = raw_data1[GOPTS->value_offset];
        
        process_image(raw_data1, copy, samples);//, 640*480*2, GOPTS->color_mode, calc_distance);
        //convert_to_rgb(copy, samples, 640*480*2, GOPTS->color_mode, calc_distance);
        //printf("GET DATA 2\n");
        //raw_data_wptr = raw_data1;
        //dlock = 1;
        ready = true;

        process_statistic(copy, GOPTS->stats_x1, GOPTS->stats_y1, GOPTS->stats_x2, GOPTS->stats_y2);

        /*
        if (raw_toggle) {
            //if (raw_data1 != NULL) { std::memset(raw_data1, 0, 921600/2); } while (dlock != 0) { usleep(1000); }
            status = frame.getData(type, &raw_data1);
            raw_data_wptr = raw_data1;
        } else {
            //if (raw_data2 != NULL) { std::memset(raw_data2, 0, 921600/2); }
            while (dlock != 0) { usleep(1000); }
            status = frame.getData(type, &raw_data1);
            raw_data_wptr = raw_data1;
        }
        */

        if (status != aditof::Status::OK) {
            LOG(ERROR) << "Could not get frame data!";
            return 0;
        }

        // std::cout << "raw toggle enable" << std::endl;
        //raw_data_wptr = (raw_toggle) ? raw_data1 : raw_data2; 
        //jpeg_data_wptr = (raw_toggle) ? jpeg_data1 : jpeg_data2;
        

        while (jlock == 1) { usleep(1000); }
        if (jpeg_data1 != NULL) { free(jpeg_data1); jpeg_data1 = NULL; }
        long foo = (long)&jpeg_data1;
        write_jpeg(samples, foo, &jlenptr);
        //printf("wrote jpeg_data\n");
    }
    ready = true;
    return 1;
}

int run_app() {
    utils::safe_cout("Starting GUI app...");

    app::init();
    app::run();

    utils::safe_cout("Closing GUI app...");

    return 0;
}


int run_test() {
    utils::safe_cout("Test complete.");
    return 0;
}





int main() {
    RGB8 c;
    uint16_t t=1;
    sys::SystemInit(320, 240);
    utils::init_options();
    samples = (unsigned char *)calloc(640*480*sizeof(uint16_t), 3);
    jpeg_data_wptr = jpeg_data2;
    jlenptr = jlen1;
    depth_to_hsv(&t, &c, true);

    //std::function<int(int, int, int, int)> calc_dist = calc_distance;

    init_mappings();

    //std::thread(process_image);
    std::thread lvgl_thread(run_app);
    // start the camera thread
    std::thread camera_thread(run_camera);
        
    printf("waiting for camera to start ...\n");
    while (!ready) { usleep(50000); }
    printf("starting web server ...\n");

    std::map<std::string, IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["position"] = position_fn;
    fnMap["setting"] = setting_fn;
    printf("web server start\n");
    web_server(PORT, fnMap);

    //auto retcode = run_app();

    printf("web server finish!\n");
    camera_thread.join();
    lvgl_thread.join();
    printf("server stopped! ...\n");



    return 0;
}
