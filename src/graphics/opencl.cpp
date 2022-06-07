#include "gpu.h"
#include <sys/types.h>
#include <stdlib.h>
#include "utils.hpp"
#include "system.h"
#include <stdio.h>

#define MAX_SOURCE_SIZE (0x100000)
#define LIST_SIZE 64

namespace GPU
{

    void release_cl_function(cl_code *func)
    {
        if (func->ptr1 != NULL)
        {
            clReleaseMemObject(func->ptr1);
        }
        if (func->ptr2 != NULL)
        {
            clReleaseMemObject(func->ptr2);
        }
        if (func->ptr3 != NULL)
        {
            clReleaseMemObject(func->ptr3);
        }
        if (func->ptr4 != NULL)
        {
            clReleaseMemObject(func->ptr4);
        }

        clFlush(func->queue);
        clFinish(func->queue);
        clReleaseKernel(func->kernel);
        clReleaseProgram(func->program);
        clReleaseCommandQueue(func->queue);
        clReleaseContext(func->context);

        free(func);
    }

    float *make_float_list(float d, int sz)
    {
        float *list = NEWMEM(float, sz);
        for (int i = 0; i < sz; i++)
        {
            list[i] = d;
        }
        return list;
    }

    void test()
    {
        INFO("TEST 1");
        GPU::cl_code *code = GPU::make_cl_function("calibrate.cl", "vector_calibrate");
        INFO("TEST 2");
        cl_int ret = 0;

        // Create memory buffers on the device for each vector
        code->ptr1 = clCreateBuffer(code->context, CL_MEM_READ_ONLY, LIST_SIZE * sizeof(unsigned short), NULL, &ret);
        INFO("TEST 3 [%x], code->ptr1");
        code->ptr2 = clCreateBuffer(code->context, CL_MEM_READ_ONLY, LIST_SIZE * sizeof(float), NULL, &ret);
        code->ptr3 = clCreateBuffer(code->context, CL_MEM_READ_ONLY, LIST_SIZE * sizeof(float), NULL, &ret);
        code->ptr4 = clCreateBuffer(code->context, CL_MEM_WRITE_ONLY, LIST_SIZE * sizeof(unsigned short), NULL, &ret);
        INFO("TEST 4");

        float *A_gain = make_float_list(1.44f, LIST_SIZE);
        float *B_offset = make_float_list(128.0f, LIST_SIZE);
        float *C_src = make_float_list(2048.0f, LIST_SIZE);
        INFO("TEST 5");

        // copy gain and offset buffers
        ret = clEnqueueWriteBuffer(code->queue, code->ptr1, CL_TRUE, 0, LIST_SIZE * sizeof(float), C_src, 0, NULL, NULL);
        INFO("TEST 6");
        ret = clEnqueueWriteBuffer(code->queue, code->ptr2, CL_TRUE, 0, LIST_SIZE * sizeof(float), A_gain, 0, NULL, NULL);
        ret = clEnqueueWriteBuffer(code->queue, code->ptr3, CL_TRUE, 0, LIST_SIZE * sizeof(float), B_offset, 0, NULL, NULL);
        INFO("TEST 7");

        // Set the arguments of the kernel
        //ret = clSetKernelArg(code->kernel, 0, sizeof(cl_mem), (void *)&code->ptr1);
        ret = clSetKernelArg(code->kernel, 0, sizeof(cl_mem), code->ptr1);
        INFO("TEST 7 A");
        ret = clSetKernelArg(code->kernel, 1, sizeof(cl_mem), (void *)&code->ptr2);
        INFO("TEST 7 B");
        ret = clSetKernelArg(code->kernel, 2, sizeof(cl_mem), (void *)&code->ptr3);
        INFO("TEST 7 C");
        ret = clSetKernelArg(code->kernel, 3, sizeof(cl_mem), (void *)&code->ptr4);
        INFO("TEST 8");

        // Execute the OpenCL kernel on the list
        size_t global_item_size = LIST_SIZE; // Process the entire lists
        size_t local_item_size = 64;         // Divide work items into groups of 64
        ret = clEnqueueNDRangeKernel(code->queue, code->kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
        INFO("TEST 9");

        // Read the memory buffer C on the device to the local variable C
        u16 *RESULT = NEWMEM(u16, LIST_SIZE);
        ret = clEnqueueReadBuffer(code->queue, code->ptr4, CL_TRUE, 0, LIST_SIZE * sizeof(unsigned short), RESULT, 0, NULL, NULL);
        INFO("TEST 10");

        for (int i = 0; i < LIST_SIZE; i++)
        {
            printf("R: %d [%d]\n", i, RESULT[i]);
        }

        PANIC("FIN");
    }

    cl_code *make_cl_function(char *src_file, char *function_name)
    {
        // Load the kernel source code into the array source_str
        FILE *fp;
        char *source_str;
        size_t source_size;

        PANIC_IFNOT((fp = fopen(src_file, "r")), "unable to open cl program", EXIT_OPEN_CL);

        source_str = (char *)malloc(MAX_SOURCE_SIZE);
        source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
        fclose(fp);

        INFO("read [%d] src bytes\n", source_size);

        cl_code *code = (cl_code *)malloc(sizeof(cl_code));

        // Get platform and device information
        cl_platform_id platform_id = NULL;
        cl_device_id device_id = NULL;
        cl_uint ret_num_devices;
        cl_uint ret_num_platforms;
        cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
        INFO("platform ids return [%d] num platforms [%d] ID[%d]", ret, ret_num_platforms, platform_id);
        ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

        INFO("num devices [%d] device id: [%d]", ret_num_devices, ret);

        // Create an OpenCL context
        code->context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
        INFO("create ctx ret: [%d]", ret);

        // Create a command queue
        code->queue = clCreateCommandQueue(code->context, device_id, 0, &ret);
        INFO("create queue ret: [%d]", ret);

        // Create a program from the kernel source
        code->program = clCreateProgramWithSource(code->context, 1,
                                                  (const char **)&source_str, (const size_t *)&source_size, &ret);

        // Build the program
        ret = clBuildProgram(code->program, 1, &device_id, NULL, NULL, NULL);

        // Create the OpenCL kernel
        cl_kernel kernel = clCreateKernel(code->program, function_name, &ret);

        return code;
    }

}