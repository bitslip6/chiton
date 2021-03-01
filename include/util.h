#ifndef UTIL_H
#define UTIL_H 1


/* Two new control structures for C. Yay! */
#define loop      for (;;)
#define loopend(x) {if(x)break;}
/* Usage:  do { ... } until (boolean); */
#define until(a)  while (!(a))

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "demos.h"



/**
 * RANDOM FUNCTIONS
 * This is skewed due to integer division, but
 * good enough when a is much smaller than RAND_MAX.
 */
#define RANDOMIZE() (srand((UINT)time( (time_t *)NULL )))
#define RANDOM(a) (rand() / (RAND_MAX / (a) + 1))
#define RANDOM_F(a) ((float)rand()/(float)(RAND_MAX)) * a;
#define RANDOM_FMM(mn, mx) (mn + (((float)rand()/(float)(RAND_MAX)) * mx));

#define SAFE_FREE(x) if (x != NULL) { free(x); }

/* This seems nifty to me */
#define DONE        1
#define NOT_DONE	0

#define SWAP_X(E, A, B, T) { T=E[A]; E[A]=B; E[B]=T; }

// swaps two 8bit numbers 
static inline void swap8(char* a , char*b) { 
    char temp = *a; 
    *a = *b; 
    *b = temp; 
} 

// swaps two 16bit numbers 
static inline void swap16(short* a , short*b) { 
    short temp = *a; 
    *a = *b; 
    *b = temp; 
} 

// swaps two 32bit numbers 
static inline void swap32(int* a , int*b) { 
    int temp = *a; 
    *a = *b; 
    *b = temp; 
} 





typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned short db_t;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

typedef int i2c_write_t;
typedef int i2c_read_t;

//number base for std::stoi
#define STOI_DECIMAL_BASE 10
#define FIRST_POS 0



// range calculation functions
void range32(int start, int end, int *addr, s16 step);
void range_f(float start, float end, float *addr, float step);

// delay and timing functions
void delay_begin(struct timeval *t1);
void delay_end(struct timeval *t1, struct timeval *t2, unsigned long microseconds);

// different type comparison
int su_cmp(int c1, unsigned int c2);


// sort functions
//typedef bool (SortCallback)(void *elm1, void *elm2);
//uint16_t Quick_Sort_16(int16_t *elements, uint16_t low, uint16_t high);
//void Quick_Sort(int16_t *elements, uint16_t low, uint16_t high);
//void Quick_Sort16(int16_t *elements, uint16_t low, uint16_t high);
//void Z_Order(Vec3_s16_t *box2, int16_t *z_order, uint16_t low, uint16_t high);


#endif
