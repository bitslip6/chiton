__kernel void vector_calibrate(__global const unsigned short *src, __global const float *gain, __global const float *offset, __global unsigned short *dst) {
    
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    // Do the operation
    dst[i] = (src[i] * gain[i]) + offset[i];
}
