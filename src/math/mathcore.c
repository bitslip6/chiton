#include <assert.h>
#include "util.h"
#include "demos.h"
#include "color/hsv.h"
#include "mathcore.h"

#define FIX_WIDTH 30
#define FIX_HEIGHT 30



/**
 * clamp a float value between lo and hi
 */
float Clamp_f(float value, float lo, float hi) {
    assert( !(hi < lo) );
    return (value < lo) ? lo : (hi < value) ? hi : value;
}

/**
 * clamp a float value between lo and hi
 */
s16 Clamp_s16(s16 value, s16 lo, s16 hi) {
    assert( !(hi < lo) );
    return (value < lo) ? lo : (hi < value) ? hi : value;
}

/**
 * Perform Hermite interpolation between two values
 * edge0 specifies the value of the lower edge of the Hermite function.
 * edge1 specifies the value of the upper edge of the Hermite function->
 * x specifies the source value for interpolation.
 */
s16 SmoothStep_s16(s16 edge0, s16 edge1, s16 x, UNUSED s16 maxv) {
    assert(!(edge1 < edge0));
    s16 E1 = x - edge0;
    s16 E2 = edge1 - edge0;
    float E3 = (float)((float)E1 / (float)E2);
    //printf(" [%d/%d] (%f)\n", E1, E2, E3);
    //s16 t = Clamp_s16((x - edge0) / (edge1 - edge0), 0, maxv);
    float t = Clamp_f(E3, 0, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * Perform Hermite interpolation between two values
 * edge0 specifies the value of the lower edge of the Hermite function.
 * edge1 specifies the value of the upper edge of the Hermite function->
 * x specifies the source value for interpolation.
 */
float SmoothStep_f(float edge0, float edge1, float x) {
    assert(!(edge1 < edge0));
    float t = Clamp_f((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}



/**
 * takes an integer between 0-SIN_SIZE(1024) and returns a
 * sinus wave from -1024 to 1024
 */
s16 sinu(s16 x) {
    //x %= ANGLE_UNIT;
    float theta = (float)x / SIN_SIZE;
    //float theta2 = fmodf(theta, RAD);
    float result = sinf(theta);
    s16 result_int = result * SIN_SIZE;
    //printf("sinus %d (%f) = (%f) [%d]\n", x, theta, result, result_int);
    return result_int;
}


