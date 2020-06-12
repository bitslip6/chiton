#ifndef __CORE_H
#define __CORE_H 1

#include "util.h"
#include "color/hsv.h"

#define SIN_SIZE 1024
#define PI 3.1415926535f
#define RAD PI * 2
#define MIX(x, y, a) x*(1-a)+y*a
#define COLORMIX(d, x, y, a) d->r = MIX(x.r, y.r, a); d->g = MIX(x.g, y.g, a); d->b = MIX(x.b, y.b, a);
#define RADIANS(degrees) (PI * degrees)/180
#define DEGREES(radians) (180.0*radians)/PI
#define FRACT(x) x-floorf(x)

/* Stuff that's already there, but is faster as a MACRO */
#define MIN(a,b)  (((a) < (b)) ?  (a) : (b) )
#define MAX(a,b)  (((a) > (b)) ?  (a) : (b) )

#define ABS(a)    (((a) < 0  ) ? -(a) : (a) )
#define ABS_F(a)   (((a) < F 0) ? -(a) : (a) )
#define ABS_D(a)   (((a) < 0.0) ? -(a) : (a) )

#define SIGN(a)   (((a) < 0  ) ?   -1 :   1 )
#define SIGN_F(a)  (((a) < F 0) ? F(-1): F 1 )
#define SIGN_D(a)  (((a) < 0.0) ? -1.0 : 1.0 )

#define stepf(edge,x) (x<edge) ? 0.0f : 1.0f

typedef void (ShaderCallback)(u16 time, u8 x, u8 y, RGBA8 *out);

typedef struct Vec28 {
    uint8_t x;       // x
    uint8_t y;       // y
} Vec28;

typedef struct Vec216 {
    int16_t x;       // x
    int16_t y;       // y
} Vec216;


typedef struct Vec2f {
    float x;       // x
    float y;       // y

    //struct Vec2f& operator+=(const Vec2f& rhs) { x += rhs.x; y += rhs.y; return *this; }
    //struct Vec2f& operator+=(const float& k) { x += k; y += k; return *this; }
} Vec2f;


/**
 * clamp a float value between lo and hi
 */
float Clamp_f(float value, float lo, float hi);

s16 Clamp_s16(s16 value, s16 lo, s16 hi);

//s16 SmoothStep_s16(s16 edge0, s16 edge1, s16 x);
s16 SmoothStep_s16(s16 edge0, s16 edge1, s16 x, s16 maxv);

float SmoothStep_f(float edge0, float edge1, float x);
s16 sinu(s16 x);

#endif
