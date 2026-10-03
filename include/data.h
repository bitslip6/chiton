// Synchronization: scalar/container operations use cam::state_mutex().
// Borrowed pointer/mapping views require the caller to hold that mutex for the
// entire lifetime of access. Copy snapshots under ownership, then release it
// before network I/O. is_ready/set_ready are lock-free atomic notifications.
#ifndef __DATA_H
#define __DATA_H 1

#include "demos.h"
#include <vector>


#define FRAME_TYPE_DEPTH 1
#define FRAME_TYPE_IR    2

#define FRAME_FORMAT_POLAR  1
#define FRAME_FORMAT_PLANAR 2
#define FRAME_FORMAT_SLAM 3

#define FRAME_MAX_WIDTH 1500
#define FRAME_MAX_HEIGHT 480
#define MAX_WALL_NOISE 250
#define MAX_WALL_TOLERANCE 200

#define FRAME_PLANAR_WIDTH_DEFAULT  1500
#define FRAME_PLANAR_HEIGHT_DEFAULT 165 
#define FRAME_POLAR_WIDTH_DEFAULT  640
#define FRAME_POLAR_HEIGHT_DEFAULT 480

#define ANGLE_POINTS 1500
#define ANGLE_POINTS_INC (PI * 2) / 1500
#define MAX_DIST_CAL 6000
#define MAX_DIST_RAW 4096
#define BUCKET_DIST_RAW 128
#define BUCKET_DIST_CAL 200

// #define BUCKET_SZ_RAW MAX_DIST_RAW / BUCKET_DIST_RAW
// #define BUCKET_SZ_CAL MAX_DIST_CAL / BUCKET_DIST_CAL
#define BUCKET_SZ_RAW 32
#define BUCKET_SZ_CAL 32

#define BUCKET_FRAMES 3
#define TAL_LANES 4
#define TAL_LANES_FACTOR ANGLE_POINTS / TAL_LANES
 
#define TAL_BUCKETS BUCKET_SZ_RAW 


namespace data
{
    void init_defaults();
    uint16_t *get_calibrated_ptr();
    uint16_t *get_smooth_ptr();
    s16 *get_point_cloud();
    float *get_point_cloud_f();
    u8 *get_point_cloud_compressed();

    // std::vector<offset_samp_t> get_point_mapping();
    //std::vector<offset_samp_t>* get_point_mapping();
    std::unordered_map<int, std::vector<offset_samp_t>>* get_point_mapping();
    void set_chiton_calibration(bool onoff);
    bool get_chiton_calibration();
    
    u32 get_beta();
    void set_beta(u32 beta_value);
    bool set_frame_type(uint type);
    uint get_frame_type();
    bool set_frame_format(uint format);
    uint get_frame_format();
    uint get_frame_width();
    uint get_frame_height();
    uint get_frame_size_bytes();
    bool is_ready();
    void set_ready(bool ready);
    void set_frame_width(int height);
    u16 *get_planar_idx();
    
    void set_point_cloud_size(int size);
    int get_point_cloud_size();

    u16 *get_slam();
    void clear_points();
    void add_point(u16 value);
    float point_stdd();
    u16 point_mean();
    u16 point_range();
    void set_frame_dim(int width, int height);

    u32* get_bucket_totals(int frame);
    u32* get_bucket_counts(int frame);
    u32* get_tal_buckets();
}
#endif
