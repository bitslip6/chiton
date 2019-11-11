
#ifndef __MAIN_H__
#define __MAIN_H__


#include <sys/types.h>
#include <png.h>
#include "geometry.h"
#include "defines.h"

typedef bool (*BoolCallback)();

void fb_cleanup();
u_int16_t* fb_init(const char* file_name, const char *font_file);
ScreenInfo fb_get_buffer_info();
void fb_line(int x0, int y0, int x1, int y1, u_int16_t color, u_int16_t *   fbm);
void fb_line4(Vec2i p0, Vec2i p1, u_int16_t color, u_int16_t *fbm);
void fb_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color, u_int16_t *fbm);
void fb_fast_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color,  u_int16_t *buffer);
//void fb_quad_fill(Vec2i *pts, u_int16_t color, u_int16_t *fbm);
void fb_my_triangle(ScreenPoint v1, ScreenPoint v2, ScreenPoint v3, u_int16_t color,  u_int16_t *buffer);
//void fb_vixel(int x0, int y0, u_int8_t sz, u_int16_t color, u_int16_t *buffer);
void fb_vixel(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer);
void fb_vixel_add(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer);
void Panic(const char *msg);

void draw_image_half_slow(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
void draw_image_half_fast(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
void draw_image_half_fast_skip(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
FBPngImage read_png_file(const char *filename);

void fb_write_string(char *msg, int buffx, int buffy, u_int16_t bgcolor, u_int16_t *buffer);

void fb_aa_line(int x0 , int y0 , int x1 , int y1, u_int16_t color, u_int16_t *buffer);
void init_fonts();



// PI BORG DRIVER
void borg_init();
void DriveForward(int power);
void AllStop();
void Right90();
void Left90();
void Right();
void Left();

void Fwd(char c);
void Rev();

// ADI WRAPPER
aditof::Camera *get_tof_camera();
void set_camera_mode(aditof::Camera *camera, char *mode);
void get_depth_frame(aditof::Camera *camera, uint16_t **data1);
#endif
