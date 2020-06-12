#ifndef __MAIN_H
#define __MAIN_H 1

#ifdef __cplusplus
    extern "C" {
        #include "fb_driver.h"
    }
#else
    #include "fb_driver.h"
#endif


extern DemoConf *GCONF;
extern u8 floor_buffer;
extern char condition;
extern char global_text1[32];
extern char global_text2[32];

#endif