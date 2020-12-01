#ifndef __DATA_H
#define __DATA_H 1

#include "demos.h"
#include <vector>


#define FRAME_TYPE_DEPTH 1
#define FRAME_TYPE_IR    2

#define FRAME_FORMAT_POLAR  1
#define FRAME_FORMAT_PLANAR 2
#define FRAME_MAX_WIDTH 1500
#define FRAME_MAX_HEIGHT 480

#define FRAME_PLANAR_WIDTH_DEFAULT  1500
#define FRAME_PLANAR_HEIGHT_DEFAULT 165 
#define FRAME_POLAR_WIDTH_DEFAULT  640
#define FRAME_POLAR_HEIGHT_DEFAULT 480



namespace data
{
    void init_defaults();
    uint16_t *get_calibrated_ptr();
    // std::vector<offset_samp_t> get_point_mapping();
    std::vector<offset_samp_t>* get_point_mapping();
    void set_chiton_calibration(bool onoff);
    bool get_chiton_calibration();
    
    bool set_frame_type(uint type);
    uint get_frame_type();
    bool set_frame_format(uint format);
    uint get_frame_format();
    uint get_frame_width();
    uint get_frame_height();
    uint get_frame_size_bytes();
}
#endif
