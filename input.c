#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/input.h>

#include <stdint.h>
#include <stdlib.h>


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
void fb_active_string(char *msg, int buffx, int buffy, u_int16_t fgcolor, u_int16_t *buffer, VoidCallback *fp) {
{
    fb_write_string(msg, buffx, buffy, fgcolor, buffer);
	tapmap.callbacks[tapmap.len] = *fp;
	tapmap.map_min_x[tapmap.len] = buffx-5;
	tapmap.map_min_y[tapmap.len] = buffy-5;
	tapmap.map_max_y[tapmap.len] = buffy+20;
	tapmap.map_max_x[tapmap.len] = buffx+5 + (strlen(msg) * 18);
	tapmap++;
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
		return;
	}

	int min_dist = 999;

	// handle the left x bounding box
	int dist = abs(g_conf.bb_min_x - sx) + abs(120 - sy);
	if (dist < min_dist) {
		selected_line = 1;
		min_dist = dist;
	}

	// handle the right x bounding box
	int dist = abs(g_conf.bb_max_x - sx) + abs(120 - sy);
	if (dist < min_dist) {
		selected_line = 2;
		min_dist = dist;
	}

	// handle the upper y bounding box
	int dist = abs(160 - sx) + abs(g_conf.bb_min_y - sy);
	if (dist < min_dist) {
		selected_line = 3;
		min_dist = dist;
	}

	// handle the lower y bounding box
	int dist = abs(160 - sx) + abs(g_conf.bb_max_y - sy);
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
    int ctr, val = 10;

    while (ctr-- != 0) {
        val = digitalRead(pin);
        useleep(5000);
        if (val == digitalRead(pin)) {
            return val;
        }
        useleep(5000);
    }
    return val;
}



/**
 * handle ui button presses
 */
void *ui_input(void *foo) {

    int update = 0;
	int fd;

    fd = open("/dev/input/event0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        printf("open_port: Unable to open /dev/input/event0\n");
        exit(2);
    }

    // Turn off blocking for reads, use (fd, F_SETFL, FNDELAY) if you want that
    //fcntl(fd, F_SETFL, 0);
    fcntl(fd, F_SETFL, FNDELAY);

    struct input_event ev[64];
	int drag = 0;
	int dx, dy = 0;
	int tx, ty = 0;


    while (running) {
        usleep(100000)
        // get ui button
        if (debounce_read(17) == 1) {
            update = 40;
            return;
        } else if (debounce_read(18) == 1) {
            update = 40;
            switch (edit_value) {
                case 1:
                    g_conf.min_depth += 100;
                    break;
                case 2:
                    g_conf.min_depth += 100;
                case 3:
                    g_conf.iron = !g_conf.iron;
                case 4:
                    g_conf.bbon = !g_conf.bbon;
            }
        } else if (debounce_read(19) == 1) {
            update = 40;
            switch (edit_value) {
                case 1:
                    g_conf.min_depth -= 100;
                case 2:
                    g_conf.max_depth -= 100;
                case 3:
                    g_conf.iron = !g_conf.iron;
                case 4:
                    g_conf.bbon = !g_conf.bbon;
            }
        } else if (debounce_read(20) == 1) {
            update = 40;
            edit_value = ((edit_value + 1) % 2) + 1;
        }

        // update the ui
        if (update) {
            switch (edit_value) {
                case 1:
                    sprintf(ui_buff, "min dep: %d", g_conf.min_depth);
                case 2:
                    sprintf(ui_buff, "max dep: %d", g_conf.min_depth);
                case 3:
                    sprintf(ui_buff, "IR: %s", (g_conf.iron == true) ? "ON" : "OFF");
                case 4:
                    sprintf(ui_buff, "Box: %s", (g_conf.bbon == true) ? "ON" : "OFF");
            }
        }
    }

	int n = read(fd, ev, sizeof(ev) * 64);

	for (int i = 0; i < n / sizeof(struct input_event); i++) {

		// y position update
		if (ev[i].code == 53) {
			ty = ev[i].value;
		}
		// x position update
		else if (ev[i].code == 54) {
			tx = 320 - ev[i].value;
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
							AllCallbacks.dragfin[z](dx, dy, tx, ty);
						}
					} else {
						for (int z=0; z<AllCallbacks.taplen; z++) {
							AllCallbacks.taps[z](tx, ty);
						}
					}
				}
				// just a tap
				else {
					for (int z=0; z<AllCallbacks.taplen; z++) {
						AllCallbacks.taps[z](tx, ty);
					}
				}

				// reset values
				drag = dx = dy = tx = ty = 0;
			}
		}
    }
	}
}
