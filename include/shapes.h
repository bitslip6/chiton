#ifndef __SHAPES_H
#define __SHAPES_H 1

#include "demos.h"
#include <vector>

namespace gfx
{
    std::vector<xyf_t> circle_points_float(int16_t center_x, int16_t center_y, uint16_t numpoints, uint16_t radius);
    std::vector<xy_t> circle_points(int16_t center_x, int16_t center_y, uint16_t numpoints, uint16_t radius);
    bool is_steep(line_t line);
    int aa_line(uint16_t *src, uint16_t *dst, line_t line, int dst_width);
    std::vector<xy_samp_t> line_to_vector(line_t line, int segments);
    std::vector<xyf_samp_t> line_to_vector_float(linef_t line, int segments);
}

#endif
