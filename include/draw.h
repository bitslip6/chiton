#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "mathcore.h"
#include "demos.h"

#ifndef DRAW_H
#define DRAW_H 1


// the pixel dimensions
#define WIDTH 30
#define HEIGHT 30 

#define INT_GRAD 256
#define INT_SHIFT 8

#define DISP2FRAC(x) x*INT_GRAD
#define FRAC2DISP(x) x>>INT_SHIFT

#define MAX_X  DISP2FRAC(WIDTH)
#define MAX_Y  DISP2FRAC(HEIGHT)
 
#define DEBUG_POINT2D(p) printf(" - DBG 2P: [%dx%d].%d/%d\n", p.x, p.y, p.xoffset, p.yoffset);


// #define WIDTH 37
// #define HEIGHT 22 

// framebuffer and sdl rendering info
#define FBWIDTH 1920
#define FB_PIXEL_SZ 42 

#define AA_FORCE_AUTO 0
#define AA_FORCE_VERT 1
#define AA_FORCE_HORIZ 2

// ws2811 lib ccode... (TODO rewrite this and optomize)
// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ

// #define STRIP_TYPE            WS2811_STRIP_RGB        // WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE            WS2811_STRIP_GRB        // WS2812/SK6812RGB integrated chip+leds


extern u8 *_DIST_LUT;
extern u16 _DIST_LUT_SZ;
extern u8 *_SIN_LUT;
extern u16 _SIN_LUT_SZ;

//int height_map[WIDTH] = {19, 20, 20, 20, 20, 20, 19, 19, 20, 20, 20, 20, 20, 20,  19, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17};

typedef u32 (DrawMethod)(u32, u32);

typedef struct IntParticle_t {
    s16 x1;
    s16 y1;
    s16 origin_x;
    s16 origin_y;
    u16 rad;

    u8 hue;
    u8 sat;
    u8 sat_o;
    u8 hue_o;
    u8 visible;
    u16 z_idx;

    float speed1;
    float speed2;
    float speed3;
    float power;
    s16 offx1;
    s16 offy1;
    s16 offx2;
    s16 offy2;
    u16 ttl;
    float ctr2;
    float ctr3;
    float ctr4;
} IntParticle_t;





// Pixel functions
void PPixelSet(DisplayBuffers *disp, int x, int y, u_int32_t color);
void HPixelSet(DisplayBuffers *disp, u_int8_t x, u_int8_t y, u_int8_t h, u_int8_t v, u_int8_t s);
void HPixelOr(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s);
void HPixelAnd(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s);

/**
 * calculate a pixel X/Y offset row major using disp->width
 */
static inline u16 PixelOFF(DisplayBuffers *disp, u8 x, u8 y) { return MIN(disp->sz+1, ((y*disp->width)+x)); }
#define DISP_OFF(disp, x, y) PixelOFF(disp, x, y)
/**
 * place a pixel at disp_ptr [offset] in u32 color space. asserts all memory access
 */
static inline void PixelAT(DisplayBuffers *disp, uint16_t offset, uint32_t color) { assert(disp->draw_ptr != NULL); assert(offset <= disp->sz); disp->draw_ptr[offset]=color; }
/**
 * place a pixel at disp_ptr(x,y) in u32 color space. asserts all memory access PixelAT
 */
static inline void PixelXY(DisplayBuffers *disp, uint8_t x, uint8_t y, uint32_t color) { PixelAT(disp, PixelOFF(disp, x, y), color); }


static inline u32 GetPixel(DisplayBuffers *disp, int x, int y) {
    return(disp->buffer1[(y*disp->width)+x]);
}
static inline u8 SatMulF(u8 x, float y) {
	u16 res = (u16) x * (float) y;
	u8 hi = res >> 8;
	u8 lo = res;
	
	return lo | -!!hi;
}
static inline u8  SatAdd(u8 a1, u8 a2) { u8 m1 = (a1 + a2); m1 |= -(m1 < a1); return m1; }
static inline u8  SatSub(u8 s1, u8 s2) { u8 m1 = (s1 - s2); m1 &= -(m1 <= s1); return m1; }
static inline u32 u32rgba(u8 a, u8 r, u8 g, u8 b) {
    return ((r*(a/255)) << 16) + ((g*(a/255)) << 8) + (b*(a/255));
}
static inline u32 u32rgb(u8 r, u8 g, u8 b) {
    return (r << 16) + (g << 8) + (b);
}

static inline void scaleint(Point2D_t *p0, Point2D_t *p1) {
    u8 t = p0->x >> 8;
    p1->x = t;
    p1->xoffset = p0->x - t;
    //#define INTTRANS(q) ah = q << 8; af = q - ah;
}

// returns absolute value of number 
static inline float absolute(float x ) { 
    if (x < 0) return -x; 
    else return x; 
} 
  
//returns integer part of a floating point number 
static inline int iPartOfNumber(float x) { 
    return (int)x; 
} 
  
//rounds off a number 
static inline int roundNumber(float x) { 
    return iPartOfNumber(x + 0.5f) ; 
} 
  
//returns fractional part of a number 
static inline float fPartOfNumber(float x) { 
    if (x>0) return x - iPartOfNumber(x); 
    else return x - (iPartOfNumber(x)+1); 
  
} 
  
//returns 1 - fractional part of number 
static inline float rfPartOfNumber(float x) { 
    return 1 - fPartOfNumber(x); 
} 
 


/*
u32b sat_subu32b(u32b x, u32b y)
{
	u32b res = x - y;
	res &= -(res <= x);

	return res;
}
*/

static inline u32 SatAdd32(u32 a1, u32 a2) {
    RGB8 c1, c2;
    U32RGB8(a1, &c1);
    U32RGB8(a2, &c2);
    RGB8 c3 = {
        .r = SatAdd(c1.r, c2.r),
        .g = SatAdd(c1.g, c2.g),
        .b = SatAdd(c1.b, c2.b)
    };
    return RGB8U32(&c3);
}

/**
 * opoacity should be betweek 0.0 - 1.0, 1.0 is a1 opaque, 
 */
static inline u32 SatAddOpacity32(u32 a1, u32 a2, float opacity1) {
    RGB8 c1, c2;
    U32RGB8(a1, &c1);
    U32RGB8(a2, &c2);
    RGB8 c3 = {
        .r = SatAdd(c1.r * opacity1, c2.r * (1.0f - opacity1)),
        .g = SatAdd(c1.g * opacity1, c2.g * (1.0f - opacity1)),
        .b = SatAdd(c1.b * opacity1, c2.b * (1.0f - opacity1))
    };
    return RGB8U32(&c3);
}



static inline u32 SatSub32(u32 a1, u32 a2) {
    RGB8 c1, c2;
    U32RGB8(a1, &c1);
    U32RGB8(a2, &c2);
    RGB8 c3 = {
        .r = SatSub(c1.r, c2.r),
        .g = SatSub(c1.g, c2.g),
        .b = SatSub(c1.b, c2.b)
    };
    return RGB8U32(&c3);
}


void ClearZBuff(DisplayBuffers *disp);
void ClearDisp(DisplayBuffers *disp);
void SetDisp(DisplayBuffers *disp, u32 c);
void RenderSrc(DisplayBuffers *disp, u_int32_t *src, u8 style);
void RenderDisp(DisplayBuffers *disp);
void RenderDispOffset(DisplayBuffers *disp);
void Decay(DisplayBuffers *disp, u_int32_t amount);
u_int32_t* ReadPNG(char* file_name, u_int32_t trans, int *width, int *height);
void SinusVertical(DisplayBuffers *disp, float counter, float magnitude, float rads);
void Detect_Edges(u16 *src, u16 *dst, u16 width, u16 height);
float Get_Dist_Power(float x1, float y1, float x2,float y2, float rad);
u8 Get_Dist_Power_Int(s16 x1, s16 y1, s16 x2, s16 y2, u16 rad);
void Decay2(DisplayBuffers *disp, float amount);
//u8 Compute_Dist(float distance_square, float power, u16 density, u16 radius);
//static inline u8 Compute_Dist(float distance_square, float power, u16 density, u16 radius);
void Make_Dist_Lut(float radius);
void Compute_Dist_Lut(float power, u16 density,  u16 radius);
void IntDrawParticles(DisplayBuffers *disp, IntParticle_t *particles, u16 num_particles);
void IntMoveParticle(IntParticle_t *particle, float friction, s16 gravity, s16 windx, s16 windy);
void DrawPerfectCircle(DisplayBuffers *disp, s16 x0, s16 y0, s16 radiusx, s16 radiusy, u8 hue, u8 sat, u8 power);
void Draw_AA_Line(DisplayBuffers *disp, int x0 , int y0 , int x1 , int y1, u8 h, u8 s);
void Draw_Point(DisplayBuffers *disp, u16 x2, u16 y2, s16 ox, s16 oy, u8 s, u8 h);
//void AAPixel(DisplayBuffers *disp, Point2D_t *p, HSV8 *color, u8 blend_mode);
void AAPixel(DisplayBuffers *disp, Point2D_t *p, HSV8 *color, u8 blend_mode, u8 dir_mode);
void Shade(DisplayBuffers *disp, u16 time, ShaderCallback *callback);

/**
 * scale a 2d point from GRADIENT space to actual space - INLINE
 */
static inline void Scale2D_Int(Point2D_t *p) {
    p->x = p->x >> INT_SHIFT;
    p->y = p->y >> INT_SHIFT;
}

/**
 * scale a 2d point from GRADIENT space to actual space - COPY RESULT to p2
 */
static inline void Scale2D_IntCpy(Point2D_t *src, Point2D_t *dst) {
    dst->x = src->x >> INT_SHIFT;
    dst->y = src->y >> INT_SHIFT;
}




void OverlayDraw(DisplayBuffers *disp, u8 x, u8 y, RGB8 *color);


/*
void Project_3d(Point3D_t *p3, Point2D_t *p2);
    float z = (float)ZPOINT/(ZPOINT + p3->z);
    p2->x = (int)p3->x * z;
    p2->y = p3->y * z;
    printf("Z:%f point: %d, p3X:%d, p2X:%d\n", z, p3->z, p3->x, p2->x);
}
*/

static inline u8 Compute_Dist(float distance_square, float power, u16 density, u16 radius) {
    float dsq_r = sqrtf(distance_square);

    //printf("%f / %d.%d\n", dsq_r, rad, rad2);
    if (dsq_r <= radius) {
        s32 inv_dist = radius-dsq_r;
        float ir = (float)(inv_dist / (float)radius);
        float id2 = (float)floorf(powf(ir*ir, power) * density);
        id2 = fminf(id2, 255.0f);
        return (u8)id2;
    }

    return 0;
}




static inline u8 Lookup_Dist_Power(s16 x1, s16 y1, s16 x2, s16 y2) {
    s16 dx = x2 - x1;
    s16 dy = y2 - y1;
    u16 dsq = (dx*dx + dy*dy)/2;
    return (dsq <= _DIST_LUT_SZ) ? _DIST_LUT[dsq] : 0;
}

static inline u8 Lookup_Sin(s16 x) {
    return _SIN_LUT[x % _SIN_LUT_SZ];
}



/*
static inline float fastsqrt(float n) {
    n = 1.0f / n;
    long i;
    float x, y;
 
    x = n * 0.5f;
    y = n;
    i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (1.5f - (x * y * y));
 
    return y;
}
*/


#ifdef FBDRIVER
u_int32_t* fb_init(const char *file_name);
#endif

#endif
