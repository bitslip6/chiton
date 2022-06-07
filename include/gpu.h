#include "CL/cl.h"

namespace GPU {

	typedef struct cl_code {
		cl_command_queue queue;
		cl_kernel kernel;
		cl_program program;
		cl_context context;

		cl_mem ptr1;
		cl_mem ptr2;
		cl_mem ptr3;
		cl_mem ptr4;
	} cl_code;

	void release_cl_function(cl_code *func);
	cl_code* make_cl_function(char *src_file, char *function_name);
    void test();

}

