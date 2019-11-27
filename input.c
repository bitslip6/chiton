#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/input.h>
#include <wiringPi.h>

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

// #include "main.h"
//#include "input.h"
//#include "fb_driver.h"

#ifdef __cplusplus
    extern "C" {
        #include "system.h"
        #include "fb_driver.h"
        #include "input.h"
        #include "main.h"
    }
#else
    #include "system.h"
    #include "fb_driver.h"
    #include "input.h"
    #include "main.h"
#endif

struct TapList AllCallbacks;
struct UIState UIConf;
struct TapMap tapmap;

/*
drag_callbacks[]

void (*dragCallback)(int sx, int sy, int dx, int dy);
void (*dragFinishCallback)(int sx, int sy, int dx, int dy);
void (*tapCallback)(int dx, int dy);
*/

void dragcall(int sx, int sy, int dx, int dy) {
	printf("DRAG CALLBACK %d,%d -> %d,%d\n", sx, sy, dx, dy);
}

void dragfinish(int sx, int sy, int dx, int dy) {
	printf("DRAG COMPLETE %d,%d -> %d,%d\n", sx, sy, dx, dy);
}

void tapcall(int sx, int sy) {
	printf("TAP CALLBACK %d,%d\n", sx, sy);
}


/**
 * render a string, register taps to fp callback
 * NOT threadsafe...
 */
void fb_active_string(DisplayBuffers *disp, char *msg, int buffx, int buffy, u8 fgcolor, VoidCallback *fp) {
    fb_write_string(disp, msg, buffx, buffy, fgcolor);
	tapmap.callbacks[tapmap.len] = *fp;
	tapmap.map_min_x[tapmap.len] = buffx-5;
	tapmap.map_min_y[tapmap.len] = buffy-5;
	tapmap.map_max_y[tapmap.len] = buffy+20;
	tapmap.map_max_x[tapmap.len] = buffx+5 + (strlen(msg) * 18);
    printf("active text BB [%d,%d]/[%d,%d]\n", buffx-5, buffy-5, buffx+5+(strlen(msg) * 18), buffy+20);
	tapmap.len++;
}

void clear_active_strings() {
	tapmap.len = 0;
}


/**
 * add a drag callback
 */
void add_drag_callback(DragCallback *fp) {
	AllCallbacks.drags[AllCallbacks.draglen++] = *fp;
}

/**
 * add a drag finished callback
 */
void add_drag_fin_callback(DragCallback *fp) {
	AllCallbacks.dragfin[AllCallbacks.dragfinlen++] = *fp;
}

/**
 * add a tap callback
 */
void add_tap_callback(TapCallback *fp) {
	AllCallbacks.taps[AllCallbacks.taplen++] = *fp;
}

/**
 * 
 */
int tap_line(int sx, int sy) {

	int selected_line = 0;
	// only handle bounding box selection in mode 2
	if (UIConf.mode != 2) {
		return 0;
	}

	int min_dist = 999;

	// handle the left x bounding box
	int dist = abs((int)GCONF->bb_min_x - sx) + abs(120 - sy);
	if (dist < min_dist) {
		selected_line = 1;
		min_dist = dist;
	}

	// handle the right x bounding box
	dist = abs((int)GCONF->bb_max_x - sx) + abs(120 - sy);
	if (dist < min_dist) {
		selected_line = 2;
		min_dist = dist;
	}

	// handle the upper y bounding box
	dist = abs(160 - sx) + abs((int)GCONF->bb_min_y - sy);
	if (dist < min_dist) {
		selected_line = 3;
		min_dist = dist;
	}

	// handle the lower y bounding box
	dist = abs(160 - sx) + abs((int)GCONF->bb_max_y - sy);
	if (dist < min_dist) {
		selected_line = 4;
		min_dist = dist;
	}

	return selected_line;
}

/**
 * read a digital pin with 5ms debounce time
 * reads until read values match with 5ms retry and 10 retrys
 */
int debounce_read(int pin) {
    int ctr = 10;
    int val = 10;

    while (ctr-- != 0) {
        val = digitalRead(pin);
        usleep(5000);
        if (val == digitalRead(pin)) {
            return val;
        }
        usleep(5000);
    }
    return val;
}



/**
 * handle ui button presses
 */
void *ui_input(void *foo) {
    printf("ui input thread running...\n");
    clear_active_strings();

    int update = 0;
	int fd;


    fd = open("/dev/input/event2", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        Panic(21, "open_port: Unable to open /dev/input/event5\n");
    }
    printf("touch screen opened\n");

    // Turn off blocking for reads, use (fd, F_SETFL, FNDELAY) if you want that
    //fcntl(fd, F_SETFL, 0);
    fcntl(fd, F_SETFL, FNDELAY);
    printf("touch screen no delay set\n");

    struct input_event ev[64];
	int drag = 0;
	int dx = 0;
    int dy = 0;
	int tx = 0;
    int ty = 0;

    int edit_value = 1;

    printf("running [%d]\n", GCONF->running);

    while (GCONF->running == 1) {
        usleep(50000);
        // get ui button
        /*
        if (debounce_read(17) == 1) {
            update = 40;
            return;
        } else if (debounce_read(18) == 1) {
            update = 40;
            switch (edit_value) {
                case 1:
                    GCONF->min_depth += 100;
                    break;
                case 2:
                    GCONF->min_depth += 100;
                    break;
                case 3:
                    GCONF->iron = !GCONF->iron;
                    break;
                case 4:
                    GCONF->bbon = !GCONF->bbon;
                    break;
            }
        } else if (debounce_read(19) == 1) {
            update = 40;
            switch (edit_value) {
                case 1:
                    GCONF->min_depth -= 100;
                    break;
                case 2:
                    GCONF->max_depth -= 100;
                    break;
                case 3:
                    GCONF->iron = !GCONF->iron;
                    break;
                case 4:
                    GCONF->bbon = !GCONF->bbon;
                    break;
            }
        } else if (debounce_read(20) == 1) {
            update = 40;
            edit_value = ((edit_value + 1) % 2) + 1;
        }

        // update the ui
        char ui_buff[64];
        if (update) {
            switch (edit_value) {
                case 1:
                    sprintf(ui_buff, "min dep: %d", GCONF->min_depth);
                    break;
                case 2:
                    sprintf(ui_buff, "max dep: %d", GCONF->min_depth);
                    break;
                case 3:
                    sprintf(ui_buff, "IR: %s", (GCONF->iron) ? "ON" : "OFF");
                    break;
                case 4:
                    sprintf(ui_buff, "Box: %s", (GCONF->bbon) ? "ON" : "OFF");
                    break;
            }
        }
    }
    */

	int n = read(fd, ev, sizeof(struct input_event) * 64);
    if (n < 1) {
        continue;
    }

	for (u8 i = 0; i < n / sizeof(struct input_event); i++) {

        //printf("ev code i[%d], code[%d]\n", i, ev[i].code);
		// y position update
		if (ev[i].code == 53) {
			ty = ev[i].value;
		}
		// x position update
		else if (ev[i].code == 54) {
			tx = 320 - ev[i].value;
            //tx = ev[i].value;
			// issue a drag update
			if (drag && (dx != tx || dy != ty)) {
				for (int z=0; z<AllCallbacks.draglen; z++) {
					AllCallbacks.drags[z](dx, dy, tx, ty);
				}
			}
			drag = 1;
		}

		// handle touch up/down
		if (ev[i].code == 330) {

			// begin touch
			if (ev[i].value == 1) {
				dx = tx;
				dy = ty;
			}
			// end touch
			else {
				if (drag == 1) {
					// registered drag but x and y did not move, issue tap
					if (abs(tx - dx) + abs(ty - dy) > 7) {
						for (int z=0; z<AllCallbacks.dragfinlen; z++) {
                            DragCallback fp = AllCallbacks.dragfin[z];
							(fp)(dx, dy, tx, ty);
						}
					} else {
                        register_tap(tx, ty);
                    }
				}
				// just a tap
				else {
                    register_tap(tx, ty);
                    /*
					for (int z=0; z<AllCallbacks.taplen; z++) {
                        TapCallback fp = AllCallbacks.taps[z];
                        (*fp)(tx, ty);
					}
                    printf("test len[%d]\n", tapmap.len);
                    for (int z=0; z<tapmap.len; z++) {
                        VoidCallback fp = tapmap.callbacks[z]; 
                        if (tx >= tapmap.map_min_x[z] &&
                            tx <= tapmap.map_max_x[z] &&
                            ty >= tapmap.map_min_y[z] &&
                            ty >= tapmap.map_max_y[z]) {
                            (*fp)(tx, ty);
                        }
                    }
                    */
				}

				// reset values
				drag = dx = dy = tx = ty = 0;
			}
		}
    }
    }
    printf("UI while finished [%d]\n", GCONF->running);
}

void register_tap(int tx, int ty) {

    for (int z=0; z<AllCallbacks.taplen; z++) {
        TapCallback fp = AllCallbacks.taps[z];
        (*fp)(tx, ty);
    }

    for (int z=0; z<tapmap.len; z++) {
        VoidCallback fp = tapmap.callbacks[z]; 
        if (tx >= tapmap.map_min_x[z] &&
            tx <= tapmap.map_max_x[z] &&
            ty >= tapmap.map_min_y[z] &&
            ty <= tapmap.map_max_y[z]) {
            (*fp)();
        }
    }
}

