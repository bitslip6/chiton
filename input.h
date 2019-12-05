#include <sys/types.h>
#ifndef TFT_INPUT_H
#define TFT_INPUT_H 1

#include "fb_driver.h"

typedef void (*DragCallback)(int sx, int sy, int dx, int dy);
typedef void (*TapCallback)(int sx, int sy);
typedef void (*VoidCallback)();

struct TapMap {
	VoidCallback callbacks[50];

	int map_min_x[50];
	int map_max_x[50];
	int map_min_y[50];
	int map_max_y[50];

	int len;
};

struct TapList {
	DragCallback drags[50];
	DragCallback dragfin[50];
	TapCallback  taps[50];

	int draglen;
	int dragfinlen;
	int taplen;
};

struct UIState {
	int mode = 0;
};




void fb_active_string(DisplayBuffers *disp, char *msg, int buffx, int buffy, u8 fgcolor, VoidCallback *fp);
//void fb_active_string(char *msg, int buffx, int buffy, u_int16_t fgcolor, u_int16_t *buffer, VoidCallback *fp);
void clear_active_strings();
void add_drag_callback(DragCallback *fp);
void add_drag_fin_callback(DragCallback *fp);
void add_tap_callback(TapCallback fp);
int tap_line(int sx, int sy);
void register_tap(int tx, int ty);

void *ui_input(void *foo);

#endif