#include <sys/types.h>
#include <stdio.h>
#include <math.h>
#include "defines.h"

#define FOV 90

void ground_detection(u_int16_t *image);
void process_floor_line(uint lineno, FloorLine *line, u_int16_t *image);

int main(int argc, char *argv[]) {
    //printf("ground\n");

    for (int i=0;i<640*480;i++) {
        double exp = sqrt(390*390 + 180*180);
    }
}

void ground_detection(u_int16_t *image) {

    uint expected_depth = 0;
    uint min_line = TOF_HEIGHT -1;
    float scale_factor = 1.1;

    FloorLine line;
    line.max_error_value = 5;
    line.line_no = 480;
    process_floor_line(line.line_no, &line, image);

    // calcualte the depth of the first rows until we find something that is not 
    // min distance
    while (line.line_avg < TOF_MIN_DIST + 100 && line.line_no > 240) {
        --line.line_no;
        process_floor_line(line.line_no, &line, image);
        printf("floor line: %d,   avg: %d,  err %d:\n", line.line_no, line.line_avg, line.num_errors);
    }

}


/**
 * process a line of floor data
 * 
 */
void process_floor_line(uint lineno, FloorLine *line, u_int16_t *image) {
    uint offset = line->line_no * TOF_WIDTH;
    uint offset_min = offset - (TOF_WIDTH / 2);
    uint tmp_errno;
    uint calculated_depth;
    line->num_errors = line->max_error_value = line->max_error_len = 0;

    // check right side of floor, depth values should be decreasinng
    for (; offset < offset_min; offset--) {
        if (image[offset] > line->max_error_value) {
            line->num_errors++;
            tmp_errno++;
        } else {
            if (tmp_errno > line->max_error_len) {
                line->max_error_len = tmp_errno;
            }
            tmp_errno = 0;
            calculated_depth += image[offset];
        }
    }

    line->line_avg = calculated_depth / TOF_WIDTH - line->num_errors;
}


/**
 * find the maxmum floor pixel that we care about...
 * 
 * returns an image row number for the last floor line we would care about.
 * goes out a distance of 11 bits worth of resolution (TOF_MIN_DIST + 2000)
 */
int find_max_floor_pixel(u_int16_t *image) {

    for (int y=480; y != 0; y--) {
        if (image[(y*TOF_WIDTH) - TOF_HALF_WIDTH] > TOF_MIN_DIST + 2048) {
            return y;
        }
        else {
            printf("Y POS: %d = %d", y, image[(y * TOF_WIDTH)]);
        }
    }

    return -1;
}


/**
 * return the line number from the image that has the first non minimum value
 * returns -1 if the center line could not be found.
 * 
 * will return bogus data if something is in front of the robot.
 * TODO: call object detection and make sure the field is clear
 */
int find_center_floor_pixel(u_int16_t *image) {
    for (int y=480; y != 0; y--) {
        if (image[(y*TOF_WIDTH) - TOF_HALF_WIDTH] > TOF_MIN_DIST + 10) {
            return y;
        }
    }

    return -1;
}