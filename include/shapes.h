#ifndef __SHAPES_H
#define __SHAPES_H 1

#include "demos.h"
#include <vector>

namespace gfx
{
    std::vector<xy_t> circle_points(s16 center_x, s16 center_y, u16 numpoints, u16 radius);
    bool is_steep(line_t line);
    int aa_line(uint16_t *src, uint16_t *dst, line_t line, int dst_width);
    std::vector<xy_samp_t> line_to_vector(line_t line, int segments);
}

#endif
