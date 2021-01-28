#include "stdint.h"
#include <map>

extern "C" {
    void process_standard_frame(uint16_t *frame, int width, int height, std::map<float, unsigned short> &dist);
}
