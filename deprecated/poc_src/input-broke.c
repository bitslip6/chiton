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

#include <sys/mman.h>

#include "input.h"
#include "main.h"


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
    fb_write_string(msg, buffx, buffy, fgcolor, buffer);
	tapmap.callbacks[tapmap.len] = fp;
	tapmap.map_min_x[tapmap.len] = buffx-5;
	tapmap.map_min_y[tapmap.len] = buffy-5;
	tapmap.map_max_y[tapmap.len] = buffy+20;
	tapmap.map_max_x[tapmap.len] = buffx+5 + (strlen(msg) * 18);
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
        usleep(500000);
        printf("READ...\n");
        // get ui button
        /*
        // TODO: add button callbacks
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
        */
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


//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013


// Access from ARM Running Linux

// #define BCM2708_PERI_BASE        0x20000000
#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

void setup_io();

void printButton(int g)
{
  // !=0 <-> bit is 1 <- port is HIGH=3.3V
  if (GET_GPIO(g)) {
    printf("Button pressed!\n");
  }  
  // port is LOW=0V
  else {
    printf("Button released!\n");
  }
}


/*
int main(int argc, char **argv)
{
  int g,rep;

  // Set up gpi pointer for direct register access
  setup_io();

  // Switch GPIO 7..11 to output mode

 // ************************************************************************
  * You are about to change the GPIO settings of your computer.          *
  * Mess this up and it will stop working!                               *
  * It might be a good idea to 'sync' before running this program        *
  * so at least you still have your code changes written to the SD-card! *
 // ************************************************************************

  // Set GPIO pins 7-11 to output
  for (g=7; g<=11; g++)
  {
    INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
    OUT_GPIO(g);
  }

  for (rep=0; rep<10; rep++)
  {
     for (g=7; g<=11; g++)
     {
       GPIO_SET = 1<<g;
       sleep(1);
     }
     for (g=7; g<=11; g++)
     {
       GPIO_CLR = 1<<g;
       sleep(1);
     }
  }

  return 0;

} // main
*/


//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io
