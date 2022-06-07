#ifndef __CALIBRATION_H
#define __CALIBRATION_H 1

#include <vector>
#include <jpeglib.h>
#include <stdint.h>
#include "system.h"

// #define ADI_GAIN 1.464843f
#define ADI_GAIN 1.44f
#define SLAM_POINTS 1024
#define SLAM_ANGLE (SLAM_POINTS / PI2)

#define SLAM_STATIC_BIT 1
#define SLAM_NOISE_BIT 2
#define SLAM_DYNAMIC_BIT 3
#define SLAM_SPIKE_BIT 4
#define SLAM_AOI_BIT 5

#define SET_SLAM_STATIC(var) var |= (1 << SLAM_STATIC_BIT)
#define SET_SLAM_NOISE(var) var |= (1 << SLAM_NOISE_BIT)
#define SET_SLAM_DYNAMIC(var) var |= (1 << SLAM_DYNAMIC_BIT)
#define SET_SLAM_SPIKE(var) var |= (1 << SLAM_SPIKE_BIT)
#define SET_SLAM_AOI(var) var |= (1 << SLAM_AOI_BIT)

#define CLEAR_SLAM_STATIC(var) var &= ~(1 << SLAM_STATIC_BIT)
#define CLEAR_SLAM_NOISE(var) var &= ~(1 << SLAM_NOISE_BIT)
#define CLEAR_SLAM_DYNAMIC(var) var &= ~(1 << SLAM_DYNAMIC_BIT)
#define CLEAR_SLAM_SPIKE(var) var &= ~(1 << SLAM_SPIKE_BIT)
#define CLEAR_SLAM_AOI(var) var &= ~(1 << SLAM_AOI_BIT)



namespace calibration
{



    typedef struct gain_t {
        float gain;
        float offset;
    } gain_t;

    typedef struct point {
        uint index;          // image source index
        uint16_t near_point; // measured distance
        uint16_t far_point;  // measured distance
        float center_angle;  // measurement angle
        uint16_t near_dist;  // actual distance
        uint16_t far_dist;   // actual distance
    } point;

    typedef struct runline {
        uint srcstart;
        uint pcidx;
        uint runlen;
        u8 padd_start;
        u8 padd_end;
    } runline;




    u16* get_pixel_dist();
    float* get_pixel_gain();
    float* get_pixel_offset();
    float* get_x_factor();
    float* get_y_factor();
    float* get_z_factor();
    point* get_calibration();
    uint* get_offsets();
    runline* get_run_lines();
    u16 get_num_runlines();

    u16 *make_slam_buffer();
    int __attribute((pure)) padding(int len, int alignment);
    void make_calibration_data();
    void vector_point_cloud(u16 *src);
    void calibrate_frame(const u16 *__restrict__ src, u16 *__restrict__ dst);
    void avg_frame_fast(const u16 *__restrict__ src, u16 *__restrict__ dst);
    void average_frame(const u16 *__restrict__ src, u16 *dst);
    void apply_vert_cos(const u16 *__restrict__ src, u16 *dst);
    void vector_slam(u16 *__restrict__ src, u16 *__restrict__ slam, u16 slam_idx);
    s16 estimate_rotation(const u16 *__restrict__ target_frame, const u16 *__restrict__ test_frame);
    float find_closest_wall_radian(const u16 *data, u16 target_depth_mm);

    void fill_missing_calibration_data();


    u16 *get_pixel_bucket();
    float *get_horiz_angle();
    float *get_vert_angle();


    float vertical_gain_multiplier(float vertical_angle);
    unsigned long gain_mapping(JSAMPLE *__restrict__ jpeg_data);
    void save_calibration_frame(const u16 *__restrict__ src, const u16 distance, const float center_angle);
    void process_calibration();
    void process_old_calibration();

    uint32_t dynamic_illumination(u16 *__restrict__ slam, u16 slam_idx, u32 cur_ill);
}


#endif
