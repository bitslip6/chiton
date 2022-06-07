#include <math.h>
#include <algorithm>
#include "demos.h"
#include "system.h"
#include "core.h"
#include "utils.hpp"

namespace core
{

                    //vertical_angle = core::value_map(dist, 240, 261, 0.0f, -7.633f);
                    //vertical_angle = core::value_map(dist, 240, 261,  -7.633f, 15.0f);
                    //vertical_angle = core::value_map(dist, 240, 261,  15.0f, 6.3f);


    // todo: figure out "1 step" value, and subtract it from x
    float value_map(float x, float in_min, float in_max, float out_min, float out_max) {
        //float x1 = std::clamp(x, in_min, in_max);
        float in_min_fix = MIN(in_min, in_max);
        //float out_min_fix = MIN(out_min, out_max);

        float out_diff = absdiff_f(out_max, out_min);
        float in_diff = absdiff_f(in_max, in_min);

        float in_factor = (x - in_min_fix) / in_diff;
        float out_scaled = out_diff * in_factor;
        float out = (out_min < out_max) ? out_scaled + out_min: out_min - out_scaled;
        /*
        if (x < 160) {
            printf("(%f) = VA: [%f] in_f:(%f - %f / %f) out_scaled:(%f - %f / %f)\n", x, out, in_min, in_max, in_factor, out_min, out_max, out_scaled);
        }
        */


        //float r = (x1 - in_min_fix) * (out_factor) / (in_factor) + out_min_fix;
        return out;
    }

    float smooth_step(float edge0, float edge1, float x) {
        float x1 = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * (3 - 2 * x);
    }

    /**
     * compute absolute difference of 2 numbers
     */
    int absdiff(int n1, int n2) {
        return std::max(n2, n1) - std::min(n2, n1);
    }
    
    /**
     * compute absolute difference of 2 numbers
     */
    float absdiff_f(float n1, float n2) {
        return std::max(n2, n1) - std::min(n2, n1);
    }


    /**
     * compute rate of change from n1 to n2 of steps
     */
    float step(unsigned int steps, int n1, int n2) {
        return (float)(n2 - n1) / (float)(steps -1);
    }

   
    /**
     * calculate distance between x1/y1 and x2/y2
     */
    int calc_distance(int x1, int y1, int x2, int y2) {
        float s1 = (float)absdiff(x2, x1);
        float s2 = (float)absdiff(y2, y1);
        float dist = sqrtf((s1 * s1) + (s2 * s2));
        return (int)dist;
    }

    /**
     * calculate distance between x1/y1 and x2/y2
     */
    float calc_distance_float(float x1, float y1, float x2, int y2) {
        float s1 = absdiff_f(x2, x1);
        float s2 = absdiff_f(y2, y1);
        float dist = sqrtf((s1 * s1) + (s2 * s2));
        //printf(" %f . %f . %f . %f / %f x %f = %f\n", x1, y1, x2, y2, s1, s2, dist);
        //printf(" %f . %f = %f\n", s1, s2, dist);
        return dist;
    }

    /**
     * mod with support for 0 where.  so ugly, how can this be?
     */
    int mod_with_zero(int x, int y) {
        if (x == 0) { return y; }
        int x2 = std::abs(x);
        int result = x2 % y;
        if (result == 0) { return x; }
        return result;
    }
    
    void fft(float *data, unsigned long nn) {
        unsigned long n, mmax, m, j, istep, i;
        float wtemp, wr, wpr, wpi, wi, theta;
        float tempr, tempi;
        
        // printf(" ----- X: [%f] [%f] [%f] [%f]\n", data[0], data[1], data[2], data[3]);
     
        // reverse-binary reindexing
        n = nn<<1;
        j=1;
        for (i=1; i<n; i+=2) {
            if (j>i) {
                std::swap(data[j-1], data[i-1]);
                std::swap(data[j], data[i]);
                //printf("swap: [%d.%d] %f - %f", i, j, data[j-1], data[j]);
            }
            m = nn;
            while (m>=2 && j>m) {
                j -= m;
                m >>= 1;
            }
            j += m;
        };
     
        // here begins the Danielson-Lanczos section
        mmax=2;
        while (n>mmax) {
            istep = mmax<<1;
            theta = -(2*PI/mmax);
            wtemp = sinf(0.5f*theta);
            wpr = -2.0f*wtemp*wtemp;
            wpi = sinf(theta);
            wr = 1.0f;
            wi = 0.0f;
            for (m=1; m < mmax; m += 2) {
                for (i=m; i <= n; i += istep) {
                    j=i+mmax;
                    tempr = wr*data[j-1] - wi*data[j];
                    tempi = wr * data[j] + wi*data[j-1];
                    //printf("wr %f [%f] = %d [%f x %f]\n", wr, data[j], j, tempr, tempi);
     
                    data[j-1] = data[i-1] - tempr;
                    data[j] = data[i] - tempi;
                    data[i-1] += tempr;
                    data[i] += tempi;
                }
                wtemp=wr;
                wr += wr*wpr - wi*wpi;
                wi += wi*wpr + wtemp*wpi;
            }
            mmax=istep;
        }
    }
}