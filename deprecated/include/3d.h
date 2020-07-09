#ifndef __3D_H
#define __3D_H 1


#include "demos.h"
#include "util.h"

typedef struct Vec3_s16_t {
    s16 d[3];
} Vec3_s16_t;

typedef struct Vec3_f_t {
    float d[3];
} Vec3_f_t;

#define DEBUG_VEC3(x) printf("DBG - vec3 [%dx%dx%d]\n", x.d[0], x.d[1], x.d[2]);
#define DEBUG_VEC3PTR(x) printf("DBG - vec3 [%dx%dx%d]\n", x->d[0], x->d[1], x->d[2]);

#define VEC_X 0
#define VEC_Y 1
#define VEC_Z 2

typedef struct Point3D_t {
    s16 x;
    s16 y;
    s16 z;
} Point3D_t;

typedef struct Mat31f {
    float x[3];
} Mat31f;

typedef struct Matrix33f {
    float x[3][3];
} Matrix33f;


typedef struct Translate3D_t {
    // rotation vector
    Vec3_f_t rotation; 

    // stored rotation cos and sin vectors
    Vec3_f_t r_cos; 
    Vec3_f_t r_sin; 

    // translation vector
    Vec3_s16_t translation;

    float scale;

    // the full 3d rotation matrix
    Matrix33f rotation_matrix;
} Translate3D_t;



#define ZPOINT 50*256

//void Z_Order(Vec3_s16_t *box2, int16_t *z_order, uint16_t points);
void Z_Sort(Vec3_s16_t *elements, int16_t start, int16_t finish);

void Project_3d(Vec3_s16_t *src, Point2D_t *p2, u16 zpoint, Translate3D_t *camera);
void RotateY_3d(Point3D_t *p, float amount);
void RotateYC_3d(Point3D_t *src, Point3D_t *dst, float amount);
void RotateX_3d(Point3D_t *p, float amount);
void RotateZ_3d(Point3D_t *p, float amount);
void RotateZC_3d(Point3D_t *src, Point3D_t *dst, float amount);

void RotateZ_2d(Point2D_t *src, Point2D_t *dst, float amount);


void Z_Order(Vec3_s16_t *elements, int16_t low, int16_t high, int16_t ttl);
void Make_Rotation_Matrix(Translate3D_t *trans);
void Translate_All(Vec3_s16_t *src, Vec3_s16_t *dst, Translate3D_t *trans, uint8_t num);
#endif
