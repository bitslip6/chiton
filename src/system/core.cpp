#include <math.h>
#include <algorithm>
#include "demos.h"
#include "core.h"

namespace core {

    /**
     * compute absolute difference of 2 numbers
     */
    int absdiff(int n1, int n2) {
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
        return dist;
    }



}