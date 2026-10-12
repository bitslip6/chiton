#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include "demos.h"
#include "json.hpp"
#include <array>

#include <aditof/camera.h>
#include <memory>
#include <atomic>
#include <map>
#include <cstddef>
#include <mutex>



#define PI  3.1415926f
#define PI2 6.2831852f
#define MAX64K  65536
#define EXIT_CONFIG_ERROR -2
#define EXIT_SYSCALL_ERROR -3
#define EXIT_TEMP_ERROR -4
#define EXIT_ADI_ERROR -5
#define EXIT_UPGRADE -6
#define EXIT_I2C_ERROR -7
#define EXIT_CORRUPT_MAPPING -8
#define EXIT_FILE_OPEN_ERR -9
#define EXIT_FB_ERROR -10
#define EXIT_OPEN_CL -11
#define TMP_DIR std::string("/usr/local/chiton/chiton_debug")

#ifdef __cplusplus
extern "C" {
#endif


typedef struct VixelCalibration {
    int vixel;
    int target1;
    int point1;
    int target2;
    int point2;

    float gain;
    float radian_min;
    float radian_max;
    float radian;
    s16 offset;
} VixelCalibration;



// global system configuration
// TODO: remove data from here.  this should only be options
typedef struct GlobalOpts {
    std::atomic<int> running{1};
    std::atomic<bool> enable{true};
    std::atomic<bool> frame_request; // Legacy graphics only; no HTTP/capture wait.
    std::mutex point_lock;

    double vixel_temperature;
    double cpu_temperature;
    
    u8 min_left; 
    u8 min_front; 
    u8 min_right; 
 
    u_int16_t max_cam_value;
    u_int16_t min_cam_value;
    u_int16_t value;
    float std_deviation;
    
    xy_t point;
    int16_t center_x;
    int16_t center_y;
    u_int16_t inner_dist;
    u_int16_t outer_dist;
    
    u_int16_t dst_width;
    u_int16_t dst_height;
    
    std::atomic<bool> depth_frame;
    std::atomic<int> color_mode;
    int noise_reduction;
    std::atomic<bool> dynamic_illumination;
    float calibrate_angle;
    int quality;
    int default_log_level;
    std::map<std::string, int> *log_level;
    char mode[32];

    bool smoothing;
    bool planar;
    u8 auto_calibrate_vixel;

    int calibration_enabled;
    int near_calibration;
    int far_calibration;
    
    bool draw_slam;
    int draw_jpeg;
    
    std::map<int, VixelCalibration*> *cal;
    json config;
    
    void *api_lib;
    
    std::string eth0;
    std::string wlan0;
    std::string message;
    std::mutex status_lock;
    void set_message(const std::string &text) {
        std::lock_guard<std::mutex> guard(status_lock);
        message = text;
    }
    // Append to preserve offsets of existing fields. Immutable policy selection;
    // never persisted in saved device configuration.
    bool experimental_illumination = false;
    std::array<int,4> illumination_initial{};
    std::atomic<uint64_t> illumination_generation{0};
    std::atomic<uint64_t> illumination_capture_generation{0};

} GlobalOpts;

extern GlobalOpts *GOPTS;



typedef struct DemoConf {
    u8 iron;
    u8 depthon;
    u8 bbon;
    u8 fpson;
    u8 senseon;
    u8 flooron;
    u16 range;
    u16 rangeoffset;
    float rangefactor;


    uint sense;
    uint max_depth;
    uint min_depth;
    uint mid_depth;

    uint bb_min_x;
    uint bb_max_x;

    uint bb_min_y;
    uint bb_max_y;

    uint vixel_w;
    uint vixel_h;

    float pre;
    float post;
    float gain;
    

    u8 running;
    u8 fps;
} DemoConf;


typedef struct Point2d {
    uint16_t x;
    uint16_t y;
} Point2d;

#define FB_PIXEL_SZ 5

// install signal handlers
void Setup_Handlers(void);

DisplayBuffers* fb_init(char *file_name);
DisplayBuffers* SystemInit(uint16_t width, uint16_t height);
extern std::shared_ptr<aditof::Camera> camera;
   
#ifdef __cplusplus
}
#endif

#endif




















































































































































