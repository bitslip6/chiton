#ifndef __SHAPES_H
#define __SHAPES_H 1

#include "demos.h"
#include <vector>
#include <map>

namespace gfx
{
    std::vector<xy_t> circle_points(s16 center_x, s16 center_y, u16 numpoints, u16 radius);
    std::vector<xyf_t> circle_points_float(int16_t center_x, int16_t center_y, uint16_t numpoints, uint16_t radius);
    //std::vector<xy_t> circle_points(int16_t center_x, int16_t center_y, uint16_t numpoints, uint16_t radius);
    float pixel_to_radians(xy_t center, xy_t pixel);
    bool is_steep(line_t line);
    int aa_line(uint16_t *src, uint16_t *dst, line_t line, int dst_width);
    std::vector<offset_samp_t> line_to_vector(line_t line, int segments, uint src_width, uint dst_width, uint segment_number);
    std::vector<xyf_samp_t> line_to_vector_float(linef_t line, int segments);
    
    std::vector<offset_samp_t> make_all_raw_points_int();
    std::vector<offset_samp_t> make_all_planar_points_int();
    std::vector<offset_quad_samp_t> make_all_quad_planar_points_int();
    //std::vector<xy_samp_t> make_all_points_int(resolution_t resolution);
    std::vector<xy_samp_t> connect_vector_points(std::vector<xy_t> points1, std::vector<xy_t> points2, int line_len);
    
    std::vector<xy_samp_t> calibrate_map(std::vector<xy_samp_t> points);
    //void map_points(std::vector<xy_samp_t> points, resolution_t res, uint16_t *src, uint16_t *dst, PixelFunc2 fn);
    //void map_points(std::vector<offset_samp_t> points, resolution_t res, uint16_t *src, uint16_t *dst, PixelFunc2 jpeg_fn);
    //void map_points_greyscale(std::vector<offset_samp_t> points, uint16_t *src, uint16_t *dst)
    //bool map_points_greyscale(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data, std::map<float, uint16_t> &dist_map);
    u16 map_points_greyscale(const std::vector<offset_quad_samp_t> &points, uint16_t *src, uint16_t *dst, UNUSED unsigned char *jpeg_data, UNUSED std::map<float, uint16_t> &dist_map);
    //std::map<float, uint16_t>* map_points_greyscale(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data);
    bool map_points_hsv(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data);
    bool map_points_gain(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data);
    //bool map_points_slam(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data);
    bool map_points_slam(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data);
}

#endif
