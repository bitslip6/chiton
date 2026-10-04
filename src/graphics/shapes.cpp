#include <vector>
#include <algorithm>
#include <map>
#include <functional>
#include <math.h>
#include "neon_compat.h"


#include "demos.h"
#include "calibration.h"
#include "core.h"
#include "system.h"
#include "shapes.h"
#include "utils.hpp"
#include "data.h"
#include "color/hsv.h"

#include "fplus/fplus.hpp"

// #define ONE_ONE_THIRD (1 + (1.0f/3.0f))
#define ONE_ONE_THIRD 1.3255

#define BYTE(data, offset) (data & (0xFF<<(offset*8)) >> (offset*8))
#define BYTE_DEC(data, offset) (data & (0xFF<<(offset*8)) >> (offset*8))
#define BYTE_INC(data, offset) (data & (0xFF<<(offset*8)) >> (offset*8))


namespace gfx
{
    
struct GainPair {
    int idx;
    float radian;
    float radian_diff;
    GainPair(int i,float r,float d) : idx(i), radian(r), radian_diff(d) { }
    
    bool operator < (const GainPair& test) const { 
        auto r = std::abs(radian_diff) < std::abs(test.radian_diff);
        return r;
    }
    bool operator > (const GainPair& test) const { 
        return std::abs(radian_diff) > std::abs(test.radian_diff);
    }

    operator std::string() const { return std::string("Index: " + std::to_string(idx) + " rad: " + std::to_string(radian) + " diff: " + std::to_string(radian_diff)); }
};

//float get_diff(const GainPair& gp) { return gp.radian_diff; };

template <typename T>
void print_result(const T& x) {
    std::cout << fplus::show(x) << std::endl;
}

/**
 * calculate a single point on a circle at radius and radians
 *
 */
xy_t calculate_circle_point(uint16_t radius, float radians) {
    radians = -radians;
    radians -= PI/2;
    xy_t p = {(s16)round(radius * cos(radians)), (s16)round(radius * sinf(radians))};
    return p;
}



/**
 * calculate a single point on a circle at radius and radians
 *
 */
xyf_t calculate_circle_point_float(uint16_t radius, float radians) {
    radians -= PI/2;
    xyf_t p = {radius * cosf(radians), radius * sinf(radians)};
    return p;
}

/**
 * return degree in radians from -PI to +PI of pixel from center of image
 * 
 * PURE
 */
float pixel_to_radians(xy_t center, xy_t pixel) {
    return atan2f((float)center.x-pixel.x, (float)center.y-pixel.y);
}

/**
 * return a function that will accept a single point and will add offsetx and offsety
 */
std::function<xy_t(xy_t)> fwd_move_point(s16 offset_x, s16 offset_y) {
    return [offset_x, offset_y](xy_t point) {
        xy_t p = {(s16)(point.x + offset_x), (s16)(point.y + offset_y)};
        return p;
    };
}

/**
 * return a function that will accept a single point and will add offsetx and offsety
 */
std::function<xyf_t(xyf_t)> fwd_move_point_float(s16 offset_x, s16 offset_y) {
    return [offset_x, offset_y](xyf_t point) {
        xyf_t p = {(float)(point.x + offset_x), (float)(point.y + offset_y)};
        return p;
    };
}



/**
 * returns a new vector of points along a circle of radius
 */
    std::vector<xy_t> circle_points(s16 center_x, s16 center_y, u16 numpoints, u16 radius) {
   
        std::vector<xy_t> points;

        // offset1 and offset2 move the sample point in an x/y direction
        // this is used to correct for the zig zag distortion
        for (float angle = -PI; angle < PI; angle += PI2 / (numpoints-1)) {
            if (angle > -0.77f && angle < 0.77f) { continue; }
            //if (angle > 3.8f && angle < 5.4f) { continue; }
//            if (angle > //3.8f && angle < 5.4f) { continue; }
            points.emplace_back(calculate_circle_point(radius, angle));
        }
       
        // move each point to the center position
        return fplus::transform(fwd_move_point(center_x, center_y), points);
    }

    /**
     * returns a new vector of points along a circle of radius
     */
    std::vector<xyf_t> circle_points_float(s16 center_x, s16 center_y, u16 numpoints, u16 radius) {
       
        std::vector<xyf_t> points;

        float offset = -0.9f;
        for (float angle = 0.0f+offset; angle < PI2+offset-0.5f; angle += PI2 / numpoints) {
            if (angle > 3.8f && angle < 5.2f) { continue; }
            points.emplace_back(calculate_circle_point_float(radius, angle));
        }

        return fplus::transform(fwd_move_point_float(center_x, center_y), points);
    }

    


    VixelCalibration* merge_vixel_calibration(VixelCalibration *v0, VixelCalibration *v1, float angle) {
        
        float r0 = v0->radian;
        float r1 = v1->radian;
        
        /*
        if (std::abs(r0) == std::abs(r1)) {
            if (angle > 0) { r1 =  3.45f; } // NOTE: 3.45 NOT 3.14 (3.45 = 2.82 + (vixel distance: .63)
            else { r1 =  -3.45f; } }
        */

        float v0dist = std::abs(angle - r0);
        float v1dist = std::abs(angle - r1);

        float diff = std::abs(r1 - r0);
        //float v0p = v0dist / diff;
        //float v1p = v1dist / diff;

        //float gain = v0->gain;
        //float offset = v0->offset;


        // we have a large diff, this is the -3.14 to +3.14 boundary.  hack in the angle fixes here
        if (diff > PI) {
            // vixel to vixel radian diff is 0.5890
            if (angle > 0.0f) {
                r1 = r0 + 0.5890f;
            } else {
                r1 = r0 - 0.5890f;
            }
        }

        
        float gain = ((v0->gain*(r1 - angle))+(v1->gain*(angle - r0))) / (r1 - r0);
        float offset = ((v0->offset*(r1 - angle))+(v1->offset*(angle - r0))) / (r1 - r0);

        //gain = (v0->gain * v0p) + (v1->gain * v1p);
        //offset = (v0->offset * v0p) + (v1->offset * v1p);
                
        
        VixelCalibration *v = new VixelCalibration();
        v->vixel = (v0dist < v1dist) ? v0->vixel : v1->vixel;
        v->radian = angle;
        v->gain = gain;
        v->offset = (s16)offset;

        return v;
    }

    /**
     * find the vixel calibration for a given angle in radians

     * TODO: expert system.  it's ugly but fast and will work 100%
     */
    VixelCalibration* find_calibration_for_radian(float angle) {
        
        //angle = -angle;
        // vixel 1 located at upper left of image
        VixelCalibration *vix1 = GOPTS->cal->find(1)->second;
        if (angle > vix1->radian && angle < 0) {
            return GOPTS->cal->find(1)->second;
        }
        // vixel 8 located at upper right image
        VixelCalibration *vix8 = GOPTS->cal->find(8)->second;
        if (angle < vix8->radian && angle > 0) {
            return GOPTS->cal->find(8)->second;
        }
        VixelCalibration closest, next_close;

        // short circuit the edges
        if (angle > 0 && angle < 1.08f) { return GOPTS->cal->find(8)->second; }
        else if (angle < 0 && angle > -1.08f) { return GOPTS->cal->find(1)->second; }

        VixelCalibration *vix5 = GOPTS->cal->find(5)->second;
        VixelCalibration *vix4 = GOPTS->cal->find(4)->second;
        if (angle <= vix4->radian) { 
            closest = *GOPTS->cal->find(4)->second;
            next_close = *GOPTS->cal->find(5)->second;
        } else if (angle >= vix5->radian) {
            closest = *GOPTS->cal->find(5)->second;
            next_close = *GOPTS->cal->find(4)->second;
        } else {
            std::vector<GainPair*> gains;
            for (auto const&[key, val] : *GOPTS->cal) {
                gains.emplace_back(new GainPair(key, val->radian, angle - val->radian));
            }

            //  maybe we can reduce this to just 2 vixels ?
            // sort by vixel distance to all other vixels...
            std::sort( gains.begin( ), gains.end( ), [angle]( const auto& lhs, const auto& rhs ) { 
                return std::abs(lhs->radian_diff) < std::abs(rhs->radian_diff);
            } );
            
            closest = *GOPTS->cal->find(gains.at(0)->idx)->second;
            next_close = *GOPTS->cal->find(gains.at(1)->idx)->second;
        }

        auto f = merge_vixel_calibration(&closest, &next_close, angle);
        //f->gain = 1.3f + (closest.vixel * 0.1f);
        if (angle> 5.330f && angle < 5.340f) {
            printf("closest: %d,%d. G1:%f, G2:%f = %f\n", closest.vixel, next_close.vixel,
                   static_cast<double>(closest.gain), static_cast<double>(next_close.gain), static_cast<double>(f->gain));
        }
        return f;
    }

    



    // returns absolute value of number
    static inline float absolute(float x ) {
        if (x < 0) return -x;
        else return x;
    }

    //returns integer part of a floating point number
    static inline int iPartOfNumber(float x) {
        return (int)floorf(x);
    }
     
    //rounds off a number
    static inline int roundNumber(float x) {
        return iPartOfNumber(x + 0.5f) ;
    }
     
    //returns fractional part of a number
    static inline float fPartOfNumber(float x) {
        if (x>0) return x - iPartOfNumber(x);
        else return x - (iPartOfNumber(x)+1);
    }
   
    //returns fractional part of a number
    static inline float rfPartOfNumber(float x) {
        return 1 - fPartOfNumber(x);
    }

    /**
     * returns true if line is > vertical and < horizontal
     */
    bool is_steep(line_t line) {
        return std::abs(line.p2.y - line.p1.y) > std::abs(line.p2.x - line.p1.x) ;
    }



    /**
     * draw a line, no multiply
     **/
    /*
    void sample_line(uint16_t *src, uint16_t *dst, line_t line) { //int x0, int y0, int x1, int y1, u8 hue, u8 sat) {

        bool steep = false;
        if (absolute(line.p1.x-line.p2.x) < absolute(line.p1.y-line.p2.y)) {
            std::swap(line.p1.x, line.p1.y);
            std::swap(line.p2.x, line.p2.y);
            steep = true;
        }
        if (line.p1.x > line.p2.x) {
            std::swap(line.p1.x, line.p2.x);
            std::swap(line.p1.y, line.p2.y);
        }
        int dx = line.p2.x-line.p1.x;
        int dy = line.p2.y-line.p1.y;
        int dx2 = dx * 2;
        int derror2 = std::abs(dy)*2;
        int error2 = 0;
        int y = line.p1.y;

        for (int x=line.p1.x; x <= line.p2.x; x++) {
            if (steep) {
                //db_t color = HsvToRgb(hue, sat, 250);//color;
                // printf("fb line X.%d, Y.%d?\n", x, y);
                //disp->buffer1[disp->width*x + y] = color;
            } else {
                db_t color =  HsvToRgb(hue, sat, 250);//color;
                // printf("fb line X.%d, Y.%d?\n", x, y);
                disp->buffer1[disp->width*y + x] = color;
            }
            error2 += derror2;
            if (error2 > dx) {
                y += (y1>y0?1:-1);
                error2 -= dx2;
            }
        }
    }
    */

    // take an x/y floating point and convert to 4 sample points with fractional components
    xy_samp_t point_to_sample(float x1, float y1, s16 dx, s16 dy) {
        s16 x2, x3, y2, y3;
        float xf = (fPartOfNumber(x1));
        float yf = (fPartOfNumber(y1));

        //printf("DX %d DY %d: %f=%f, %f=%f\n", dx, dy, x1, xf, y1, yf);

        // find previous y pixel depending on direction
        y2 = CLAMP480(iPartOfNumber(y1));
        y3 = CLAMP480((dy > 0) ? y2 +1 : y2 -1);

        // find previous x pixel depending on direction
        x2 = CLAMP640(iPartOfNumber(x1));
        x3 = CLAMP640((dx > 0) ? x2 +1 : x2 -1);
       
        xy_samp_t p = {
            xy_t{x2, y2}, xy_t{x3, y3},
            xf, yf, 
            pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x2, y2}),
            0
        };

        //printf("sample, dx/dy %d/%d, x2/3 %d,%d\n", dx, dy, x2, x3);
        return p;
    }

    float vertical_gain_multiplier(float vertical_angle) {
        float gain_vert = 1.0f;

        if (vertical_angle < -5.7f) {
            gain_vert = core::value_map(vertical_angle, -5.7f, -17, 0.99055f, 0.96620f);
        } else if (vertical_angle < 0.0f) {
            gain_vert = core::value_map(vertical_angle, -5.7f, 0, 0.99055f, 1.0f);
        } else if (vertical_angle < 6.4f) {
            gain_vert = core::value_map(vertical_angle, 0, 6.4f, 1.0f, 1.0f);
        } else if (vertical_angle < 16.14f) {
            gain_vert = core::value_map(vertical_angle, 6.4f, 16.14f, 1.0f, 0.97126f);
        } else if (vertical_angle < 22.32f) {
            gain_vert = core::value_map(vertical_angle, 16.14f, 22.32f, 0.97126f, 0.96480f);
        } else if (vertical_angle < 27.52f) {
            gain_vert = core::value_map(vertical_angle, 22.32f, 27.52f, 0.96480f, 0.95573f);
            //gain_vert = core::value_map(vertical_angle, 22.32f, 27.52f, 0.96480f, 0.94573f);
        } else if (vertical_angle < 30.97f) {
            gain_vert = core::value_map(vertical_angle, 27.52f, 30.97f, 0.95573f, 0.94680f);
            //gain_vert = core::value_map(vertical_angle, 27.52f, 30.97f, 0.94573f, 0.92680f);
        } else if (vertical_angle < 35.25f) {
            gain_vert = core::value_map(vertical_angle, 30.97f, 35.25f, 0.94680f, 0.917350f);
            //gain_vert = core::value_map(vertical_angle, 30.97f, 35.25f, 0.92680f, 0.89f);
        } else if (vertical_angle < 39.16f) {
            gain_vert = core::value_map(vertical_angle, 35.25f, 39.16, 0.917350f, 0.868947f);
            //gain_vert = core::value_map(vertical_angle, 35.25f, 39.16, 0.89f, 0.81f);
        } else if (vertical_angle < 42.36f) {
            gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.76680f);
            //gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.77680f);
            //gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.65);
        } else {
            gain_vert = core::value_map(vertical_angle, 42.36f, 44.16f, 0.76680f, 0.7250f);
            //gain_vert = core::value_map(vertical_angle, 42.36f, 44.16f, 0.77680f, 0.765f);
            //gain_vert = core::value_map(vertical_angle, 42.36f, 44.5f, 0.72f, 0.65f);
        }

        return gain_vert;
        
        // gain vert is the vertical lens correction gain value from Andreas measurements
        float corrected_gain = 0.0f;
        float angle_dist = abs(vertical_angle);
        if (vertical_angle < 0) {
            corrected_gain = sinf(1.57f + (angle_dist * 0.028f)) * gain_vert;
        } else {
            corrected_gain = sinf(1.57f + (angle_dist * 0.013f)) * gain_vert;
        }


        return corrected_gain;
    }

    offset_samp_t point_to_sample2(float x1, float y1, uint src_width, uint dst_offset, uint slice) {
        s16 x, y;
        //float xf = (fPartOfNumber(x1)) - 0.5f;
        //float yf = (fPartOfNumber(y1)) - 0.5f;
        float xf = fPartOfNumber(x1);
        float yf = fPartOfNumber(y1);

        // find previous y pixel depending on direction
        //y = CLAMP480(iPartOfNumber(y1));
        //y = (yf < 0.5f) ? y-1 : y;
        y = (y1 <= 0.0f || y1 > 640.0f) ? 0 : roundf(y1);

        // find previous x pixel depending on direction
        //x = CLAMP640(iPartOfNumber(x1));
        //x = (xf < 0.5f) ? x-1 : x;
        x = (x1 <= 0.0f || x1 > 640.0f) ? 1 : roundf(x1);

        xf = (xf >= 0.5f) ? xf - 0.5f : 0.5f + xf;
        yf = (yf >= 0.5f) ? yf - 0.5f : 0.5f + yf;

        uint src_offset = (y*src_width)+x;
        float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x, y});
        u16 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 310);
        VixelCalibration *calibration = find_calibration_for_radian(angle);

        float vertical_angle = core::value_map(dist, 236, 282, 1.0f, -15.0f);
        if (dist <= 236) {
            vertical_angle = core::value_map(dist, 130, 236, 45.0f, 1.0f);
        }


       
        offset_samp_t p = {
            src_offset,
            dst_offset,
            xf, yf,
            angle,
            calibration->gain * vertical_gain_multiplier(vertical_angle),
            calibration->offset,
            dist,
            vertical_angle * 0.0174533f,
            vertical_angle * 0.0174533f,
            (u16) slice,
            false,
            0, // slice (previously implicitly zero-initialized)
            0  // bottom (previously implicitly zero-initialized)
        };


        return p;
    }


    // take an x/y floating point and convert to 4 sample points with fractional components
    xyf_samp_t point_to_sample_float(float x1, float y1, float dx, float dy) {
        float x2, x3, y2, y3;
        float xf = (fPartOfNumber(x1));
        float yf = (fPartOfNumber(y1));

        //printf("%f=%f, %f=%f\n", x1, xf, y1, yf);

        // find previous y pixel depending on direction
        y2 = CLAMP480(iPartOfNumber(y1));
        y3 = CLAMP480((dy > 0) ? y2+1 : y2-1);

        // find previous x pixel depending on direction
        x2 = CLAMP640(iPartOfNumber(x1));
        x3 = CLAMP640((dx > 0) ? x2+1 : x2-1);
       
        //xyf_samp_t p = {x2, y2, x3, y3, xf, yf};
        xyf_samp_t p = {x1, y1, x3, y3, xf, yf};
        return p;
    }


    /**
     * draw a floating point line using naive implementation
     */
    std::vector<xyf_samp_t> line_to_vector_float(linef_t line, int segments) {
        line.p1.x = CLAMP640(line.p1.x);
        line.p2.x = CLAMP640(line.p2.x);
        line.p1.y = CLAMP480(line.p1.y);
        line.p2.y = CLAMP480(line.p2.y);
       
        //compute the slope
        float dx = line.p2.x - line.p1.x;
        float dy = line.p2.y - line.p1.y;

        float gradient_x = (dx / segments);
        float gradient_y = (dy / segments);

        // printf("gradient x/y [%f/%f] X:%f, total:%f\n", gradient_x, gradient_y, dx+dy, gradient_x+gradient_y);

        std::vector<xyf_samp_t> points;
        points.reserve(segments);

        float x1 = line.p1.x;
        float y1 = line.p1.y;
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        printf("gradient %f - %f = %f . %f\n", line.p2.x, line.p1.x, gradient_x, gradient_y);

        for (int i=1; i<segments; i++) {
            points.emplace_back(point_to_sample_float(x1, y1, dx, dy));
            x1 += gradient_x;
            y1 += gradient_y;
        }

        points.emplace_back(point_to_sample_float(x1, y1, dx, dy));

        //xyf_samp_t s1 = points.at(1);
        //xyf_samp_t s2 = points.at(50);
        //printf("line length: [%f] [%f]\n", core::calc_distance_float(s1.x1, s1.y1, s2.x1, s2.y2), core::calc_distance_float(line.p1.x, line.p1.y, line.p2.x, line.p2.y));
        #pragma GCC diagnostic pop

        return points;
    }


    std::vector<Point2D_t> line_to_point_vector(linef_t line, int segments) {
        std::vector<Point2D_t> points;

        // compute minimum segments and slopes
        // we add 1.02 extra pixels to ensure that we dont accidentally miss a pixel, this will double sample about 8 pixels but make sure we don't miss any 
        //int segments = std::max(std::abs(line.p2.x - line.p1.x), std::abs(line.p2.y - line.p1.y)) * 1.02;
        float gradient_x = core::step(segments, line.p1.x, line.p2.x);
        float gradient_y = core::step(segments, line.p1.y, line.p2.y);

        float x1 = line.p1.x;
        float y1 = line.p1.y;
        
        // add each point to list
        for (int i=0; i<segments; i++) {
            Point2D_t p = {static_cast<int16_t>(x1), static_cast<int16_t>(y1), 0, 0};
            if (x1 >= 0 && x1 < CAM_WIDTH && y1 >= 0 && y1 < CAM_HEIGHT) {
                points.push_back(p);
            }
            //points.emplace_back(p);
            x1 += gradient_x;
            y1 += gradient_y;
        }

        // return the list of points
        return points;
    }


    /**
     * draw a naive floating point line and return vector with segment points in it
     */
    std::vector<offset_samp_t> line_to_vector(line_t line, int segments, uint src_width, uint dst_width, uint segment_number) {
        line.p1.x = CLAMP640(line.p1.x);
        line.p2.x = CLAMP640(line.p2.x);
        line.p1.y = CLAMP480(line.p1.y);
        line.p2.y = CLAMP480(line.p2.y);

        // PANIC("line: [%d.%d] [%d.%d]", line.p1.x, line.p1.y, line.p2.x, line.p2.y);

        // bind first parameter (segments to the step function)
        //auto segment_step = std::bind(core::step<int16_t>, segments, std::placeholders::_1, std::placeholders::_2);

        // compute the slope for each line
        float gradient_x = core::step(segments, line.p1.x, line.p2.x);
        float gradient_y = core::step(segments, line.p1.y, line.p2.y);

        std::vector<offset_samp_t> points;
        points.reserve(segments);

        float x1 = line.p1.x;
        float y1 = line.p1.y;
        //uint doffset = dst_width - segment_number;
        uint doffset = (segment_number + (dst_width/2))%dst_width;
        //printf("dst width: %d , %d\n", dst_width, segments);
        
        // TODO: WHY IS point to sample not adding fractional points ??????
        for (int i=1; i<segments; i++) {
            offset_samp_t o = point_to_sample2(x1, y1, src_width, doffset, i);
            printf("sample src: %d, dst: %d\n", o.src_off, o.dst_off);

            points.emplace_back(o);
            x1 += gradient_x;
            y1 += gradient_y;

            // advance 1 pix down
            doffset += dst_width;
        }
        auto point = point_to_sample2(line.p2.x, line.p2.y, src_width, doffset, segments);
        point.bottom = true;
        points.emplace_back(point);

        return points;
    }

    /**
     * return the value of a sample at a sample location
     */
    static inline uint16_t sample_point(uint16_t *src, u16 width, xy_samp_t *p) {

        //return src[(p->p1.y*width)+p->p1.x] * 1.43f;

        u32 off1 = (u32)(p->p1.y * width);
        u32 off2 = (u32)(p->p2.y * width);
        
        float v1 = src[off1+(u16)(p->p1.x)];
        float v2 = src[off1+(u16)(p->p2.x)];
        float v3 = src[off2+(u16)(p->p1.x)];
        float v4 = src[off2+(u16)(p->p2.x)];

    
        // upper left + upper right
        // lower left + lower right
        float xa = (v1 * (1-p->xf) * (1-p->yf)) + (v2 * (p->xf * (1-p->yf)));
        float xb = (v3 * (1-p->xf) * p->yf) + (v4 * (p->xf * p->yf));

        #ifdef MAX_RANGE
            return CLAMP((xa + xb), 0, MAX_RANGE);
        #elif MAX_VALUE
            return MAX((xa + xb), MAX_VALUE);
        #else
            return (xa + xb);
        #endif
    }
    

    /**
     * @brief find the distance to the nearest angle on the line
     * 
     * @param slice 
     * @param data 
     */
    void find_min_slice_dist(std::vector<u16> &slice, SliceData *data) {
        u16 len = slice.size();
        if (len < 7) { 
            data->min = 100;
            data->max = 100;
            data->mean = 100;
            data->mode = 100;
            return;
        }
        
        std::sort(slice.begin( ), slice.end( ), [](const auto& lhs, const auto& rhs) { 
            return lhs<rhs;
        });

        data->max = (slice.at(len - 3) + slice.at(len - 4))/2;
        data->min = (slice.at(4));// + slice.at(4))/2;
        data->mode = slice.at(len/2);
    }


    /**
     * todo: map points from vertical strips to horizontal strips
     */
    u16 map_points_fastest(const std::vector<offset_samp_t> &points, uint16_t *src, std::vector<AngleDist> &distances) {
        
        distances.clear();
        
        UNUSED u8 highlight = 0;
        u16 dist_zero = 0;

        std::vector<u16> slice = std::vector<u16>();
        slice.reserve(180);
        
        for (std::size_t i=0; i<points.size(); ++i) {
            offset_samp_t sample = points[i];
            
            if (sample.bottom) {
                
                SliceData data = {};
                find_min_slice_dist(slice, &data);
                
                AngleDist d;
                d.angle = sample.angle;
                d.dist = (data.min * sample.gain) + sample.gain_offset;
                d.dist_max = (data.max * sample.gain) + sample.gain_offset;
                d.saturated = dist_zero;
                d.gain = sample.gain;
                d.offset = sample.gain_offset;
                
                slice.clear();
                distances.emplace_back(d);

                highlight = 0;
                dist_zero = 0;
                continue;
            }

            u16 t = src[sample.src_off];

            if (t < 150) {
                highlight = 3;
                dist_zero++;
                continue;
            } else {
                slice.emplace_back(t);
            }
        }
        
        return 0;
    }


    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_greyscale(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data, std::vector<AngleDist> &distances) {

        memset(jpeg_data, 0, 640*480);

        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        u16 dist = 9999;
        UNUSED const int height = data::get_frame_height();
        
        std::unordered_map<u16, u16> dist_map;

        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorize the gain and offset calculation...
        for(auto p : points) {
            

            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;

            // map greyscale jpeg image data
            UNUSED uint rgb_offset = p.dst_off * 3; 
            uint8_t v = 255-(dst[p.dst_off] / 26); // trim 6000 down to <255
            jpeg_data[p.src_off] = v;

            if (dist > 150) {
                u16 t = dist_map[p.slice];
                if (dst[p.dst_off] < t) {
                    dist_map[p.slice] = dst[p.dst_off];
                }
            }

            if (p.bottom) {
                AngleDist d;
                d.angle = p.slice;
                d.dist = dist_map[p.slice]; //(dist_map.min * sample.gain) + sample.gain_offset;
                d.dist_max = 0;//(data.max * sample.gain) + sample.gain_offset;
                d.saturated = //dist_zero;
                d.gain = p.gain;
                d.offset = p.gain_offset;
                
                distances.emplace_back(d);
            }


        }
        
        return true;
    }
    
    /**
     * create a list of all distance mappings
     */
    std::vector<AngleDist> create_distance_map() {
        std::vector<AngleDist> angles;
        for (int i=0; i<(ANGLE_POINTS+20); i++) {
            AngleDist d = {};
            angles.emplace_back(d);
        }
        return angles;
    }
    
    u8 calc_beta(u8 current_beta, int idist) {
        
        u8 result = 0x0C;
        int offsets[] = {2, 4, 8, 12};

        if (current_beta == 0x0A) { offsets[3]+=2; }
        if (current_beta == 0x08) { offsets[2]+=2; offsets[3]+=2; }
        if (current_beta == 0x04) { offsets[1]+=2; offsets[2]+=2; offsets[3]+=2; }
        if (current_beta == 0x01) { offsets[0]+=2; offsets[1]+=2; offsets[2]+=2; }
        
        if (idist <= offsets[0]) { result = 0x01; }
        else if (idist <= offsets[1]) { result = 0x04; }
        else if (idist <= offsets[2]) { result = 0x08; }
        else if (idist <= offsets[3]) { result = 0x0A; }
        
        return result;
    }

    float normalize_12bit_int(u16 dist) {
        return dist / 2^12;
    }




    /**
     * todo: map points from vertical strips to horizontal strips
     * UGLY I know, but, we need to be fast.  good fit between
     * 
     */
    void calibrate(std::unordered_map<int, std::vector<offset_samp_t>> &point_map, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {
        if (GOPTS->color_mode == 4 && GOPTS->frame_request == false) { return; }
        static u32 frame_number = 0;
        auto move_ctr = gfx::fwd_move_point(GOPTS->center_x, GOPTS->center_y);

        using SampleVect = std::vector<offset_samp_t>;
        using SampleMap = std::unordered_map<int, SampleVect>;
               
        frame_number++;

        int fz = data::get_frame_size_bytes();
        if (GOPTS->color_mode >= 0) { memset(jpeg_data, 0, (GOPTS->color_mode == 3) ? fz * 3 : fz); }

        // don't run calibration on the IR data:
        bool chitoncal = data::get_chiton_calibration();
        if (!GOPTS->depth_frame) { chitoncal = false; }

        if (GOPTS->color_mode == 0 && !chitoncal) { memcpy(dst, src, 640*480*sizeof(u16)); }

        
        int o1 = 0;
        // TODO: make this a non-const for updates...
        //for (SampleMap::const_iterator it = point_map.begin(); it != point_map.end(); it++) {
        for (SampleMap::iterator it = point_map.begin(); it != point_map.end(); it++) {
            auto i = it->first;
            
            offset_samp_t p = {};
            
            if (GOPTS->color_mode == 4) {
                float x, y, z, z1 = 0.0f;
                s16 *pc = data::get_point_cloud();
                for (uint j=0; j<it->second.size(); j++) {
                    p = it->second[j];
                    if (src[p.src_off] > 4000 || src[p.src_off] < 200) { continue; }
                    //u16 t = (src[p.src_off] + p.gain_offset) * p.gain;
                    u16 t = (src[p.src_off] * p.gain) + p.gain_offset;

                    dst[p.dst_off] = (std::abs(t - dst[p.dst_off]) < 100) ? (dst[p.dst_off] * 0.7f) + (t * 0.3f) : t; 

                    pc[o1++] = -p.angle_sin * dst[p.dst_off] * p.vert_cos;
                    pc[o1++] = p.vert_sin * dst[p.dst_off];// * (0.7f + (std::abs(cosf(p.angle)) * 0.3f));
                    pc[o1++] = p.angle_cos * dst[p.dst_off] * p.vert_cos;


                    pc[o1++] = dst[p.dst_off];
                }
            }

            else if (GOPTS->color_mode < 1) {
                // black image for color mode 0, no image for -1
                if (chitoncal) {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                    }
                } else {
                    // printf("size: %d\n", it->second.size());
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off]);// * p.gain) + p.gain_offset;
                    }
                }
            }


            else if (GOPTS->color_mode == 1) {
                if (GOPTS->depth_frame) {
                    if (chitoncal) {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];

                            int y = floor(p.src_off / 640);
                            int x = p.src_off - (y * 640);
                            dst[p.dst_off] = (src[p.src_off] * p.gain);// + p.gain_offset;
                            if (x == GOPTS->point.x && y == GOPTS->point.y) { printf("%dx%d (%f) [%d]   %d => %d\n", x, y, static_cast<double>(p.gain), p.gain_offset, dst[p.dst_off], src[p.src_off]); }
                            jpeg_data[p.dst_off] = 255 - (dst[p.dst_off] / 26); // CREATE IMAGE FROM 16 bit 
                        }
                    } else {
                        float px, py, pz = 0.0f;
                        float x, y, z = 0.0f;
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off]);
                            jpeg_data[p.dst_off] = 255 - (src[p.src_off] / 16); // CREATE IMAGE FROM 12 bit 
                        }
                    }
                }
                // INVERT IR IMAGE ON GREYSCALE..
                else {
                    if (chitoncal) {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                            jpeg_data[p.dst_off] = (dst[p.dst_off] / 25); // CREATE IMAGE FROM 16 bit 
                        }
                    } else {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off]);
                            jpeg_data[p.dst_off] = (dst[p.dst_off] >> 4); // CREATE IMAGE FROM 12 bit 
                        }
                    }

                }
            }
            else if (GOPTS->color_mode == 2) {
                if (chitoncal) {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                        uint rgb_offset = p.dst_off * 3; 
                        uint8_t r = p.gain_offset + 128;
                        uint8_t v = std::clamp((u8)((p.gain-1.35f) * 400.0f), (u8)0, (u8)255);
                        jpeg_data[rgb_offset] = r;
                        jpeg_data[rgb_offset+1] = v;
                        jpeg_data[rgb_offset+2] = v;
                    }
                } else {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off]);
                        uint rgb_offset = p.dst_off * 3; 
                        uint8_t r = std::clamp(p.gain_offset + 128, 1, 254);
                        uint8_t v = std::clamp((u8)((p.gain-1.35f) * 400.0f), (u8)0, (u8)255);
                        jpeg_data[rgb_offset] = r;
                        jpeg_data[rgb_offset+1] = v;
                        jpeg_data[rgb_offset+2] = v;
                    }
                }

            }
            else if (GOPTS->color_mode == 3) {
                if (chitoncal) {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (dst[p.dst_off] * 0.5f) + ((src[p.src_off] * p.gain) * 0.5f);

if (frame_number  == 100) {
    float center_angle = PI;
    float upper = center_angle + 0.54f;
    float lower = center_angle - 0.54f;
                        //if ((p.angle2 > lower && p.angle2 < upper)) {
                        if ((p.angle+PI) > lower && (p.angle+PI) < upper) {

                            // expected X dist
                            /*
                            float f1 = std::abs(p.angle); // angle is really PI-angle
                            float d1 = core::absdiff_f(f1, 1.571f);
                            float s1 = sin(d1);
                            float sx = 1 / s1;
                            s16 ex = (s16)((float)(1232 * sx));

                            // expected Y dist
                            float f2 = std::abs(p.vert_rad); // angle is really PI-angle
                            float d2 = core::absdiff_f(f2, 1.571f);
                            float s2 = sin(d2);
                            float sy = 1 / s2;
                            s16 ey = (s16)((float)(1232 * sy));
                            */


                            float ang_xa = std::abs(PI - p.angle);
                            float ang_xc = (PI/2) - ang_xa;
                            float len_c = 1232;
                            s16 ex = len_c * (sin(ang_xa) / sin(ang_xc));

                            float ang_ya = std::abs(p.vert_rad);
                            float ang_yc = (PI/2) - ang_ya;
                            s16 ey = len_c * (sin(ang_ya) / sin(ang_yc));


                            s16 ez = sqrtf(ex*ex + ey*ey + len_c*len_c);


                            int y = floor(p.src_off / 640);
                            int x = p.src_off - (y * 640);

                            // distortion fix
                            float inv_a = std::abs(p.angle) - 2.6f;
                            float inv_s_a = sinf(inv_a);

                            float inv_v = std::abs(p.vert_rad);
                            float inv_c_v = cosf(inv_v);
                            float scaling_factor = 0.25f;

                            float fix = inv_c_v * inv_s_a * scaling_factor;
                            //printf("%d.%d inv_a: %f, inv_v: %f, sa: %f, cv: %f, MUL: %f\n", x, y, inv_a, inv_v, inv_s_a, inv_c_v, fix);


                            

                            float gain_diff = (float)ez / (float)dst[p.dst_off];
                            p.gain *= gain_diff;
                            //printf("set gain to: [%d] / [%d], (%f) [%f]\n", ez, dst[p.dst_off], gain_diff, p.gain);
                            it->second[j] = p;



                            //TODO: get distance input.  compute diff from expected to actual
                            // expected / actual (dst)

                            //s16 diff = core::absdiff(ez, factor);
                            //printf(" E [%.4d] R [%.d] F [%.4d] D [%d] %d.%d\n", expected, dst[p.dst_off], factor, diff, x, y);
                            //printf(" F1 (%f) (%f) (%f)\n",f1, d1, sf);
                            //if (y > 250 && y < 350 && p.angle < 3.24) {
                            //printf("%.3d.%.3d  EZ:%.4d  EX:%.4d, EY:%.4d   f1:(%f) f2:(%f), d1:(%f) d2:(%f) s1(%f) s2(%f) sx(%f) sy(%f)\n", x, y, ez, ex, ey, f1, f2, d1, d2, s1, s2, sx, sy);
                            //printf("%.3d.%.3d  EZ:%.4d  EX:%.4d, EY:%.4d   xa:(%f) xc:(%f), ya:(%f) yc:(%f)\n", x, y, ez, ex, ey, ang_xa, ang_xc, ang_ya, ang_yc);
                            //}
                        }
}
                        jpeg_data[p.dst_off] = 255 - (dst[p.dst_off] / 25); // CREATE IMAGE FROM 12 bit 
                    }
                } else {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (dst[p.dst_off] * 0.9f) + (src[p.src_off] * 0.1f);
                        jpeg_data[p.dst_off] = 255 - (src[p.src_off] / 16); // CREATE IMAGE FROM 12 bit 
                    }
                }
            }
        }

        GOPTS->frame_request = false;
        data::set_point_cloud_size(o1);
    }



   
    /**
     * todo: map points from vertical strips to horizontal strips
     * UGLY I know, but, we need to be fast.  good fit between
     * 
     */
    u32 map_points_greyscale2(const std::unordered_map<int, std::vector<offset_samp_t>> &point_map, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data, std::vector<AngleDist> &distances) {
        if (GOPTS->color_mode == 4 && GOPTS->frame_request == false) { return 0xC0C0C0C0; }

        static u32 frame_number = 0;
        auto move_ctr = gfx::fwd_move_point(GOPTS->center_x, GOPTS->center_y);

        using SampleVect = std::vector<offset_samp_t>;
        using SampleMap = std::unordered_map<int, SampleVect>;
               
        //u32 *bucket_totals = data::get_bucket_totals(frame_number);
        //u32 *bucket_counts = data::get_bucket_counts(frame_number++);
        frame_number++;
        u32 *tal_buckets = data::get_tal_buckets();


        //memset(bucket_counts, 0, sizeof(u32) * (ANGLE_POINTS+20) * BUCKET_SZ_RAW);
        //memset(bucket_totals, 0, sizeof(u32) * (ANGLE_POINTS+20) * BUCKET_SZ_RAW);
        memset(tal_buckets, 0, 7000);
        int fz = data::get_frame_size_bytes();
        if (GOPTS->color_mode >= 0) { memset(jpeg_data, 0, (GOPTS->color_mode == 3) ? fz * 3 : fz); }

        // don't run calibration on the IR data:
        bool chitoncal = data::get_chiton_calibration();
        if (!GOPTS->depth_frame) { chitoncal = false; }

        if (GOPTS->color_mode == 0 && !chitoncal) { memcpy(dst, src, 640*480*sizeof(u16)); }

        
        int hit = 0;
        int o1 = 0;
        for (SampleMap::const_iterator it = point_map.begin(); it != point_map.end(); it++) {
            auto i = it->first;
            // for some reason, dereferencing it->second here and using the de-ref:
            // SampleVect samples = it->second; is 3ms slower per frame
            
            // tal_idx should be number of tal lanes * bucket size (bucket size = distance bucket size (25))
            u8 tal_idx = ((u8)floorf(i / (ANGLE_POINTS / TAL_LANES))) * TAL_BUCKETS; // * BUCKET_SZ_RAW; // (1500/4 = 375) * 25
            u8 bucket_off = ((u8)floorf(i / (ANGLE_POINTS / 8))) * TAL_BUCKETS; // * BUCKET_SZ_RAW; // (1500/4 = 375) * 25
            
            
            offset_samp_t p = {};
            
            if (GOPTS->color_mode == 4) {
                float x, y, z, z1 = 0.0f;
                s16 x2, y2, z2 = 0;
                s16 *pc = data::get_point_cloud();
                for (uint j=0; j<it->second.size(); j++) {
                    p = it->second[j];
                    if (src[p.src_off] > 4000) { continue; }
                    u16 cal = (src[p.src_off] * p.gain) + p.gain_offset;
                    
                    u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                    tal_buckets[tal_idx+idx]++;
                    if (cal >= 400) {
                        pc[o1++] = p.angle_sin * cal;
                        pc[o1++] = p.vert_sin * cal;
                        pc[o1++] = p.angle_cos * cal;
                        pc[o1++] = cal;
                    }
                }
            }

            else if (GOPTS->color_mode < 1) {
                // black image for color mode 0, no image for -1
                if (chitoncal) {
                    //printf("size: %d\n", it->second.size());
                    //printf("idx: %d . [%d]\n", i, tal_idx);
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                        u8 idx = dst[p.dst_off] / BUCKET_DIST_CAL; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += src[p.src_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;
                    }
                } else {
                    // printf("size: %d\n", it->second.size());
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        //dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                        u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += src[p.src_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;
                    }
                }
            }


            else if (GOPTS->color_mode == 1) {
                // WARN("calibration size: %d", it->second.size());
                if (GOPTS->depth_frame) {
                    if (chitoncal) {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                            u8 idx = dst[p.dst_off] / BUCKET_DIST_CAL; // idx = distance bucklet 0-24
                            //bucket_totals[bucket_off+idx] += dst[p.dst_off];
                            //bucket_counts[bucket_off+idx]++;
                            //if (idx > 25 || tal_idx+idx >= 125) { printf("uh oh: %d/%d\n", tal_idx, idx); }
                            tal_buckets[tal_idx+idx]++;

                            jpeg_data[p.dst_off] = 255 - (dst[p.dst_off] / 26); // CREATE IMAGE FROM 16 bit 
                        }
                        //printf("line1\n");
                    } else {
                        float px, py, pz = 0.0f;
                        float x, y, z = 0.0f;
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off]);
                            u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                            //bucket_totals[bucket_off+idx] += src[p.src_off];
                            //bucket_counts[bucket_off+idx]++;
                            tal_buckets[tal_idx+idx]++;
                            jpeg_data[p.dst_off] = 255 - (src[p.src_off] / 16); // CREATE IMAGE FROM 12 bit 
                        }
                    }
                }
                // INVERT IR IMAGE ON GREYSCALE..
                else {
                    if (chitoncal) {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                            u8 idx = dst[p.dst_off] / BUCKET_DIST_CAL; // idx = distance bucklet 0-24
                            //bucket_totals[bucket_off+idx] += dst[p.dst_off];
                            //bucket_counts[bucket_off+idx]++;
                            tal_buckets[tal_idx+idx]++;

                            jpeg_data[p.dst_off] = (dst[p.dst_off] / 25); // CREATE IMAGE FROM 16 bit 
                        }
                    } else {
                        for (uint j=0; j<it->second.size(); j++) {
                            p = it->second[j];
                            dst[p.dst_off] = (src[p.src_off]);
                            u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                            //bucket_totals[bucket_off+idx] += src[p.src_off];
                            //bucket_counts[bucket_off+idx]++;
                            tal_buckets[tal_idx+idx]++;
                            jpeg_data[p.dst_off] = (dst[p.dst_off] >> 4); // CREATE IMAGE FROM 12 bit 
                        }
                    }

                }
            }
            else if (GOPTS->color_mode == 2) {
                if (chitoncal) {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                        u8 idx = dst[p.dst_off] / BUCKET_DIST_CAL; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += dst[p.dst_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;
                        
                        uint rgb_offset = p.dst_off * 3; 
                        uint8_t r = p.gain_offset + 128;
                        uint8_t v = std::clamp((u8)((p.gain-1.35f) * 400.0f), (u8)0, (u8)255);
                        jpeg_data[rgb_offset] = r;
                        jpeg_data[rgb_offset+1] = v;
                        jpeg_data[rgb_offset+2] = v;
                    }
                } else {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off]);
                        u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += src[p.src_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;
                        
                        uint rgb_offset = p.dst_off * 3; 
                        uint8_t r = std::clamp(p.gain_offset + 128, 1, 254);
                        uint8_t v = std::clamp((u8)((p.gain-1.35f) * 400.0f), (u8)0, (u8)255);
                        jpeg_data[rgb_offset] = r;
                        jpeg_data[rgb_offset+1] = v;
                        jpeg_data[rgb_offset+2] = v;
                    }
                }

            }
            else if (GOPTS->color_mode == 3) {
                if (chitoncal) {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                        u8 idx = dst[p.dst_off] / BUCKET_DIST_CAL; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += dst[p.dst_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;

                        uint rgb_offset = p.dst_off * 3; // RGB IMAGE
                        depth_to_hsv_raw(dst + p.dst_off, jpeg_data + rgb_offset);
                    }
                } else {
                    for (uint j=0; j<it->second.size(); j++) {
                        p = it->second[j];
                        dst[p.dst_off] = (src[p.src_off]);
                        u8 idx = src[p.src_off] / BUCKET_DIST_RAW; // idx = distance bucklet 0-24
                        //bucket_totals[bucket_off+idx] += src[p.src_off];
                        //bucket_counts[bucket_off+idx]++;
                        tal_buckets[tal_idx+idx]++;

                        uint rgb_offset = p.dst_off * 3; // RGB IMAGE
                        depth_to_hsv_raw(dst + p.dst_off, jpeg_data + rgb_offset);
                    }
                }
            }

            /*
            AngleDist d = distances[i];
            d.angle = p.angle; // angle in radians
            d.angle_num = i;   // angle index
            d.saturated = bucket_counts[bucket_off+0] + bucket_counts[bucket_off+1] + bucket_counts[bucket_off+2]; // number of pixels that are saturated (< 480mm)
            d.dist = 5;

            
            // if we have < 10 saturated pixels
            if (d.saturated < 10) {
                // take the first distance bucket with > 6 pixels
                for (uint k=bucket_off; k<bucket_off+25; k++) {
                    if (bucket_counts[k] > 6) {
                        d.dist = bucket_totals[k] / bucket_counts[k];
                        if (!chitoncal) { d.dist = (d.dist * p.gain) + p.gain_offset; }
                        break;
                    }
                }
            }
            
            // draw the distance plot
            if (GOPTS->draw_slam) {
                xy_t plot = move_ctr(gfx::calculate_circle_point(((u16)d.dist / 18), p.angle));
                if (GOPTS->color_mode <= 1) {
                    jpeg_data[(CLAMP480(plot.y) * 640) + CLAMP640(plot.x)] = 255;
                } else {
                    uint off = (CLAMP480(plot.y) * 640) + CLAMP640(plot.x);
                    off *= 3;
                    jpeg_data[off] = 255;
                    jpeg_data[off+1] = 255;
                    jpeg_data[off+2] = 255;
                }
            }

            distances[i] = d;
            bucket_off += BUCKET_SZ_RAW; // bucket angle index
            */
        }

        GOPTS->frame_request = false;
        
        
        data::set_point_cloud_size(o1);
        //PANIC_IF(false, "shutdown", -1);
        //
        if (hit) { PANIC("HIT!"); GOPTS->running = 0; }
        
        int min_d = 6000;
        for (auto d : distances) {
            if (d.angle_num<500) {
                min_d = MIN(d.dist, min_d);
            }
        }
        
        // mark the last pixel
        if (GOPTS->color_mode == 4) {
        }

        
        // compute min distance
        /*
        for (int vixel=0; vixel<8; vixel++) {
            int vdist = 20;
            for (int i=0; i<=20; i++) {
                if (bucket_counts[i*vixel] > 1500) {
                    vdist = i;
                    break;
                }
            }
            if (vixel == 0) { GOPTS->min_right = vdist; }
            if (vixel == 3) { GOPTS->min_front = vdist; }
            if (vixel == 6) { GOPTS->min_left = vdist; }
        }
        */
        
        
        u32 beta = data::get_beta();
        u32 beta_out = 0;
        int idist = 25;

        // TODO: make this a function
        int l1 = (beta & 0x00FF0000)>>16;
        for (int i=0; i<=15; i++) {
            if (tal_buckets[i] > 1500) {
                idist = i;
                break;
            }
        }
        int l2 = calc_beta(l1, idist);
        beta_out += (l2<<16);

        l1 = (beta & 0x000000FF);
        idist = 25;
        for (int i=(TAL_BUCKETS*1); i<=((TAL_BUCKETS*1)+17); i++) {
            //printf(" .%d{%d} ", i, tal_buckets[i]);
            if (tal_buckets[i] > 1500) {
                idist = i-(TAL_BUCKETS*1);
                break;
            }
        }
        l2 = calc_beta(l1, idist);
        //printf("l1 : [%x] [%d] = (%x)\n", l1, idist, l2);
        beta_out += (l2);

        l1 = (beta & 0xFF000000)>>24;
        idist = 25;
        for (int i=(TAL_BUCKETS*2); i<=((TAL_BUCKETS*2)+17); i++) {
            //printf(" .%d{%d} ", i, tal_buckets[i]);
            if (tal_buckets[i] > 1500) {
                idist = i-(TAL_BUCKETS*2);
                break;
            }
        }
        l2 = calc_beta(l1, idist);
        //printf("l1 : [%x] [%d] = (%x)\n", l1, idist, result);
        beta_out += (l2<<24);
        
        l1 = (beta & 0x0000FF00)>>8;
        idist = 25;
        for (int i=(TAL_BUCKETS*3); i<=((TAL_BUCKETS*3)+17); i++) {
            //printf(" .%d{%d} ", i, tal_buckets[i]);
            if (tal_buckets[i] > 1500) {
                idist = i-(TAL_BUCKETS*3);
                break;
            }
        }
        l2 = calc_beta(l1, idist);
        //printf("l1 : [%x] [%d] = (%x)\n", l1, idist, l2);
        beta_out += (l2<<8);
        
        if ((frame_number % 60) == 1) { INFO("BETA: 0x%08x", beta_out); }


        data::set_beta(beta_out);
        return beta_out;

        //int level = beta_to_level(beta);

        // each i element has 25 "sub elements" following it for the 25 depth positions at that index
        /*
        int buckets[25] = {0};
        for (int i=0; i<375; i++) {
            printf("%d: %d\n", i, tal_buckets[i]);
        }
        */

        /*
        if (tal_buckets[0] > 100 && BYTE(beta, 0) > 1) {
            DEC_BYTE(beta, 0);
        }
        
        */
        
        /*
        u32 v = 
            (std::clamp(0xC - (u8)(tal_buckets[0] * .02), 0, 0xC)) +
            (std::clamp(0xC - (u8)(tal_buckets[25] * .02), 0, 0xC) << 8) +
            (std::clamp(0xC - (u8)(tal_buckets[50] * .02), 0, 0xC) << 16) +
            (std::clamp(0xC - (u8)(tal_buckets[75] * .02), 0, 0xC) << 24);

        static u16 limits[4] =  {0,0,0,0};
        u8 z1 = beta & 0xFF;
        //if (z1 == 0x0C) { limits[0] = }

        if (tal_buckets[2] > 1000) {
            printf ("3 (%x). %d . %d . %d . %d . %d\n", z1, tal_buckets[0], tal_buckets[1], tal_buckets[2], tal_buckets[3], tal_buckets[4]);
            beta = 0x01010103;
        } else if (tal_buckets[3] > 1000) {
            printf ("6 (%x). %d . %d . %d . %d . %d\n", z1, tal_buckets[0], tal_buckets[1], tal_buckets[2], tal_buckets[3], tal_buckets[4]);
            beta = 0x01010106;
        } else if (tal_buckets[4] > 1000) {
            printf ("9 (%x). %d . %d . %d . %d . %d\n", z1, tal_buckets[0], tal_buckets[1], tal_buckets[2], tal_buckets[3], tal_buckets[4]);
            beta = 0x01010109;
        } else {
            printf ("C (%x). %d . %d . %d . %d . %d\n", z1, tal_buckets[0], tal_buckets[1], tal_buckets[2], tal_buckets[3], tal_buckets[4]);
            beta = 0x010101C0;
        }
        */
        return 0x0C0C0C0C;//data::get_beta();
        //return beta;
    }
   

    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_gain(const std::vector<offset_samp_t> &points, UNUSED uint16_t *src, unsigned char *jpeg_data) {
        

        memset(jpeg_data, 0, 640*480*3);

        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorize the gain and offset calculation...
        for(auto p : points) {
            // map greyscale jpeg image data
            uint rgb_offset = p.dst_off * 3; 

            uint8_t r = p.gain_offset + 128;
            uint8_t v = std::clamp((u8)((p.gain-1.35f) * 400.0f), (u8)0, (u8)255);

            jpeg_data[rgb_offset] = r;
            jpeg_data[rgb_offset+1] = v;
            jpeg_data[rgb_offset+2] = v;
        }
        
        return true;
    }
 
    

    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_slam(const std::vector<offset_samp_t> &points, uint16_t *src, UNUSED uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        std::unordered_map<u16, u16> dist_map;
        
        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        UNUSED u16 dist = 9999;
        const int height = data::get_frame_height();

        std::memset(jpeg_data, 0, 640*480*3);


        const int rows = points.size() / height;
        //offset_samp_t *p;
        uint idx=0;

        auto p = points[0];
        for (int y=0; y<rows; y++) {
            u16 closest = 16000;
            for (int x=0; x<height-20; x++) {
                p = points[idx++];
                closest = std::min(closest, src[p.src_off]);
            }
            float angle = p.angle * (float)M_PI / 180; 

            int dx = closest * cosf(angle);
            int dy = closest * sinf(angle);

            idx += 20;
            INFO("y: %d, idx: %d, %f, %d, %d, %d\n", y, idx, (double)p.angle, closest, dx, dy);
        }

        return true;
    }



    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_hsv(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        //printf("map to mem at [%x]\n", dst);

        for(auto p : points) {
            
            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;

            // map hsv jpeg image data
            uint rgb_offset = p.dst_off * 3; 
            depth_to_hsv_raw(dst + p.dst_off, jpeg_data + rgb_offset);
        }

        return true;
    }




    /**
     * create a series of points connecting all elements in points1 
     * with all elements of points2, each line_len elements long
     * 
     * returns a vector of xy_samp_t points1.size() * line_len long
     * 
     * TODO: deleteme 
     */
    std::vector<xy_samp_t> connect_vector_points(std::vector<xy_t> points1, std::vector<xy_t> points2, int line_len) {
        std::vector<xy_samp_t> all_points;
        Return_IfNot(points1.size() == points2.size(), "vector lists must be the same size", all_points);
        all_points.reserve(line_len * points1.size());
        

        /*
        int len=all_points.size();
        for (int i=0; i<len; i++) {
            line_t new_line = {points1.at(i), points2.at(i)};
            // OFFSET FIX REMOVE THIS: fplus::append(all_points, gfx::line_to_vector(new_line, line_len));
        }
        */

        return all_points;
    }


    
    std::vector<offset_samp_t> make_all_slam_points_int() {
        /*
        std::vector<offset_samp_t> all_points;
        all_points.reserve(FRAME_PLANAR_WIDTH_DEFAULT);
        
        std::vector<xy_t> circle = circle_points(GOPTS->center_x, GOPTS->center_y, FRAME_PLANAR_WIDTH_DEFAULT, 200);
        
        for (auto point : circle) {
            float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, point);
            

        }
        */
        TRACE("");
        const int width = data::get_frame_width();
        const int height = data::get_frame_height();

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * ONE_ONE_THIRD, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * ONE_ONE_THIRD, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        std::vector<offset_samp_t> all_points;
        const int len = inner.size();
        all_points.reserve(height * len);

        for (int i=0; i<len; i++) {
            xy_t p1 = inner.at(i);
            xy_t p2 = outer.at(i);
            line_t l1 = {p1, p2};
            
            std::vector<offset_samp_t> p = gfx::line_to_vector(l1, height, CAM_WIDTH, len, i);
            all_points.insert(all_points.end(), p.begin(), p.end());
        }

        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }


    std::vector<offset_samp_t> make_all_raw_points_int() {
        std::vector<offset_samp_t> all_points;
        all_points.reserve(CAM_HEIGHT * CAM_WIDTH);

        float xf = 0.0f;
        float yf = 0.0f;

        WARN("center %d.%d", GOPTS->center_x, GOPTS->center_y);

        for (int16_t y=0; y<CAM_HEIGHT; y++) {
            for (int16_t x=0; x<CAM_WIDTH; x++) {
                
                float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x, y});
                VixelCalibration *calibration = find_calibration_for_radian(angle);
                float gain = calibration->gain;
                s16 offset = (s16)calibration->offset;

                //u8 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 250);
                u16 dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
                if (dist > GOPTS->outer_dist || dist < GOPTS->inner_dist) {
                    continue;
                    gain = 0;
                    offset = 0;
                }
                if (angle > -0.77f && angle < 0.77f) {
                    continue;
                    gain = 0;
                    offset = 0;
                }

                float vertical_angle = core::value_map(dist, 239, 282, 0.0f, -14.3f);
                if (dist <= 239 && dist >= 205) {
                    vertical_angle = core::value_map(dist, 205, 239, 13.9f, 0.0f);
                } else if (dist <= 205 && dist >= 173) {
                    vertical_angle = core::value_map(dist, 173, 205, 26.3f, 13.9f);
                } else if (dist <= 173 && dist >= 149) {
                    vertical_angle = core::value_map(dist, 149, 173, 36.59f, 26.3f);
                } else if (dist <= 149 && dist >= 135) {
                    vertical_angle = core::value_map(dist, 135, 149, 44.7f, 36.59f);
                } 

                offset_samp_t p = {
                    (uint)(y*CAM_WIDTH)+x,
                    (uint)(y*CAM_WIDTH)+x,
                    xf, yf,
                    angle,
                    gain * vertical_gain_multiplier(vertical_angle),
                    offset,
                    dist,
                    vertical_angle * 0.0174533f,
                    (u16)y,
                    false,
                    0.0f, // vert_rad (previously implicitly zero-initialized)
                    0,    // slice (previously implicitly zero-initialized)
                    0     // bottom (previously implicitly zero-initialized)
                };

                all_points.emplace_back(p);
            }
        }

        return all_points;
    }
    

    std::vector<offset_samp_t> make_all_polar_points_int() {
        
        const int width = FRAME_POLAR_WIDTH_DEFAULT;
        const int height = FRAME_POLAR_HEIGHT_DEFAULT;

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        std::vector<offset_samp_t> all_points;
        const int len = inner.size();
        all_points.reserve(height * len);

        for (int i=0; i<len; i++) {
            xy_t p1 = inner.at(i);
            xy_t p2 = outer.at(i);
            line_t l1 = {p1, p2};

            std::vector<offset_samp_t> p = gfx::line_to_vector(l1, height, CAM_WIDTH, len, i);
            fplus::transform([](offset_samp_t s) { s.dst_off = s.src_off; if (s.angle > 0.77f || s.angle < 0.77f) { s.gain = 0; s.gain_offset = 0; } return s; }, p);
            all_points.insert(all_points.end(), p.begin(), p.end());
        }
        return all_points;
    }

    /**
     * find the target closest index in a vector
     */
    int find_closest_index(std::vector<float> *angles, float target) {
        float dist1 = 999.0f;
        int index = 0;
        
        for (uint i=0; i<angles->size(); i++) {
            float d = std::abs(angles->at(i) - target);
            if (d < dist1) {
                dist1 = d;
                index = i;
            }
        }
        
        return index;
    }
    
    
    /**
     * take an unordered list of samples and return a map of ANGLE_POINTS list of sample points
     * the map vactors will be ordered from inside out
     */
    std::unordered_map<int, std::vector<offset_samp_t>> order_samples(std::vector<offset_samp_t> &input) {

        std::vector<offset_samp_t> calibrated;
        calibrated.reserve(input.size());
        std::unordered_map<int, std::vector<offset_samp_t>> samples;
         
        // add each sample to the map of slices
        for (auto sample : input) {
            if (samples.find(sample.slice) == samples.end()) {
                std::vector<offset_samp_t> v;
                samples[sample.slice] = v;
            }
            samples[sample.slice].emplace_back(sample);
        }

        // loop over all map indexes and order them from inside out
        std::unordered_map<int, std::vector<offset_samp_t>>::iterator it = samples.begin();
        while(it != samples.end()) {
            std::vector<offset_samp_t> s = it->second;
            std::sort( s.begin( ), s.end( ), []( const auto& lhs, const auto& rhs ) { 
                return lhs.center_dist < rhs.center_dist;
            } );
            it++;
        }
        
        return samples;
    }
    
    
    /**
     * 
     */
    std::vector<offset_samp_t> make_all_polar_points_int2() {
        
        std::vector<offset_samp_t> all_points;
        xy_t center = {GOPTS->center_x, GOPTS->center_y};
        
        std::vector<float> point_locks;
        float interval = ((PI*2)/1500) * (2.37f/3.14f); // create 1500 points ratio for 270deg (2.37rad)
        for (float i=-PI; i<PI; i+=interval) {
            if (i > -0.77f && i < 0.77f) { continue; }
            point_locks.emplace_back(i);
        }
        

        const float p1 = 21.580174f;
        const float p2 = -120.472658f;
        const float p3 = 121.429266f;
        const float pix_sz_mm = 0.0056f;

        for (int16_t x=0; x<FRAME_POLAR_WIDTH_DEFAULT; x++) {
            for (int16_t y=0; y<FRAME_POLAR_HEIGHT_DEFAULT; y++) {
                
                xy_t pixel = {x, y};
                uint32_t offset = (y * FRAME_POLAR_WIDTH_DEFAULT) + x;
                float angle = -pixel_to_radians(center, pixel);
                u16 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 310);
                float dist_f = core::calc_distance_float(GOPTS->center_x, GOPTS->center_y, x, y);
                //dist_f *= pix_sz_mm;
                
                if (angle > -0.785f && angle < 0.785f) { continue; }
                if (dist < 137 || dist < GOPTS->inner_dist || dist > GOPTS->outer_dist || dist > 285) { continue; }
                
                VixelCalibration *calibration = find_calibration_for_radian(angle);
                calibration->gain = 1.46484f;

                float vertical_angle = 0;
                if (dist >= 261 && dist <= 285) {
                    vertical_angle = core::value_map(dist, 261, 285, -7.633f, -15.0f);
                    //vertical_angle = core::value_map(dist, 261, 285, -10.0f, -23.0f);
                } else if (dist >= 240 && dist <= 261) {
                    vertical_angle = core::value_map(dist, 240, 261, 0.0f, -7.633f);
                } else if (dist >= 223 && dist <= 240) {
                    //vertical_angle = core::value_map(dist, 223, 240, 0.0f, 7.633f);
                    //vertical_angle = core::value_map(dist, 240, 223, 0.0f, 7.633f);
                    vertical_angle = core::value_map(dist, 223, 240, 7.633f, 0.0f);
                } else if (dist >= 204 && dist <= 223) {
                    //vertical_angle = core::value_map(dist, 204, 223, 7.633f, 14.175f);
                    //vertical_angle = core::value_map(dist, 223, 204, 7.633f, 14.175f);
                    vertical_angle = core::value_map(dist, 204, 223, 14.175f, 7.633f);
                } else if (dist >= 188 && dist <= 204) {
                    //vertical_angle = core::value_map(dist, 188, 204, 14.175f, 20.621f);
                    vertical_angle = core::value_map(dist, 188, 204, 20.621f, 14.175f);
                } else if (dist >= 173 && dist <= 188) {
                    //vertical_angle = core::value_map(dist, 173, 188, 20.621f, 26.565f);
                    vertical_angle = core::value_map(dist, 173, 188, 26.565f, 20.621f);
                } else if (dist >= 161 && dist <= 173) {
                    //vertical_angle = core::value_map(dist, 161, 173, 20.621f, 31.995f);
                    vertical_angle = core::value_map(dist, 161, 173, 31.995f, 26.565f);
                } else if (dist >= 151 && dist <= 161) {
                    //vertical_angle = core::value_map(dist, 151, 161, 36.964f, 31.995f);
                    vertical_angle = core::value_map(dist, 151, 161, 38.964f, 31.995f);
                } else if (dist >= 137 && dist <= 151) {
                    //vertical_angle = core::value_map(dist, 137, 151, 41.228f, 36.964f);
                    vertical_angle = core::value_map(dist, 137, 151, 44.228f, 38.964f);
                } 
                


               float vert_radians = vertical_angle * 0.0174533f;

                offset_samp_t p = {
                    offset,
                    offset,
                    sinf(-angle), cosf(-angle),
                    (angle),
                    ((angle<0) ? PI*2 - std::abs(angle) : angle),
                    calibration->gain,// * vertical_gain_multiplier(vertical_angle),
                    calibration->offset,
                    dist,
                    sinf(-vert_radians),
                    cosf(-vert_radians),
                    vert_radians,
                    (u16) find_closest_index(&point_locks, angle),
                    (dist < 131) ? true : false
                };

                //float off = PI-angle;
                // off is < 45deg off of center
                /*
                if (angle > 1.57079665f && angle <= 2.35619465f) {
                    p.vert_sin *= (p.angle_sin);
                } else if (angle >= 2.35619465f) {
                    p.vert_sin *= (p.angle_cos);
                }
                */
                
                all_points.emplace_back(p);
            }
        }

        return all_points;
    }


 
    

    /**
     * create a vector of offset sample points at:
     *  position: GOPTS->center_x, center_x, 
     *  between the radius
     *  radius: inner, outer diameter (from GOPTS)
     * 
     * PURE
     */
    //std::vector<offset_samp_t> make_all_planar_points_int() {
    std::unordered_map<int, std::vector<offset_samp_t>> make_all_planar_points_int() {
        TRACE("");
        const int width = 1500;
        const int height = 165;

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * ONE_ONE_THIRD, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * ONE_ONE_THIRD, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        const int len = inner.size();
        //std::vector<offset_samp_t> all_points;
        //all_points.reserve(height * len);
        std::unordered_map<int, std::vector<offset_samp_t>> all_points;

        u16 *planar_idx = data::get_planar_idx();


        for (int i=0; i<len; i++) {
            xy_t p1 = inner.at(i);
            xy_t p2 = outer.at(i);
            line_t l1 = {p1, p2};

            std::vector<offset_samp_t> p = gfx::line_to_vector(l1, height, CAM_WIDTH, len, i);
            //all_points.insert(all_points.end(), p.begin(), p.end());
            for (auto x : p) {
                planar_idx[x.src_off] = i;
            }
        }

        data::set_frame_width(len);
        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }

    /**
     * create a gausian blur kernel
     * PURE
     */
    float *make_gausian_kernel(int size) {
        if (size < 0 || size > 32) throw std::invalid_argument("kernel radius must be 0-32");
        const int side = size * 2 + 1;
        auto *kernel = static_cast<float *>(calloc(side * side, sizeof(float)));
        if (!kernel) throw std::bad_alloc();
        const float sigma = std::max(0.5f, size / 2.0f);
        float sum = 0.0f;
        for (int y = -size; y <= size; ++y) {
            for (int x = -size; x <= size; ++x) {
                const auto weight = expf(-(x*x + y*y) / (2.0f * sigma * sigma));
                kernel[(y + size) * side + x + size] = weight;
                sum += weight;
            }
        }
        for (int i = 0; i < side * side; ++i) kernel[i] /= sum;
        return kernel; // Caller owns/free()s this allocation.
    }


    /**
     * @brief apply the convolution filter *kernel 
     * 
     * @param size 
     * @param kernel 
     * @param src 
     * @param dst 
     */
    void apply_kernel(int size, float *kernel, calibration::point *src) {
        if (size < 0 || size > 32 || !kernel || !src)
            throw std::invalid_argument("invalid calibration kernel");
        const int side = size * 2 + 1;
        for (int i = 0; i < side * side; ++i) {
            uint32_t bits;
            memcpy(&bits, &kernel[i], sizeof(bits));
            if ((bits & 0x7f800000u) == 0x7f800000u || kernel[i] < 0)
                throw std::invalid_argument("invalid kernel weight");
        }
        std::vector<calibration::point> result(src, src + CAM_WIDTH * CAM_HEIGHT);
        // Preserve borders and metadata. Normalize near/far independently over
        // valid samples so missing measurements do not contribute zero values.
        for (int y = size; y < CAM_HEIGHT - size; ++y) {
            for (int x = size; x < CAM_WIDTH - size; ++x) {
                double near_sum = 0, far_sum = 0, near_weight = 0, far_weight = 0;
                for (int dy = -size; dy <= size; ++dy) {
                    for (int dx = -size; dx <= size; ++dx) {
                        const auto &point = src[(y + dy) * CAM_WIDTH + x + dx];
                        const double weight = kernel[(dy + size) * side + dx + size];
                        if (point.near_point > 200) {
                            near_sum += weight * point.near_point;
                            near_weight += weight;
                        }
                        if (point.far_point > 200) {
                            far_sum += weight * point.far_point;
                            far_weight += weight;
                        }
                    }
                }
                auto &point = result[y * CAM_WIDTH + x];
                if (near_weight > 0) point.near_point = std::lround(near_sum / near_weight);
                if (far_weight > 0) point.far_point = std::lround(far_sum / far_weight);
            }
        }
        std::copy(result.begin(), result.end(), src);
    }
    
    void u16_multiply_4x4_neon(u16 *A, float32_t *B, float32_t *C) {
        // these are the columns A
        float32x4_t A0;
        float32x4_t A1;
        float32x4_t A2;
        float32x4_t A3;
        
        // these are the columns B
        float32x4_t B0;
        float32x4_t B1;
        float32x4_t B2;
        float32x4_t B3;
        
        // these are the columns C
        float32x4_t C0;
        float32x4_t C1;
        float32x4_t C2;
        float32x4_t C3;
        
        float *f1 = (float*)A;
        float *f2 = (float*)A+2;
        float *f3 = (float*)A+4;
        float *f4 = (float*)A+6;
        A0 = vld1q_f32(f1);
        A1 = vld1q_f32(f2);
        A2 = vld1q_f32(f3);
        A3 = vld1q_f32(f4);
        
        // vdupq is the portable NEON spelling supported by older SIMDe too.
        C0 = vdupq_n_f32(0.0f);
        C1 = vdupq_n_f32(0.0f);
        C2 = vdupq_n_f32(0.0f);
        C3 = vdupq_n_f32(0.0f);
        
        // Multiply accumulate in 4x1 blocks, i.e. each column in C
        B0 = vld1q_f32(B);
        C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
        C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
        C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
        C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
        vst1q_f32(C, C0);
        
        B1 = vld1q_f32(B+4);
        C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
        C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
        C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
        C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
        vst1q_f32(C+4, C1);
        
        B2 = vld1q_f32(B+8);
        C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
        C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
        C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
        C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
        vst1q_f32(C+8, C2);
        
        B3 = vld1q_f32(B+12);
        C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
        C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
        C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
        C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
        vst1q_f32(C+12, C3);
    }



    void matrix_multiply_4x4_neon(float32_t *A, float32_t *B, float32_t *C) {
        // these are the columns A
        float32x4_t A0;
        float32x4_t A1;
        float32x4_t A2;
        float32x4_t A3;
        
        // these are the columns B
        float32x4_t B0;
        float32x4_t B1;
        float32x4_t B2;
        float32x4_t B3;
        
        // these are the columns C
        float32x4_t C0;
        float32x4_t C1;
        float32x4_t C2;
        float32x4_t C3;
        
        A0 = vld1q_f32(A);
        A1 = vld1q_f32(A+4);
        A2 = vld1q_f32(A+8);
        A3 = vld1q_f32(A+12);
        
        // vdupq is the portable NEON spelling supported by older SIMDe too.
        C0 = vdupq_n_f32(0.0f);
        C1 = vdupq_n_f32(0.0f);
        C2 = vdupq_n_f32(0.0f);
        C3 = vdupq_n_f32(0.0f);
        
        // Multiply accumulate in 4x1 blocks, i.e. each column in C
        B0 = vld1q_f32(B);
        C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
        C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
        C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
        C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
        vst1q_f32(C, C0);
        
        B1 = vld1q_f32(B+4);
        C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
        C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
        C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
        C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
        vst1q_f32(C+4, C1);
        
        B2 = vld1q_f32(B+8);
        C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
        C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
        C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
        C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
        vst1q_f32(C+8, C2);
        
        B3 = vld1q_f32(B+12);
        C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
        C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
        C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
        C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
        vst1q_f32(C+12, C3);
    }



   
    
}
