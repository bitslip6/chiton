#include <stdint.h>
#include <math.h>
#include "3d.h"
#include "demos.h"
#include "lut.h"
#include "util.h"

void Project_3d(Vec3_s16_t *src, Point2D_t *p2, u16 zpoint, Translate3D_t *camera) {
    float z = (float)zpoint/(zpoint + (src->d[VEC_Z] + camera->translation.d[VEC_Z]));
    p2->x = (s16)(src->d[VEC_X] + camera->translation.d[VEC_X]) * z;
    p2->y = (s16)(src->d[VEC_Y] + camera->translation.d[VEC_Y]) * z;
    //printf("project: [%dx%d - %f]\n", p2->x, p2->y, z);
}

/**
 * update a translation cos and sin values from the rotation thetas
 */
void Make_Rotation_Matrix(Translate3D_t *trans) {
    trans->r_cos.d[VEC_X]=cosf(trans->rotation.d[VEC_X]);
    trans->r_cos.d[VEC_Y]=cosf(trans->rotation.d[VEC_Y]);
    trans->r_cos.d[VEC_Z]=cosf(trans->rotation.d[VEC_Z]);
    trans->r_sin.d[VEC_X]=sinf(trans->rotation.d[VEC_X]);
    trans->r_sin.d[VEC_Y]=sinf(trans->rotation.d[VEC_Y]);
    trans->r_sin.d[VEC_Z]=sinf(trans->rotation.d[VEC_Z]);

    trans->rotation_matrix.x[0][0] = trans->r_cos.d[VEC_X] * trans->r_cos.d[VEC_Y];
    trans->rotation_matrix.x[0][1] = (trans->r_cos.d[VEC_X] * trans->r_sin.d[VEC_Y] * trans->r_sin.d[VEC_Z]) - (trans->r_sin.d[VEC_X] * trans->r_cos.d[VEC_Z]);
    trans->rotation_matrix.x[0][2] = (trans->r_cos.d[VEC_X] * trans->r_sin.d[VEC_Y] * trans->r_cos.d[VEC_Z]) + (trans->r_sin.d[VEC_X] * trans->r_sin.d[VEC_Z]);

    trans->rotation_matrix.x[1][0] = (trans->r_sin.d[VEC_X] * trans->r_cos.d[VEC_Y]);
    trans->rotation_matrix.x[1][1] = (trans->r_sin.d[VEC_X] * trans->r_sin.d[VEC_Y] * trans->r_sin.d[VEC_Z]) + (trans->r_cos.d[VEC_X] * trans->r_cos.d[VEC_Z]);
    trans->rotation_matrix.x[1][2] = (trans->r_sin.d[VEC_X] * trans->r_sin.d[VEC_Y] * trans->r_cos.d[VEC_Z]) - (trans->r_cos.d[VEC_X] * trans->r_sin.d[VEC_Z]);
    
    trans->rotation_matrix.x[2][0] = -(trans->r_sin.d[VEC_Y]);
    trans->rotation_matrix.x[2][1] = trans->r_cos.d[VEC_Y] * trans->r_sin.d[VEC_Z];
    trans->rotation_matrix.x[2][2] = (trans->r_cos.d[VEC_Y] * trans->r_cos.d[VEC_Z]);
    

    #ifdef __3D_DEBUG
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdouble-promotion"
    //printf("  COS-Z: %f = [%f] (%f)\n", trans->rotation.d[VEC_Y], trans->r_sin.d[VEC_Y], cosf(0.0f));
    printf("ROTATION MATRIX1: %f, %f, %f\n", 
        trans->rotation_matrix.x[0][VEC_X],
        trans->rotation_matrix.x[0][VEC_Y],
        trans->rotation_matrix.x[0][VEC_Z]
    );
    #pragma GCC diagnostic pop
    #endif
    // STATIC INIT EXAMPLE:
    /* Matrix33f rotation = {{ {trans->cz + trans->cy, -(trans->sz), trans->sy}, {trans->sz, trans->cz, 0}, {-(trans->sy), 0, trans->cy} }}; */
}

void Translate_All(Vec3_s16_t *src, Vec3_s16_t *dst, Translate3D_t *trans, uint8_t num) {

    //DEBUG_VEC3PTR(src);
    //DEBUG_VEC3PTR(dst);
    memset(dst, 0, sizeof(Vec3_s16_t)*num);
    //DEBUG_VEC3PTR(dst);

    for (uint8_t i=0; i<num; i++) {

        for (uint8_t s1=0; s1<3; s1++) {
            for (uint8_t s2=0; s2<3; s2++) {
                dst[i].d[s1] += trans->rotation_matrix.x[s1][s2] * src[i].d[s2];
                //printf("DO MATRIX\n");
            }
            dst[i].d[s1] += trans->translation.d[s1];
        }

    }
}

void RotateZ_2d(Point2D_t *src, Point2D_t *dst, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    dst->x = (src->x * c) - (src->y * s);
    dst->y = (src->y * c) + (src->x * s);
}


void RotateZ_3d(Point3D_t *p, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    float t = (p->x * c) - (p->y * s);
    p->y = (p->y * c) + (p->x * s);
    p->x = (int)t;
}

void RotateZC_3d(Point3D_t *src, Point3D_t *dst, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    dst->x = (src->x * c) - (src->y * s);
    dst->y = (src->y * c) + (src->x * s);
}



void RotateX_3d(Point3D_t *p, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    float t = (p->y * c) + (p->z * s);
    p->y = (p->z * c) - (p->y * s);
    p->z = (int)t;
}

void RotateY_3d(Point3D_t *p, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    float t = (p->x * c) + (p->z * s);
    p->z = (p->z * c) - (p->x * s);
    p->x = (int)t;
}

void RotateYC_3d(Point3D_t *src, Point3D_t *dst, float amount) {
    float s = sinf(amount);
    float c = cosf(amount);

    dst->x = (src->x * c) + (src->z * s);
    dst->z = (src->z * c) - (src->x * s);
}

/* The main function that implements QuickSort
 arr[] --> Array to be sorted,
  low  --> Starting index,
  high  --> Ending index
*/
void Z_Order(Vec3_s16_t *elements, int16_t low, int16_t high, int16_t ttl) {
    if (ttl-- < 1) { return; }
    if (low < high) {
        
        Vec3_s16_t pivot = elements[high];
        Vec3_s16_t temp;
        int16_t i = (low - 1);
      
        for (int16_t j = low; j <= high-1; j++) {
            
            // If current element is smaller than the pivot
            if (elements[j].d[VEC_Z] > pivot.d[VEC_Z]) {
                ++i;    // increment index of smaller element
                temp = elements[i];
                elements[i] = elements[j];
                elements[j] = temp;
            }
        }
        
        temp = elements[i+1];
        elements[i+1] = elements[high];
        elements[high] = temp;
        i++;

        // Separately sort elements before
        // partition and after partition
        Z_Order(elements, low, i - 1, ttl);
        Z_Order(elements, i + 1, high, ttl);
    }
}
