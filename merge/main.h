#ifndef __MAIN_H__
#define __MAIN_H__

#include "system.h"
#include "fb_driver.h"

// global system configuration
typedef struct GlobalOpts {
    u_int8_t clear_exit;
    u_int8_t running;
} GlobalOpts;



GlobalOpts *GOPTS;
DemoConf *GCONF;
DisplayBuffers *GBUF;

#endif
