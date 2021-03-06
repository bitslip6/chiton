#include "stdint.h"
#include <map>

extern "C" {
    
    enum FrameType { GAIN, POLAR, PLANAR };
    enum ImageType { NONE, GREY, COLOR };

    typedef struct SliceTranche {
        unsigned int tranche[20];
        unsigned short dist;
    } SliceTranche;


    typedef struct AngleDist {
        float angle;
        u16 dist;
        u16 dist_zero;
        s16 offset;
        s16 min_height;
        s16 max_height;
        float gain;
    } AngleDist;


    /**
     * callback for frame processing.
     * 
     * frame will be a point to with*height 16bit unsigned integers.  the values will be 0-4096 for uncalibrated data (4096 = 6000mm)
     * or 0-6000mm for calibrated data
     *
     * width, height are the width and height of the frame respectively
     * 
     * dist is a vector of AngleDist objects which represent horizontal angles and minimum / maximum distances at those angles
     */
    void process_standard_frame(uint16_t *frame, int width, int height, std::vector<AngleDist> &dist);
    
    /**
     * set the returned frame type to: 
     * GAIN - return the calibration gain mapping for each pixel 
     * POLAR - return raw polar data (the angle of each pixel = atan2(delta_x, delta_y) where delta_x = center_x - pixel_x, delta_y = center_y - pixel_y
     * PLANAR - return the polar image represented in traditional 2d planar data (1500x165)
     */
    bool set_frame_type(FrameType type);
    
    /**
     * set the type of image to create, if set to NONE, no jpeg data will be generated per frame
     * set to grey to produce compresed greyscale jpeg image data (8 bit)
     * color is an HSV color depth image (24 bit)
     */
    bool set_image_type(ImageType type);
    
    /**
     * set to true to turn on per pixel calibration
     */
    bool calibration_enable(bool enabled);
}
