#ifndef __LUT
#define __LUT 1

#define TRIG_UNIT                   511
#define ANGLE_UNIT                  256
/* The same idea as 2*PI, PI/2, PI/4, etc. */
#define ANGLE_UNIT_2                (ANGLE_UNIT*2)
#define ANGLE_UNIT_HALF             (ANGLE_UNIT/2)
#define ANGLE_UNIT_EIGHTH           (ANGLE_UNIT/8)
#define ANGLE_UNIT_QUART            (ANGLE_UNIT/4)
#define ANGLE_UNIT_THREE_QUARTERS   (ANGLE_UNIT*3/4)


long lut_sin (long a);
/* long lut_cos (long a);  As a macro */
#define lut_cos(angle) (lut_sin((angle)+ANGLE_UNIT_QUART))
long lut_angle (long dx, long dy);
long lut_dist (long x, long y);
#endif

/* You see, 360 degree, 2*PI radians are more or less arbitrary angle units.
 * Since I have created my own trig functions I can create my own
 * angle units. I created ones that powers of 2.
 */


#ifndef ACIDWARP_H
#define ACIDWARP_H 1
#include <sys/types.h>
#include "../demos.h"

// ugly
typedef struct RandPoints_t {
    float x1;
    float x2;
    float x3;
    float x4;
    float y1;
    float y2;
    float y3;
    float y4;
    int dx;
    int dy;
    int wx;
    int wy;
} RandPoints_t;

typedef float (AcidCallback)(RandPoints_t *, Point2D_t *, float, float, float);
void Animate_Acid(DisplayBuffers *disp, AnimationOptions *opts, Costume_t *c, AcidCallback *fp);
float d11(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d10(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d9(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d8(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d7(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d6(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d5(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d4(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d3(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d2(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);
float d1(UNUSED RandPoints_t *r, UNUSED Point2D_t *point, UNUSED float dist, float angle, float off);

//Color_t color = fp(r, point, dist, angle);

/*
typedef struct Point2D_t {
    u8 x;
    u8 y;
    u16 offset;
} Point2D_t;

typedef struct Color_t {
    u8 r;
    u8 g;
    u8 b;
} Color_t;

typedef struct Costume_t  {
    u8 width;
    u8 height;
    u8 end;
    u8 toff[64];
    u8 boff[64];
    u8 leds;
    u8 center_x;
    u8 center_y;
    u16 offset;
    Color_t image[1200];
} Costume_t;
*/

#endif
