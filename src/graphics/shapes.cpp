#include <vector>
#include <algorithm>
#include <map>
#include <functional>
#include <math.h>


#include "demos.h"
#include "core.h"
#include "system.h"
#include "shapes.h"
#include "utils.hpp"
#include "data.h"
#include "color/hsv.h"

#include "fplus/fplus.hpp"

#define ANGLE_POINTS 1500


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
    xy_t p = {(s16)round(radius * cos(radians)), (s16)round(radius * sin(radians))};
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
        
        if (std::abs(r0) == std::abs(r1)) {
            if (angle > 0) { r1 =  3.45f; } // NOTE: 3.45 NOT 3.14 (3.45 = 2.82 + (vixel distance: .63)
            else { r1 =  -3.45f; }
        }

        float v0dist = std::abs(angle - r0);
        float v1dist = std::abs(angle - r1);

        
        float value = ((v0->gain*(r1 - angle))+(v1->gain*(angle - r0))) / (r1 - r0);
        float offset = ((v0->offset*(r1 - angle))+(v1->offset*(angle - r0))) / (r1 - r0);
        
        
        VixelCalibration *v = new VixelCalibration();
        v->vixel = (v0dist < v1dist) ? v0->vixel : v1->vixel;
        v->radian = angle;
        v->gain = value;
        v->offset = (s16)offset;

        return v;
    }

    /**
     * find the vixel calibration for a given angle in radians
     * TODO: blending vixels 4 and 5 (-3.14 +3.14 needs work...)
     * TODO: make sure 8 and 1 are on the correct sides
     * TODO: expert system.  it's ugly but fast and will work 100%
     */
    VixelCalibration* find_calibration_for_radian(float angle) {
        
        // iterate over all calibration entries
        //for (auto& [vixel_num, calibration] : *GOPTS->cal) {
        //for (const auto &cal : *GOPTS->cal) {
        // we could hard code angles for next_close here for > 2.71 = vixel @ -3.14 and < 2.71 @ +3.14

        if (angle > -1.05f && angle < 0) {
            return GOPTS->cal->find(8)->second;
        }
        if (angle < 1.05f && angle > 0) {
            return GOPTS->cal->find(1)->second;
        }
        VixelCalibration closest, next_close;

        if (angle <= -2.82f ) { 
            //return GOPTS->cal->find(8)->second;
            closest = *GOPTS->cal->find(5)->second;
            next_close = *GOPTS->cal->find(4)->second;
        } else if (angle >= 2.82f ) {
            //return GOPTS->cal->find(8)->second;
            //closest = *GOPTS->cal->find(4)->second;
            //next_close = *GOPTS->cal->find(4)->second;
            closest = *GOPTS->cal->find(4)->second;
            next_close = *GOPTS->cal->find(5)->second;
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
        
        //PANIC_IF((std::abs(gains.at(0)->idx - gains.at(1)->idx) > 1), "not using 2 measurements next to each other!", -9);

        auto f = merge_vixel_calibration(&closest, &next_close, angle);
        // printf("(%f) 1: %d 2: %d, gain [%f]\n", angle, gains.at(0)->idx, gains.at(1)->idx, f->gain);
        return f;
                   
        //PANIC("unable to find vixel calibration for angle: [%f]", angle);
        //VixelCalibration *v = new VixelCalibration();
        //return v;
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
            gain_vert = core::value_map(vertical_angle, -5.7f, -16, 0.99055f, 0.96620f);
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
        } else if (vertical_angle < 30.97f) {
            gain_vert = core::value_map(vertical_angle, 27.52f, 30.97f, 0.95573f, 0.94680f);
        } else if (vertical_angle < 35.25f) {
            gain_vert = core::value_map(vertical_angle, 30.97f, 35.25f, 0.94680f, 0.917350f);
        } else if (vertical_angle < 39.16f) {
            gain_vert = core::value_map(vertical_angle, 35.25f, 39.16, 0.917350f, 0.868947f);
        } else if (vertical_angle < 42.36f) {
            gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.76680f);
        } else {
            gain_vert = core::value_map(vertical_angle, 42.36f, 44.16f, 0.76680f, 0.7250f);
        }

        return gain_vert;
    }

    offset_samp_t point_to_sample2(float x1, float y1, uint src_width, uint dst_offset, uint slice) {
        s16 x, y;
        //float xf = (fPartOfNumber(x1)) - 0.5f;
        //float yf = (fPartOfNumber(y1)) - 0.5f;
        float xf = fPartOfNumber(x1);
        float yf = fPartOfNumber(y1);

        // find previous y pixel depending on direction
        y = CLAMP480(iPartOfNumber(y1));
        //y = (yf < 0.5f) ? y-1 : y;

        // find previous x pixel depending on direction
        x = CLAMP640(iPartOfNumber(x1));
        //x = (xf < 0.5f) ? x-1 : x;

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
            (u16) slice,
            false
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
        uint doffset = dst_width - segment_number;
        
        // TODO: WHY IS point to sample not adding fractional points ??????
        for (int i=1; i<=segments-1; i++) {
            points.emplace_back(point_to_sample2(x1, y1, src_width, doffset, i));
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
            uint rgb_offset = p.dst_off * 3; 
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
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_greyscale2(const std::unordered_map<int, std::vector<offset_samp_t>> &point_map, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data, std::vector<AngleDist> &distances) {

        memset(jpeg_data, 0, 640*480);

        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        UNUSED const int height = data::get_frame_height();
        
        std::unordered_map<u16, u16> dist_map;
               
        u32 test_a[256];
        u8 test_n[256];
        auto move_ctr = fwd_move_point(GOPTS->center_x, GOPTS->center_y);

        for (int i = 0; i<=ANGLE_POINTS; i++) {
            //printf("scale 5: [%d]\n", i);
            auto s = point_map.find(i);
            if (s == point_map.end()) {
                exit(0);
            }

            memset(test_a, 0, sizeof(u32) * 26);
            memset(test_n, 0, sizeof(u8) * 26);
            
            offset_samp_t p;
            for (uint j=0; j<s->second.size(); j++) {
                //dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;
                p = s->second[j];
                u8 idx = src[p.src_off] / 160;
                test_a[idx] += src[p.src_off];
                test_n[idx]++;

                //uint rgb_offset = p.dst_off * 3; 
                //jpeg_data[p.src_off] = 255 - (src[p.dst_off] >> 4);
            }

            AngleDist d;
            d.angle = p.angle;
            d.saturated = i;
            
            for (int k=3; k<25; k++) {
                if (test_n[k] > 5) {
                    d.dist = ((test_a[k] / test_n[k]) * p.gain) + p.gain_offset;
                    break;
                }
            }

            xy_t plot = move_ctr(calculate_circle_point(d.dist / 18, p.angle));
            jpeg_data[(CLAMP480(plot.y) * 640) + CLAMP640(plot.x)] = 200;
            distances.emplace_back(d);
        }
       
        return true;
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

            int dx = closest * cos(angle);
            int dy = closest * sin(angle);

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


    

    std::vector<xy_samp_t> calibrate_map(std::vector<xy_samp_t> points) {
        std::vector<xy_samp_t> calibrated;
        calibrated.reserve(points.size());

        for (xy_samp_t point : points) {
            VixelCalibration *v = find_calibration_for_radian(point.angle);
            
            point.xf *= v->gain;
            point.yf *= v->gain;

            point.offset = v->offset;
            calibrated.emplace_back(point);
        }

        return calibrated;
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
                    gain = 0;
                    offset = 0;
                }
                if (angle > -0.77f && angle < 0.77f) {
                    gain = 0;
                    offset = 0;
                }

                float vertical_angle = core::value_map(dist, 236, 282, 1.0f, -15.0f);
                if (dist <= 236) {
                    vertical_angle = core::value_map(dist, 130, 236, 45.0f, 1.0f);
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
                    false
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
         
        // create vector for each sample
        for (int i=0; i<=ANGLE_POINTS; i++) {
            std::vector<offset_samp_t> v;
            samples[i] = v;
        }
        
        // add each sample to the map of slices
        for (auto sample : input) {
            samples[sample.slice].emplace_back(sample);
        }

        // loop over all map indexes and order them from inside out
        std::unordered_map<int, std::vector<offset_samp_t>>::iterator it = samples.begin();
        while(it != samples.end()) {
            std::vector<offset_samp_t> samples = it->second;
            std::sort( samples.begin( ), samples.end( ), []( const auto& lhs, const auto& rhs ) { 
                return lhs.center_dist < rhs.center_dist;
            } );
            it++;
        }
        
        return samples;
    }
    
    
    std::vector<offset_samp_t> make_all_polar_points_int2() {
        
        std::vector<offset_samp_t> all_points;
        xy_t center = {GOPTS->center_x, GOPTS->center_y};
        
        std::vector<float> point_locks;
        float interval = ((PI*2)/1500) * (2.37f/3.14f); // create 1500 points ratio for 270deg (2.37rad)
        for (float i=-PI; i<PI; i+=interval) {
            if (i > -0.77f && i < 0.77f) { continue; }
            point_locks.emplace_back(i);
        }
        

        for (int16_t x=0; x<FRAME_POLAR_WIDTH_DEFAULT; x++) {
            for (int16_t y=0; y<FRAME_POLAR_HEIGHT_DEFAULT; y++) {
                
                xy_t pixel = {x, y};
                uint32_t offset = (y * FRAME_POLAR_WIDTH_DEFAULT) + x;
                float angle = pixel_to_radians(center, pixel);
                u16 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 310);
                
                if (angle > -0.77f && angle < 0.77f) { continue; }
                if (dist < 130 || dist > 295) { continue; }
                
                VixelCalibration *calibration = find_calibration_for_radian(angle);

                float vertical_angle = core::value_map(dist, 236, 282, 1.0f, -15.0f);
                if (dist <= 236) {
                    vertical_angle = core::value_map(dist, 130, 236, 45.0f, 1.0f);
                }

                offset_samp_t p = {
                    offset,
                    offset,
                    0.0f, 0.0f,
                    angle,
                    calibration->gain * vertical_gain_multiplier(vertical_angle),
                    calibration->offset,
                    dist,
                    vertical_angle * 0.0174533f,
                    (u16) find_closest_index(&point_locks, angle),
                    (dist < 131) ? true : false
                };
                
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
    std::vector<offset_samp_t> make_all_planar_points_int() {
        TRACE("");
        const int width = data::get_frame_width();
        const int height = data::get_frame_height();

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->outer_dist);
        //const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.6, GOPTS->inner_dist);
        //const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.6, GOPTS->outer_dist);

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


    
}