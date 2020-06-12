#include <fplus/fplus.hpp>
#include <iostream>
#include <lvgl/lvgl.h>

int main() {

	static lv_disp_buf_t disp_buf;
	static lv_color_t buf[LV_HOR_RES_MAX * 10];                     /*Declare a buffer for 10 lines*/
	lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);    /*Initialize the display buffer*/

	std::list<std::string> things = {"same old", "same old"};
	if (fplus::all_the_same(things)) {
		std::cout << "All things being equal." << std::endl;
	} else {
		std::cout << "Nothing will ever be the same." << std::endl;
	}
}
