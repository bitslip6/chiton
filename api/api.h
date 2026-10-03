#include "stdint.h"
#include <map>

#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480

extern "C" {
    
    enum FrameType { GAIN, POLAR, PLANAR };
    enum ImageType { NONE, GREY, COLOR };

    typedef struct runline {
        unsigned int srcstart;
        unsigned int pcidx;
        unsigned int runlen;
        unsigned char padd_start;
        unsigned char padd_end;
    } runline;

    
    /**
     * callback for frame processing.
     * 
     * frame will be a point to with*height 16bit unsigned integers.  the values will be 0-4096 for uncalibrated data (4096 = 6000mm)
     * or calibrated depth values in millimeters within the calibration AOI.
     * For ABI compatibility, the callback's final element contains the legacy
     * 16-bit frame counter, not a measurement. This is now a private scratch
     * frame: normal pixel edits are published, but final-element edits are not.
     * Do not retain the frame pointer beyond the callback. Public frames retain
     * their last measurement; frame IDs/times live in HTTP metadata instead.
     *
     */
    void process_standard_frame(uint16_t *frame, float *horiz_angle, float *vert_angle, uint16_t *angle_bucket);
    
    void set_runlines(runline* lines, unsigned short num_lines);
}
