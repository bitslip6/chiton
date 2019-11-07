//////////////////////////////////////////////////////////////////////
// Projection library for Arduino
// Created March 2105 by Andrew Meyer
// 
// NOTES:
//  - Coordinate system is a right-handed, Z-up system
//  - 3D rotations follow ZYX Euler angle convention (yaw-pitch-roll)
//  - Angles are assumed to be in degrees
//////////////////////////////////////////////////////////////////////

#include "projection.h"
#include "main.h"
#include <sys/types.h>
#include <math.h>

double round(double d)
{
  return floor(d + 0.5);
}

Transform::Transform()
    : m00(1), m01(0), m02(0),
      m10(0), m11(1), m12(0),
      m20(0), m21(0), m22(1),
      x(0),   y(0),   z(0)
{ }

Transform::Transform(float mat00, float mat01, float mat02,
                     float mat10, float mat11, float mat12,
                     float mat20, float mat21, float mat22,
                     float posX,  float posY,  float posZ)
    : m00(mat00), m01(mat01), m02(mat02),
      m10(mat10), m11(mat11), m12(mat12),
      m20(mat20), m21(mat21), m22(mat22),
      x(posX),    y(posY),    z(posZ)
{ }

Transform::Transform(float angleX, float angleY, float angleZ,
		             float scaleX, float scaleY, float scaleZ,
		             float posX,   float posY,   float posZ)
    : x(posX), y(posY), z(posZ)
{
    float sx = sin(DEG_TO_RAD(angleX));
    float sy = sin(DEG_TO_RAD(angleY));
    float sz = sin(DEG_TO_RAD(angleZ));
    float cx = cos(DEG_TO_RAD(angleX));
    float cy = cos(DEG_TO_RAD(angleY));
    float cz = cos(DEG_TO_RAD(angleZ));

    m00 = scaleX * cy * cz;
    m01 = scaleY * (sx * sy * cz - cx * sz);
    m02 = scaleZ * (sx * sz + cx * sy * cz);

    m10 = scaleX * cy * sz;
    m11 = scaleY * (sx * sy * sz + cx * cz);
    m12 = scaleZ * (cx * sy * sz - sx * cz);

    m20 = scaleX * -sy;
    m21 = scaleY * sx * cy;
    m22 = scaleZ * cx * cy;
}

Transform Transform::inverse() const
{
    // Compute N = inv(M)
    float n00 = (m11 * m22) - (m12 * m21);
    float n01 = (m02 * m21) - (m01 * m22);
    float n02 = (m01 * m12) - (m02 * m11);
    float n10 = (m12 * m20) - (m10 * m22);
    float n11 = (m00 * m22) - (m02 * m20);
    float n12 = (m02 * m10) - (m00 * m12);
    float n20 = (m10 * m21) - (m11 * m20);
    float n21 = (m01 * m20) - (m00 * m21);
    float n22 = (m00 * m11) - (m01 * m10);
    
    float det = (m02 * n20) + (m01 * n10) + (m00 * n00);
    
    n00 /= det; n01 /= det; n02 /= det;
    n10 /= det; n11 /= det; n12 /= det;
    n20 /= det; n21 /= det; n22 /= det;
    
    // Compute R = -N * B
    float r0 = -((n00 * x) + (n01 * y) + (n02 * z));
    float r1 = -((n10 * x) + (n11 * y) + (n12 * z));
    float r2 = -((n20 * x) + (n21 * y) + (n22 * z));

    // Return inverse as [ N R ]
    //                   [ 0 1 ]
    return Transform(n00, n01, n02,
                     n10, n11, n12,
                     n20, n21, n22,
                     r0,  r1,  r2);
}

Transform Transform::operator *(const Transform &rhs) const
{
    return Transform(m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20,
                     m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21,
                     m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22,

                     m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20,
                     m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21,
                     m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22,

                     m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20,
                     m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21,
                     m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22,

                     m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + x,
                     m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + y,
                     m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + z);
}

point3 Transform::operator *(const point3 &rhs) const
{
    return { m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + x,
             m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + y,
             m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + z };
}

line3 Transform::operator *(const line3 &line) const
{
    return { (*this) * line.p0, (*this) * line.p1 };
}

PCamera::PCamera()
    : transform(Transform(-90, 0, 0, 1, 1, 1, 0, 0, 0))
{ }

PCamera::PCamera(int displayWidth, int displayHeight)
    : screenWidth(displayWidth), screenHeight(displayHeight), projMode(PROJ_PERSPECTIVE),
      focalDistPx(125), nearDist(MIN_NEAR_DIST), farDist(1000.0),
      transform(Transform(-90, 0, 0, 1, 1, 1, 0, 0, 0))
{ }

point2 PCamera::project(const point3 &point, bool clip /* = true */) const
{
    point2 ret;
    point3 camPoint = transform.inverse() * point; // Transform point to camera space
    
    if (clip)
    {
        // Clip to near/far 3D planes
        
        float near = fmax(nearDist, MIN_NEAR_DIST);
        float far = fmax(farDist, near);
        
        if (camPoint.z < near || camPoint.z > far)
        {
            return { NAN, NAN };
        }
    }
    
    // Project to screen
    ret = getImageCoords(camPoint);
    
    if (clip)
    {
        // Clip to 2D viewport

        if (ret.x < 0 || ret.x > (screenWidth - 1) ||
            ret.y < 0 || ret.y > (screenHeight - 1))
        {
            return { NAN, NAN };
        }
    }

    /*
    if (round)
    {
        ret = { round(ret.x), round(ret.y) };
    }
    */
    
    return ret;
}

line2 PCamera::project(const line3 &line, bool clip /* = true */) const
{
    line2 ret;
    Transform worldToCam = transform.inverse();
    line3 camLine = { worldToCam * line.p0, worldToCam * line.p1 };  // Transform line to camera space
    
    if (clip)
    {
        // Clip line to near/far 3D planes
        
        clipLine(camLine);
        
        if (isnan(camLine.p0.x))
        {
            return { { NAN, NAN }, { NAN, NAN } };
        }
    }
    
    // Project to screen
    ret = { getImageCoords(camLine.p0), getImageCoords(camLine.p1) };
    
    if (clip)
    {
        // Clip line to 2D viewport
        
        clipLine(ret);
        
        if (isnan(ret.p0.x))
        {
            return { { NAN, NAN }, { NAN, NAN } };
        }
    }
    
    /*
    if (round)
    {
        ret = { { round(ret.p0.x), round(ret.p0.y) },
                { round(ret.p1.x), round(ret.p1.y) } };
    }
    */
    
    return ret;
}

float PCamera::getLineT(float bound, float start, float end)
{
    return (bound - start) / (end - start);
}

point2 PCamera::getImageCoords(const point3 &camPoint) const
{
    if (projMode == PROJ_PERSPECTIVE)
    {
        return { (focalDistPx * camPoint.x) / camPoint.z + screenWidth / 2,
                 (focalDistPx * camPoint.y) / camPoint.z + screenHeight / 2 };
    }
    else
    {
        return { (screenWidth * camPoint.x) / orthoViewWidth + screenWidth / 2,
                 (screenWidth * camPoint.y) / orthoViewWidth + screenHeight / 2 };
    }
}

byte PCamera::outcode(const point3 &point, float near, float far) const
{
    byte code = 0;
    
    bitWrite(code, 0, point.z < near);
    bitWrite(code, 1, point.z > far);
    
    return code;
}

byte PCamera::outcode(const point2 &point) const
{
    byte code = 0;
    
    bitWrite(code, 0, point.x < 0);
    bitWrite(code, 1, point.x > (screenWidth - 1));
    bitWrite(code, 2, point.y < 0);
    bitWrite(code, 3, point.y > (screenHeight - 1));
    
    return code;
}

void PCamera::clipLine(line3 &line) const
{
    bool done = false;
    float near = fmax(nearDist, MIN_NEAR_DIST);
    float far = fmax(farDist, near);
    
    do
    {
        byte code0 = outcode(line.p0, near, far);
        byte code1 = outcode(line.p1, near, far);
        
        if ((code0 | code1) == 0)
        {
            // Trivially accept
            done = true;
        }
        else if ((code0 & code1) != 0)
        {
            // Trivially reject
            line = { { NAN, NAN, NAN }, { NAN, NAN, NAN } };
            done = true;
        }
        else
        {
            // Clip one end
            
            byte code;
            point3 *point;
            
            if (code0)
            {
                code = code0;
                point = &line.p0;
            }
            else
            {
                code = code1;
                point = &line.p1;
            }
            
            // Compute point of intersection with clipping plane
            
            float bound = bitRead(code, 0) ? near : far;
            float t = getLineT(bound, line.p0.z, line.p1.z);
            
            point->x = line.p0.x + t * (line.p1.x - line.p0.x);
            point->y = line.p0.y + t * (line.p1.y - line.p0.y);
            point->z = bound;
        }
    } while (!done);
}

void PCamera::clipLine(line2 &line) const
{
    bool done = false;
    
    do
    {
        byte code0 = outcode(line.p0);
        byte code1 = outcode(line.p1);
        
        if ((code0 | code1) == 0)
        {
            // Trivially accept
            done = true;
        }
        else if ((code0 & code1) != 0)
        {
            // Trivially reject
            line = { { NAN, NAN }, { NAN, NAN } };
            done = true;
        }
        else
        {
            // Clip one end
            
            byte code;
            point2 *point;
            
            if (code0)
            {
                code = code0;
                point = &line.p0;
            }
            else
            {
                code = code1;
                point = &line.p1;
            }
            
            // Compute point of intersection with clipping edge
            
            if (bitRead(code, 0))       // x < 0
            {
                float t = getLineT(0, line.p0.x, line.p1.x);
                
                point->x = 0;
                point->y = line.p0.y + t * (line.p1.y - line.p0.y);
            }
            else if (bitRead(code, 1))  // x > (screenWidth - 1)
            {
                float t = getLineT(screenWidth - 1, line.p0.x, line.p1.x);
                
                point->x = screenWidth - 1;
                point->y = line.p0.y + t * (line.p1.y - line.p0.y);
            }
            else if (bitRead(code, 2))  // y < 0
            {
                float t = getLineT(0, line.p0.y, line.p1.y);
                
                point->x = line.p0.x + t * (line.p1.x - line.p0.x);
                point->y = 0;
            }
            else                        // y > (screenHeight - 1)
            {
                float t = getLineT(screenHeight - 1, line.p0.y, line.p1.y);
                
                point->x = line.p0.x + t * (line.p1.x - line.p0.x);
                point->y = screenHeight - 1;
            }
        }
    } while (!done);
}



/**
 * draw a simple wireframe object to the *buffer
 */
void DrawWireObject(line3 *lines, Transform trans, PCamera cam, u_int32_t bcolor, u_int16_t *buffer) {
    int ctr = 0;
    line3 line;
    while ((line3 line = lines[ctr++]) && line.p1 != NULL) {
        if (line.p1 == NULL) { return; }

        line2 projLine = cam.project(trans * line);
        fb_aa_line(line.p0.x, line.p0.y, line.p1.x, line.p1.y, bcolor, buffer);
    }
}

/**
 * Draw a scene
 **/
void DrawScene(Scene scene, u_int16_t *buffer) {
    for (int i=0; i<scene.numObjects; i++) {
        if (scene.sold[i] == false) {
            DrawWireObject(scene.objLines[i], scene.objTrans[i], cam, scene.bcolor[i], buffer);
        }
    }
}

/**
 * update the demo scene for every frame
 **/
void DrawDemoScene(Scene *scene) {
    for (int i=0; i<scene.numObjects; i++) {
        scene.objTrans[i].m00 += 0.25;
        scene.objTrans[i].m01 -= 0.2;
        scene.objTrans[i].m02 += 0.15;
    }
}


/**
 * create the demo scene
 */
Scene CreateDemoScene() {
    Scene scene;
    scene.cam = new PCamera(FB_WIDTH, FB_HEIGHT)i;
    scene.light = NULL;
    scene.numObjects = 1;

    // Cube vertices
    point3 objVerts[] = {
        {  1,  1,  1 },
        {  1, -1,  1 },
        { -1, -1,  1 },
        { -1,  1,  1 },
        {  1,  1, -1 },
        {  1, -1, -1 },
        { -1, -1, -1 },
        { -1,  1, -1 }
    };

    // Cube lines
    line3 objLines[] = {
        { objVerts[0], objVerts[1] },
        { objVerts[1], objVerts[2] },
        { objVerts[2], objVerts[3] },
        { objVerts[3], objVerts[0] },
        { objVerts[4], objVerts[5] },
        { objVerts[5], objVerts[6] },
        { objVerts[6], objVerts[7] },
        { objVerts[7], objVerts[4] },
        { objVerts[0], objVerts[4] },
        { objVerts[1], objVerts[5] },
        { objVerts[2], objVerts[6] },
        { objVerts[3], objVerts[7] },
        { NULL, NULL },
    };

    // the projection camera and object transform matrix
    Transform objTrans = Transform(0, 0, 0, 1, 1, 1, 0, 0, 0);

    scene.objLines[] = [objLines];
    scene.objTrans[] = [objTrans];
    scene.solid[] = [false];
    scene.bcolor[] = [TURQ]

    return scene;
}


/**
 * render the demo scene forever.
 */
void RenderScene(Scene scene, u_int16_t *buffer, u_int16_t *fbm, pthread_mutex_t scene_lock) {

    struct timeval frame_start, frame_end;

    // render until lock released
    while(pthread_mutex_trylock(&scene_lock) > 0) { 
        // check lock 1 every 15 frames (2x second)
        for (int frame_ctr=0; frame_ctr < 15; frame_ctr++) {
            bzero(buffer, FB_WIDTH*FB_HEIGHT*2);
            gettimeofday(&frame_start, NULL);
            // rotate the objects in scene a bit
            scene.drawCallback(scene);
            /*
                        */
            
            // draw it to the buffer
            DrawScene(scene, buffer);
            fb_write_string("Init Camera", 1, 1, -1, buffer);
            memcpy(fbm, buffer, FB_WIDTH * FB_HEIGHT * 2);

            // sleep until 33ms have passed (30 fps)
            double frame_taken = 0.0;
            while (frame_taken < 0.0333) {
                gettimeofday(&frame_end, NULL);
                frame_taken = (frame_end.tv_sec - frame_end.tv_sec) * 1e6; 
                frame_taken += (frame_end.tv_usec - frame_start.tv_usec) * 1e-6; 
                usleep(300);
            }
        }
    }
}
