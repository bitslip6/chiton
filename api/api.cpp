#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "api.h"


extern "C" {

    static runline* runlines = NULL;
    static unsigned short num_runlines = 0;
    

    /**
     * @brief called before the first call to process_standard_frame.  
     * provides a mapping to all pixels that contain pixel data.  Allows
     * process_standard_frame to skip over pixels that are not used.
     * 
     * @param lines 
     * @param num_lines 
     */
    void set_runlines(runline* lines, unsigned short num_lines) {
        printf("set %d runlines\n", num_lines);
        runlines = lines;
        num_runlines = num_lines;
    }

    /**
     * @brief process single frame data, raw data is arranged like a standard png, or 
     * jpeg image. this method is called once for every frame.
     *
     * frame number is 16bit unsigned short located at frame[(width*height)-1]
     *
     * frame is a pointer to a list of width*height points of 16bit unsigned
     * integer data. each data point represents a distance value in millimeters.
     * data is arranged like a normal image, memory increases along
     * each row (width) and then after each row, the column increases (height)
     * ---->
     * ---->
     * ---->
     *
     * @param horiz_angle is the angle of the pixel in radians.  calculated as atan2(center_x - x, center_y - y)
     * @param vert_angle is the vertical angle of the the pixel in radians, from -15 degrees to +45 degrees.  
     *     this is calculated as the distance of the pixel from the center_x, center_y location
     * @param angle_bucket is a vertical angle bucket which maps a pixel to a 1 pixel wide vertical bucket 
     *     from -15 to +45 degrees. there are 1500 buckets each representing 1/5 horizontal degrees.
     *
     *
     */
    void process_standard_frame(uint16_t *frame, float *horiz_angle, float *vert_angle, uint16_t *angle_bucket) {

        int offset = 0;
        // STANDARD METHOD TO INSPECT EVERY PIXEL
        for (int y=0; y<CAMERA_HEIGHT; y++) {
            for (int x=0; x<CAMERA_WIDTH; x++) {

                int16_t x1 = frame[offset] * (-sinf(horiz_angle[offset]) * cosf(vert_angle[offset]));
                int16_t y1 = frame[offset] * (-sinf(vert_angle[offset]));
                int16_t z1 = frame[offset] * (cosf(horiz_angle[offset]) * cosf(vert_angle[offset]));

                // print out a few pixels of data for debugging
                /*
                if (x == 320 && frame[offset] > 128) {
                    printf("XYZ %d %d %d\n", x1, y1, z1);
                }
                */
                offset++;
            }
        }

        frame[(240*640)+320] = 256;

        return;


        // OPTIMIZED METHOD TO INSPECT ONLY PIXELS WITH DATA
        // runlines contains a list of uninterrupted pixel data horizontal runs
        // each runline contains a start pixel offset and an end pixel offset for the run
        // to inspect only pixels with valid data, iterate over each run, and then
        // iterate over each pixel in the run.
        for (int j=0; j<num_runlines; j++) {
            const uint srcstart = runlines[j].srcstart;
            const uint runlen = runlines[j].runlen;

            for (uint offset=srcstart; offset<(runlen);offset++) {
                int16_t x1 = frame[offset] * (-sinf(horiz_angle[offset]) * cosf(vert_angle[offset]));
                int16_t y1 = frame[offset] * (-sinf(vert_angle[offset]));
                int16_t z1 = frame[offset] * (cosf(horiz_angle[offset]) * cosf(vert_angle[offset]));

                // print out a few pixels of data for debugging
                if (offset == 320 && frame[offset] > 128) {
                    printf("XYZ %d %d %d\n", x1, y1, z1);
                }
            }
        }
    }


    /**
     * called when library is loaded (dlopen)
     */
    void __attribute__ ((constructor)) initLibrary(void) {
        printf("^^  Custom Library is initialized\n");
    }

    /**
     * called when library exits
     */
    void __attribute__ ((destructor)) cleanUpLibrary(void) {
        printf("$$  Custom Library exited\n");
    }

}
