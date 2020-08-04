#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include "system.h"
#include "mathcore.h"
#include "util.h"

#include "fplus/fplus.hpp"

template <typename String, typename ContainerOut = std::vector<String>>
ContainerOut split_words_space(const bool allowEmpty, const String &str)
{
    return fplus::split_by(fplus::is_whitespace<String>, allowEmpty, str);
}


/**
 * create a series of numbers from start to end incrementing by step at the memory pointed to by addr
 * step may be negative or positive, start may be <> end.
 * addr must be 32bit integer
 */
void range32(int start, int end, int *addr, s16 step) {
    
    int ctr = 0;
    if (start > end && step < 0) {
        for (int i=start; i>=end; i+=step) { addr[ctr++] = i; }
    } else {
        for (int i=start; i<=end; i+=step) { addr[ctr++] = i; }
    }

}

void range_f(float start, float end, float *addr, float step) {
    int ctr = 0;
    for (float i=start; i<=end; i+=step) {
        addr[ctr] = i;
    }
}

/**
 * begin counting timer for a wait
 */
void delay_begin(struct timeval *t1) {
    u8 err = gettimeofday(t1, NULL);
    if (err) {
        Panic(99, "gettimeofday failed\n");
    }
}

/**
 * waits microseconds from the last call to delay_begin, or 0 if current time is after microseconds
 */
void delay_end(struct timeval *t1, struct timeval *t2, unsigned long microseconds) {
    u8 err = gettimeofday(t2, NULL);
    if (err) {
        Panic(99, "gettimeofday failed\n");
    }

    signed long waited = ((t2->tv_sec * 1000000 + t2->tv_usec) - (t1->tv_sec * 1000000 + t1->tv_usec));
    waited = MIN(0, waited);
    u32 diff = microseconds - waited;
    if (diff < 1) { Panic(99, "can't wait < 1 micro %ld! [%d / %ld]\n", diff, microseconds, waited); }
    if (diff > 100000) { Panic(99, "can't wait > 1 second %ld!\n", diff); }
    usleep(diff);
}

/**
 * signed unsigned int compare,
 * returns -1,0,1 
 */
int su_cmp(int c1, unsigned int c2) {
    if (c1 >= 0 && c1 >= c2) {
        return ((unsigned int)c1 > c2) ? 1 : 0;
    }
    return -1;
}


/* This function takes last element as pivot, places 
   the pivot element at its correct position in sorted 
    array, and places all smaller (smaller than pivot) 
   to left of pivot and all greater elements to right 
   of pivot */
   /*
uint16_t Quick_Sort16_Part(Vec3_s16_t *elements, uint16_t *z_order, uint16_t low, uint16_t high) { 
    int16_t pivot = elements[high];    // pivot 
    uint16_t i = (low - 1);  // Index of smaller element 
  
    for (uint16_t j = low; j <= high-1; j++) {
        
        // If current element is smaller than the pivot
        if (elements[j] < pivot) {
            ++i;    // increment index of smaller element
            swap16(&elements[i], &elements[j]);
        }
    }
    swap16(&elements[i + 1], &elements[high]);
    return (i + 1);
}
*/
 
