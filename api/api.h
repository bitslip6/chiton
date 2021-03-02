#include "stdint.h"
#include <map>

extern "C" {

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


    void process_standard_frame(uint16_t *frame, int width, int height, std::vector<AngleDist> &dist);
}
