#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include "demos.h"
#include <aditof/camera.h>
#include <memory>
#include <atomic>
#include <map>
#include <cstddef>


#define Exit_If(condition, error_str, exit_return) if ((condition)) { if (error_str) { PANIC(error_str); } exit(exit_return); }
#define Return_If(condition, error_str, error_return) if ((condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define Return_IfNot(condition, error_str, error_return) if (!(condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define ReturnVoidIf(condition, error_str) if ((condition)) { if (error_str != NULL) { WARN(error_str); } return; }
#define ReturnVoid_IfNot(condition, error_str) if (!(condition)) { if (error_str != NULL) { WARN(error_str); } return; }


#define Exit_If(condition, error_str, exit_return) if ((condition)) { if (error_str) { PANIC(error_str); } exit(exit_return); }
#define Return_If(condition, error_str, error_return) if ((condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define Return_IfNot(condition, error_str, error_return) if (!(condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define ReturnVoidIf(condition, error_str) if ((condition)) { if (error_str != NULL) { WARN(error_str); } return; }
#define ReturnVoid_IfNot(condition, error_str) if (!(condition)) { if (error_str != NULL) { WARN(error_str); } return; }

#define MAX64K  65536

#ifdef __cplusplus
extern "C" {
#endif


typedef struct VixelCalibration {
    int vixel;
    int target1;
    int point1;
    int target2;
    int point2;
    int angle;

    float gain;
    s16 offset;
} VixelCalibration;



// global system configuration
typedef struct GlobalOpts {
    bool running;
    bool enable;

    float vixel_temperature;
    float cpu_temperature;
 
    u_int16_t max_cam_value;
    u_int16_t min_cam_value;
    u_int16_t value;
    std::atomic<float> std_deviation;
    u_int32_t value_offset;
    
    u_int16_t target_x;
    u_int16_t target_y;
    u_int16_t target_x1;
    u_int16_t target_y1;
    u_int16_t target_x2;
    u_int16_t target_y2;

    u_int16_t center_x;
    u_int16_t center_y;
    u_int16_t inner_dist;
    u_int16_t outer_dist;
    
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
    
    std::map<int, VixelCalibration*> *cal;

    json config;

// TODO: remove these
    u_int16_t stats_x1;
    u_int16_t stats_y1;
    u_int16_t stats_x2;
    u_int16_t stats_y2;
    std::atomic<double> area_mean_value;
    std::atomic<double> area_std_deviation;
    std::atomic<double> area_temporal_noise;
    std::atomic<double> area_rms_value;
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




#define FB_PIXEL_SZ 5

// install signal handlers
void Setup_Handlers(void);

DisplayBuffers* fb_init(char *file_name);
DisplayBuffers* SystemInit(u16 width, u16 height);
extern std::shared_ptr<aditof::Camera> camera;
   
#ifdef __cplusplus
}
#endif

#endif
