#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include "demos.h"
#include "json.hpp"

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
#define TMP_DIR std::string("/tmp/chiton_debug")

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
    int running;
    bool enable;
    std::mutex point_lock;
    std::atomic<int> waiting;


    double vixel_temperature;
    double cpu_temperature;
 
    u_int16_t max_cam_value;
    u_int16_t min_cam_value;
    u_int16_t value;
    std::atomic<float> std_deviation;
    
    u_int16_t target_x1;
    u_int16_t target_y1;
    u_int16_t target_x2;
    u_int16_t target_y2;

    int16_t center_x;
    int16_t center_y;
    u_int16_t inner_dist;
    u_int16_t outer_dist;

    u_int16_t cross_x;
    u_int16_t cross_y;
    
    u_int16_t dst_width;
    u_int16_t dst_height;
    
    bool depth_frame;
    int color_mode;
    int noise_reduction;
    float color_rotation;
    int quality;
    int default_log_level;
    std::map<std::string, int> *log_level;
    char mode[32];

    bool geometry;
    bool chitoncal;
    bool planar;

    std::atomic<bool> is_calibration_enabled;
    int near_calibration;
    int far_calibration;
    
    std::map<int, VixelCalibration*> *cal;
    json config;
    
    void *api_lib;

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




















































































































































