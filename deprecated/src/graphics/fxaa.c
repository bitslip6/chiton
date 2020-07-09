#include "demos.h"
#include "draw.h"

float rgb2luma(RGB8 color) {
    return sqrt(color.r * 0.299 + color.g * 0.587 + color.b * 0.114);
}

u16 int2luma(u32 color) {
    return sqrt(((color & 0x00FF0000) >> 16) * 0.299 + ((color & 0x0000FF00) >> 8) * 0.587 + (color & 0x000000FF) * 0.114) * 4096;
}


#define EDGE_THRESHOLD_MIN 127
#define EDGE_THRESHOLD_MAX 0.125

/**
 * find the end of the edge by traveling along offset x and y until gradient scaled is reached
 */
Point2D_t *find_end(DisplayBuffers *disp, u16 lumalocalAverage, s32 gradientScaled, Point2D_t *p1, s8 offx, s8 offy) {

    p1->x += offx;
    p1->y += offy;
    printf("p1x %d p1y %d\n", p1->x, p1->y);

// CONTYINUE WWITH RFIND  END
    s32 luma = int2luma(GetPixel(disp, p1->x, p1->y)) - lumalocalAverage;
    printf(" FE FX2 -- X:%d, Y:%d find end- luma:%d, GS: %d\n", offx, offy, luma, gradientScaled);
    if (abs(luma) >= gradientScaled) {
        return p1;
    }

    return find_end(disp, lumalocalAverage, gradientScaled, p1, offx, offy);
}





void fxaa2(DisplayBuffers *disp, u8 x, u8 y) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdouble-promotion"
    u32 colorCenter = GetPixel(disp, x, y);
    u16 lumaCenter = int2luma(colorCenter);
    printf(" FX2 LC: %d\n", lumaCenter);

    // Luma at the four direct neighbours of the current fragment.
    u16 lumaDown = int2luma(GetPixel(disp, x, y+1));
    u16 lumaUp = int2luma(GetPixel(disp, x, y-1));
    u16 lumaLeft = int2luma(GetPixel(disp, x-1, y));
    u16 lumaRight = int2luma(GetPixel(disp, x+1, y));
    printf("   -- D%d,U%d,L%d,R%d", lumaDown,  lumaUp, lumaLeft, lumaRight);

    // Find the maximum and minimum luma around the current fragment.
    //#pragma GCC diagnostic ignored "-Wgnu"
    u16 lumaMin = min(lumaCenter,min(min(lumaDown,lumaUp),min(lumaLeft,lumaRight)));
    u16 lumaMax = max(lumaCenter,max(max(lumaDown,lumaUp),max(lumaLeft,lumaRight)));

    // Compute the delta.
    u16 lumaRange = lumaMax - lumaMin;
    printf(" FX2 Lmin: %d, Lmax: %d, LRange: %d\n", lumaMin, lumaMax, lumaRange);


    // If the luma variation is lower that a threshold (or if we are in a really dark area), we are not on an edge, don't perform any AA.
    if (lumaRange < max(EDGE_THRESHOLD_MIN,lumaMax*EDGE_THRESHOLD_MAX)) {
        printf("  FX2 luma range too small\n");
        return;
    }


    u16 lumaDownLeft = int2luma(GetPixel(disp, x-1, y+1));
    u16 lumaUpRight = int2luma(GetPixel(disp, x+1, y-1));
    u16 lumaUpLeft = int2luma(GetPixel(disp, x-1, y-1));
    u16 lumaDownRight = int2luma(GetPixel(disp, x+1, y+1));

    // todo: move these into the below calculations
    u32 lumaDownUp = lumaDown + lumaUp;
    u32 lumaLeftRight = lumaLeft + lumaRight;
    u32 lumaLeftCorners = lumaDownLeft + lumaUpLeft;
    u32 lumaDownCorners = lumaDownLeft + lumaDownRight;
    u32 lumaRightCorners = lumaDownRight + lumaUpRight;
    u32 lumaUpCorners = lumaUpRight + lumaUpLeft;

    // Compute an estimation of the gradient along the horizontal and vertical axis.
    //s32 edgeHorizontal = abs(-2 * lumaLeft + lumaLeftCorners) + abs(-2 * lumaCenter + lumaDownUp ) * 2 + abs(-2 * lumaRight + lumaRightCorners);
    //s32 edgeVertical   = abs(-2 * lumaUp + lumaUpCorners) + abs(-2 * lumaCenter + lumaLeftRight) * 2 + abs(-2 * lumaDown + lumaDownCorners);
    s32 edgeHorizontal = (-2 * lumaLeft + lumaLeftCorners) + (-2 * lumaCenter + lumaDownUp ) * 2 + (-2 * lumaRight + lumaRightCorners);
    s32 edgeVertical   = (-2 * lumaUp + lumaUpCorners) + (-2 * lumaCenter + lumaLeftRight) * 2 + (-2 * lumaDown + lumaDownCorners);

    // Is the local edge horizontal or vertical ?
    bool isHorizontal = (abs(edgeHorizontal) >= abs(edgeVertical));

    // continue  here. EH and EV may not be correct...
    printf(" FX2 EH: %d, EV: %d, isHoriz?: %d, \n", edgeHorizontal, edgeVertical, isHorizontal);


    // Select the two neighboring texels lumas in the opposite direction to the local edge.
    u16 luma1 = isHorizontal ? lumaDown : lumaLeft;
    u16 luma2 = isHorizontal ? lumaUp : lumaRight;
    //u16 luma1 = isHorizontal ? lumaLeft : lumaDown;
    //u16 luma2 = isHorizontal ? lumaRight : lumaUp;
    // Compute gradients in this direction.
    s32 gradient1 = luma1 - lumaCenter;
    s32 gradient2 = luma2 - lumaCenter;

    printf("L1:%d, L2:%d, LC:%d\n", luma1, luma2, lumaCenter);

    // Which direction is the steepest ?
    bool is1Steepest = abs(gradient1) >= abs(gradient2);

    // Gradient in the corresponding direction, normalized.
    s32 gradientScaled = 0.25*max(abs(gradient1),abs(gradient2));

    printf(" FX2 is1 steep: %d, gradientScaled: %d\n", is1Steepest, gradientScaled);

    u16 lumaLocalAverage = 0;
    s8 step = 1;
    Point2D_t end1 = {x, y, 0, 0};
    Point2D_t end2 = {x, y, 0, 0};

    if(is1Steepest){
        // Switch the direction
        step = -1;
        lumaLocalAverage = 0.5*(luma1 + lumaCenter);
    } else {
        lumaLocalAverage = 0.5*(luma2 + lumaCenter);
    }
    printf(" FX2 - luma1: %d, luma2: %d, AVG luma: %d\n", luma1, luma2, lumaLocalAverage);

    if (isHorizontal) {
        find_end(disp, lumaLocalAverage, gradientScaled, &end1, -1, 0);
        find_end(disp, lumaLocalAverage, gradientScaled, &end2, 1, 0);
    } else {
        find_end(disp, lumaLocalAverage, gradientScaled, &end1, 0, -1);
        find_end(disp, lumaLocalAverage, gradientScaled, &end2, 0, 1);
    }

    printf(" FX2 ENDS:  X%d.Y%d,  X%d.Y%d\n", end1.x, end1.y, end2.x, end2.y);


    // Compute the distances to each extremity of the edge.
    s8 distance1 = isHorizontal ? (x - end1.x) : (y - end1.y);
    s8 distance2 = isHorizontal ? (end2.x - x) : (end2.y - y);

    // In which direction is the extremity of the edge closer ?
    //bool isDirection1 = distance1 < distance2;
    s8 distanceFinal = min(distance1, distance2);


    // Length of the edge.
    s8 edgeThickness = (distance1 + distance2);

    // UV offset: read in the direction of the closest side of the edge.
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;

    printf(" FX2  d1: %d, d2: %d, PO: %f\n", distance1, distance2, pixelOffset);
    RGB8 finalColor, initColor, p1;
    U32RGB8(colorCenter, &initColor);
    (isHorizontal) ? U32RGB8(GetPixel(disp, x, y+step), &p1) : U32RGB8(GetPixel(disp, x+step, y), &p1);
    finalColor.r = (initColor.r * (1-pixelOffset)) + (p1.r * pixelOffset); 
    finalColor.g = (initColor.r * (1-pixelOffset)) + (p1.g * pixelOffset); 
    finalColor.b = (initColor.r * (1-pixelOffset)) + (p1.b * pixelOffset);
    printf(" Fx3 [%d, %d, %d]\n", finalColor.r, finalColor.g, finalColor.b);
    //PPixelSet(disp, x, y, RGB8U32(finalColor));

    #pragma GCC diagnostic pop
}




/*
void fxaa(DisplayBuffers *disp, float x, float y) {

//disp->buffer1[doff+x] = RGB8U32(c->image[coff+x]);
RGB8 colorCenter;
U32RGB8(disp->buffer1[(y*disp->width)+x], &colorCenter);

// Luma at the current fragment
float lumaCenter = rgb2luma(colorCenter);

// Luma at the four direct neighbours of the current fragment.
float lumaDown = GetPixel(disp, x-1, y);
float lumaUp = GetPixel(disp, x+1, y);
float lumaLeft = GetPixel(disp, x, y-1);
float lumaRight = GetPixel(disp, x, y+1);

// Find the maximum and minimum luma around the current fragment.
float lumaMin = min(lumaCenter,min(min(lumaDown,lumaUp),min(lumaLeft,lumaRight)));
float lumaMax = max(lumaCenter,max(max(lumaDown,lumaUp),max(lumaLeft,lumaRight)));

// Compute the delta.
float lumaRange = lumaMax - lumaMin;

// If the luma variation is lower that a threshold (or if we are in a really dark area), we are not on an edge, don't perform any AA.
if(lumaRange < max(EDGE_THRESHOLD_MIN,lumaMax*EDGE_THRESHOLD_MAX)) {
    //fragColor = colorCenter;
    //return;
}


float lumaDownLeft = GetPixel(disp, x-1, y-1);
float lumaUpRight = GetPixel(disp, x+1, y+1);
float lumaUpLeft = GetPixel(disp, x-1, y+1);
float lumaDownRight = GetPixel(disp, x+1, y-1);


float lumaDownUp = lumaDown + lumaUp;
float lumaLeftRight = lumaLeft + lumaRight;

float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
float lumaDownCorners = lumaDownLeft + lumaDownRight;
float lumaRightCorners = lumaDownRight + lumaUpRight;
float lumaUpCorners = lumaUpRight + lumaUpLeft;


// Compute an estimation of the gradient along the horizontal and vertical axis.
float edgeHorizontal =  abs(-2.0 * lumaLeft + lumaLeftCorners)  + abs(-2.0 * lumaCenter + lumaDownUp ) * 2.0    + abs(-2.0 * lumaRight + lumaRightCorners);
float edgeVertical =    abs(-2.0 * lumaUp + lumaUpCorners)      + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0  + abs(-2.0 * lumaDown + lumaDownCorners);

// Is the local edge horizontal or vertical ?
u8 isHorizontal = (edgeHorizontal >= edgeVertical);



// Select the two neighboring texels lumas in the opposite direction to the local edge.
float luma1 = isHorizontal ? lumaDown : lumaLeft;
float luma2 = isHorizontal ? lumaUp : lumaRight;
// Compute gradients in this direction.
float gradient1 = luma1 - lumaCenter;
float gradient2 = luma2 - lumaCenter;

// Which direction is the steepest ?
u8 is1Steepest = abs(gradient1) >= abs(gradient2);

// Gradient in the corresponding direction, normalized.
float gradientScaled = 0.25*max(abs(gradient1),abs(gradient2));

// Choose the step size (one pixel) according to the edge direction.
float stepLength = isHorizontal ? disp->invy : disp->invx;

// Average luma in the correct direction.
float lumaLocalAverage = 0.0;

if(is1Steepest) {
    // Switch the direction
    stepLength = -stepLength;
    lumaLocalAverage = 0.5*(luma1 + lumaCenter);
} else {
    lumaLocalAverage = 0.5*(luma2 + lumaCenter);
}

// Shift UV in the correct direction by half a pixel.
//vec2 currentUv = In.uv;
u8 nx = x;
u8 ny = y;
if(isHorizontal){
    ny += stepLength * 0.5;
} else {
    nx += stepLength * 0.5;
}


// first exploration
// Compute offset (for each iteration step) in the right direction.
vec2 offset = isHorizontal ? vec2(inverseScreenSize.x,0.0) : vec2(0.0,inverseScreenSize.y);
// Compute UVs to explore on each side of the edge, orthogonally. The QUALITY allows us to step faster.
vec2 uv1 = currentUv - offset;
vec2 uv2 = currentUv + offset;

// Read the lumas at both current extremities of the exploration segment, and compute the delta wrt to the local average luma.
float lumaEnd1 = rgb2luma(texture(screenTexture,uv1).rgb);
float lumaEnd2 = rgb2luma(texture(screenTexture,uv2).rgb);
lumaEnd1 -= lumaLocalAverage;
lumaEnd2 -= lumaLocalAverage;

// If the luma deltas at the current extremities are larger than the local gradient, we have reached the side of the edge.
bool reached1 = abs(lumaEnd1) >= gradientScaled;
bool reached2 = abs(lumaEnd2) >= gradientScaled;
bool reachedBoth = reached1 && reached2;

// If the side is not reached, we continue to explore in this direction.
if(!reached1){
    uv1 -= offset;
}
if(!reached2){
    uv2 += offset;
}   

}
*/
