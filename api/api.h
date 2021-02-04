#include "stdint.h"
#include <map>

extern "C" {

    typedef struct SliceTranche {
        unsigned int tranche[20];
        unsigned short dist;
    } SliceTranche;

    void process_standard_frame(uint16_t *frame, int width, int height, std::map<uint16_t, SliceTranche> &dist);
}
