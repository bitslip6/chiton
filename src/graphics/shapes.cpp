#include <vector>

#include "demos.h"
#include "system.h"
#include "shapes.h"
#include "utils.hpp"

#include "fplus/fplus.hpp"

namespace gfx
{

    /**
     * calculate a single point on a circle at radius and radians
     *
     */
    xy_t calculate_circle_point(uint16_t radius, double radians) {
        xy_t p = {(s16)(radius * cos(radians)), (s16)(radius * sin(radians))};
        return p;
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
     * returns a new vector of points along a circle of radius
     */
    std::vector<xy_t> circle_points(s16 center_x, s16 center_y, u16 numpoints, u16 radius) {
       
        std::vector<xy_t> points;

        for (double angle = 0.0f; angle < PI2; angle += PI2 / numpoints) {
            if (angle > 3.8 && angle < 5.4) { continue; }
            points.emplace_back(calculate_circle_point(radius, angle));
        }

        //return points;
       
        return fplus::transform(fwd_move_point(center_x, center_y), points);
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
    xy_samp_t point_to_sample(float x1, float y1) {
        s16 x2, x3, y2, y3;
        float xf = (fPartOfNumber(x1));
        float yf = (fPartOfNumber(y1));

        // find 2 closest y pixels
        if (yf >= 0.5f) {
            y2 = CLAMP480(ceilf(y1));
            y3 = CLAMP480(y2 - 1);
        } else {
            y2 = CLAMP480(floorf(y1));
            y3 = CLAMP480(y2 + 1);
        }

        // find 2 closest x pixels
        if (xf >= 0.5f) {
            x2 = CLAMP640(ceilf(x1));
            x3 = CLAMP640(x2 - 1);
        } else {
            x2 = CLAMP640(floorf(x1));
            x3 = CLAMP640(x2 + 1);
        }
        
        xy_samp_t p = {(s16)x1, (s16)y1, (s16)x1, (s16) y1, xf, yf};
        return p;
    }


    /**
     * draw an antialiased line using wu algorithm
     * TODO: TEST fb_aa_line
     */
    std::vector<xy_samp_t> line_to_vector(line_t line, int segments) {
        //bool steep = is_steep(line);

        line.p1.x = CLAMP640(line.p1.x);
        line.p2.x = CLAMP640(line.p1.x);
        line.p1.y = CLAMP480(line.p1.x);
        line.p2.y = CLAMP480(line.p1.x);
       
        //compute the slope
        float dx = line.p2.x - line.p1.x;
        float dy = line.p2.y - line.p1.y;

        float gradient_x = (dx / segments);
        float gradient_y = (dy / segments);

        std::vector<xy_samp_t> points;
        points.reserve(segments);

        float x1 = line.p1.x;
        float y1 = line.p1.y;

        for (int i=1; i<segments; i++) {
            points.emplace_back(point_to_sample(x1, y1));
            x1 += gradient_x;
            y1 += gradient_y;
        }

        //printf("line len: [%d]\n", points.size());
        //p = {line.p2.x, line.p2.y, line.p2.x, line.p2.y, 1.0f, 1.0f};
        //points.emplace_back(p);
        return points;
    }



    /**
     * draw an antialiased line using wu algorithm
     * TODO: TEST fb_aa_line
     */
    int aa_line(uint16_t *src, uint16_t *dst, line_t line, int src_width, int dst_width) {
        bool steep = is_steep(line);

        // swap the co-ordinates if slope > 1
        if (steep) {
            std::swap(line.p1.x, line.p1.y);
            std::swap(line.p2.x, line.p2.y);
        }
        // swap x/y or we draw backwards
        if (line.p1.x > line.p2.x) {
            std::swap(line.p1.x, line.p2.x);
            std::swap(line.p1.y, line.p2.y);
        }
     
        //compute the slope
        float dx = line.p2.x - line.p1.x;
        float dy = line.p2.y - line.p1.y;
        float gradient = (dx == 0.0f) ? 1.0f : dy / dx;
     
        int xpxl1 = std::max((s16)1, line.p1.x);
        int xpxl2 = std::min((s16)src_width, line.p2.x);
        float intersect = (line.p1.y <= 0) ? 1 : line.p1.y;

        //printf("gradient: [%f] [%d] s:%d e:%d\n", gradient, steep, xpxl1, xpxl2);
       
        //printf("xpxl1 %d / xpxl2 %d\n", xpxl1, xpxl2);
        // main loop
        u32 doffset = 0;
        if (steep) {
            // vertical lines
            for (u16 y = xpxl1 ; y <=xpxl2; y++) {
                // pixel coverage is determined by fractional
                // part of y co-ordinate
                //uint16_t px = src[intersect_y * 640] * fPartOfNumber(intersect_y);

                u32 dst_x = iPartOfNumber(intersect);
                u32 soffset = (y * src_width) + dst_x;
               
                dst[doffset] = (src[soffset]) * fPartOfNumber(intersect);
                dst[doffset] += (src[soffset-src_width]) * rfPartOfNumber(intersect);

                //printf("%d = %d\n", src[soffset], dst[doffset]);
                // todo: 640 is number of line segments
                doffset += dst_width;

                // TODO: HSV THIS
                //u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
                //PIXEL_OR(buffer, iPartOfNumber(intersectY), x, color);
                /*
                disp->buffer1[iPartOfNumber(intersect_y) + x] = HsvToRgb(hue, sat, px);
                //printf("STEP PIX X%d.Y%d C%d\n", iPartOfNumber(intersectY)*FB_WIDTH, x, color);

                px = 255 * rfPartOfNumber(intersect_y);
                //px = 255 * rfPartOfNumber(intersectY);
                //color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
                disp->buffer1[(iPartOfNumber(intersect_y)-1) + x] = HsvToRgb(hue, sat, px);
                //PIXEL_OR(buffer, iPartOfNumber(intersectY)-1, x, color);
                //printf("STEEP PIX X%d.Y%d C%d\n", (iPartOfNumber(intersectY)-1)*FB_WIDTH, x, color);
                */

                intersect += gradient;
            }
        }
        else {
            // printf("W%d / Y%f - H%d\n", disp->width, intersect, disp->height);
            //for (u16 x = xpxl1 ; x <=xpxl2 && x < disp->width && intersect_y < disp->height; x++)
            for (u16 x = xpxl1; x <=xpxl2; x++) {

                u32 dst_y = iPartOfNumber(intersect);
                u32 soffset = (dst_y * src_width) + x;

                dst[doffset] = (src[soffset]) * fPartOfNumber(intersect);
                dst[doffset] += (src[soffset-src_width]) * rfPartOfNumber(intersect);


                //printf("[%x] . %f [%d] %f/%f = %d\n", &dst[doffset], intersect, src[soffset], fPartOfNumber(intersect), rfPartOfNumber(intersect), dst[doffset]);

                doffset += dst_width;
                /*
                int px = 255 * fPartOfNumber(intersect_y);
                //u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
                disp->buffer1[(iPartOfNumber(intersect_y)*FB_WIDTH)+x] = HsvToRgb(hue, sat, px);
                //printf("PIX X%d.Y%d C%d\n", iPartOfNumber(intersectY)*FB_WIDTH, x, color);

                px = 255 * rfPartOfNumber(intersect_y);
                //color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
                disp->buffer1[((iPartOfNumber(intersect_y)-1)*FB_WIDTH)+x] = HsvToRgb(hue, sat, px);
                //printf("PIX X%d.Y%d C%d\n", (iPartOfNumber(intersectY)-1)*FB_WIDTH, x, color);
                */

                intersect += gradient;
            }
            //printf("line len: [%d]\n", xpxl2 - xpxl1);
            //printf("W.%d, IY.%f, H.%d\n", disp->width, intersectY, disp->height);
        }

        return xpxl2 - xpxl1;
    }

}