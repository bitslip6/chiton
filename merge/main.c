/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "XX.YY.ZZ";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <math.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                12
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                   12 
#define HEIGHT                  37 
#define LED_COUNT               (WIDTH * HEIGHT)

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;
//int height_map[WIDTH] = {19, 20, 20, 20, 20, 20, 19, 19, 20, 20, 20, 20, 20, 20,  19, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17};
int height_map[WIDTH] = {37, 37, 37, 33, 30, 26, 26, 30, 33, 37, 37, 37 };//20, 20, 20, 20, 20, 19, 19, 20, 20, 20, 20, 20, 20,  19, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17};

int clear_on_exit = 0;


// basic frame buffer info
typedef struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
} ScreenInfo;


typedef struct matrixdot_t {
	int xpos;
	double ypos;
	double speed;
	unsigned char h;
} matrixdot_t;


ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t *matrix;

static uint8_t running = 1;
static double plasma[600];
//static struct matrixdemo_t matrixdemo;

void Panic(const char *msg, int code) {
    perror(msg);
    exit(code);
}

void matrix_render2(void) {

    int ctr = 0;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height_map[x]; y++) {
            ledstring.channel[0].leds[ctr++] = matrix[(y * width) + x];
        }

        x++;
        if (x < width) {
            for (int y = 0; y < height_map[x]; y++) {
                int by = height_map[x] -1 - y;
                //int by = y;//height_map[x] - y;
                ledstring.channel[0].leds[ctr++] = matrix[(by * width) + x];
            }
        }
    }
}



int HsvToRgb(unsigned char h, unsigned char s, unsigned char v) {
    //RgbColor rgb;

	//unsigned char r, g, b;
    unsigned char region, remainder, p, q, t;

    if (s <= 0) {
        return 0;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
			return v << 16 | t << 8 | p;
            //rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
			return q << 16 | v << 8 | p;
            //rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
			return p << 16 | v << 8 | t;
            //rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
			return p << 16 | q << 8 | v;
            //rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
			return t << 16 | p << 8 | v;
            //rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
			return v << 16 | p << 8 | q;
            //rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return 255<<16|255<<8|255;
}


void create_plasma(float xoff, float yoff) {
	for (int y=0; y<height; y++) {
		for (int x=0; x<width; x++) {
			double value = sin((x +xoff)/ 16.0);
			value += sin((y + yoff) / 8);
			value += sin((x + xoff+ y + yoff) / 16.0);
			value += sin(sqrt(x * x + y * y) / 8.0);
			value += 4; // shift range from -4 .. 4 to 0 .. 8
			value /= 8; // bring range down to 0 .. 1
			plasma[(y*width)+x] = value;//0x00800000 + ((height * 8) << 8) + (width * 8);// value;
		}
	}
}

void animate_plasma() {
	double hueShift = 0;
    float xoff = 0.0;
    float yoff = 0.0;
	while(running) {
        create_plasma(xoff, yoff);
        xoff += 0.4;
        yoff += 0.25;
		hueShift += 0.02;
		if (hueShift > 1) { hueShift -= 1; }

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {

				double hue = (hueShift + plasma[(y*width)+x]);
				if (hue > 1) { hue -= 1; }

				unsigned char sh = floor(hue * 255);
				matrix[(y * width) + x] =  HsvToRgb(sh, 240, 32);
			}
		}

        matrix_render2();

        if ((ws2811_render(&ledstring)) != WS2811_SUCCESS) {
            Panic("ws2811_render failed", 1);
        }

        // 32 frames /sec
        usleep(1000000 / 16);
	}
}





void Decay(int amount) {
    if (amount) {
        return;
    } else {
        return;
    }
/*
    int r = amount << 16;
    int g = amount << 8;
    int b = amount;
    
    //printf("decay %x %x, %x\n", r, g, b);

	// decay the whole screen
	for (int y = 0; y <= (height+1); y++) {
		for (int x = 0; x <= (width); x++) {
			//u_int32_t val = buffer[(y*1920)+x];
			u_int32_t val = matrix[(y*width)+x];

			//u_int32_t val = buffer[(ypos2*1920*4)+dot.xpos*4];

			if ((val & 0x00FF0000) && (val >= r)) {
				//buffer[(y*1920) + x] -= r;
				matrix[(y*width) + x] -= r;
			}
			if ((val & 0x0000FF00) && (val >= g)) {
				//buffer[(y*1920) + x] -= g;
				matrix[(y*width) + x] -= g;
			}
			if ((val & 0x000000FF) && (val >= b)) {
				//buffer[(y*1920) + x] -= b;
				matrix[(y*width) + x] -= b;
			}
		}

		//printf("\n");
	}
*/
}


/**
 * create single matrix dot
 */
matrixdot_t new_matrix_dot() {
	printf("animating [%d x %d]...\n", width, height);
	matrixdot_t dot;
	dot.xpos = rand() % width;
	dot.ypos = 15 + (double)rand()/RAND_MAX * height;
	dot.speed = -0.08 - ((double)rand()/RAND_MAX);
	// set the hue to black for 1/5 of all drops...
	//dot.h = ((rand()%10) < 2) ? 0.2 : (double)rand()/RAND_MAX * 2;
	dot.h = (rand() % 255);
	if (dot.h < 32) { dot.h = 0; }
	//else if (dot.h < 128) { dot.h = 100; }
	else { dot.h = 50; }
	printf("x:%d y:%f / (%d), s:%f, h:%d\n", dot.xpos, dot.ypos, height, dot.speed, dot.h);
	return dot;
}

/**
 * create entire matrix dot
 * returns a pointer to num matrixdot_t that must be freed!
 */
matrixdot_t* create_matrix(int num) {
	matrixdot_t *dots = malloc(sizeof(matrixdot_t) * num);
	for (int i=0; i<num; i++) {
		dots[i] = new_matrix_dot();
	}

	return dots;
}



void decay_screen(int amount) {
    int r = amount << 16;
    int g = amount << 8;
    int b = amount;

	// fade the whole screen
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int val = matrix[(y*width)+x];
			if ((val & 0x00FF0000) && (val > 0x0001FFFF)) {
				matrix[(y*width) + x] -= r;
			}
			if ((val & 0x0000FF00) && (val > 0x000001FF)) {
				matrix[(y*width) + x] -= g;
			}
			if ((val & 0x000000FF) && (val > 0x00000001)) {
				matrix[(y*width) + x] -= b;
			}
		}
	}
}

void animate_matrix(int num) {

    matrixdot_t *dots = create_matrix(num);

	while(running) {

		// decay the dropping dots
		if (rand() % 10 < 9) {
			Decay(1);
		}

		// animate the falling lights...
		for (int i=0; i<num; i++) {
			matrixdot_t dot = dots[i];

			// advance the dot down...
			int ypos1 = floor(dot.ypos);
			//printf("Y1: %f ", dot.ypos);
			dot.ypos += dot.speed;
			dots[i] = dot;
			//printf("Y2: %f \n", dot.ypos);
			int ypos2 = floor(dot.ypos);

			// if we advanced along y, then set previous dot to normal value
			// set the new dot to bright white...
			if (ypos1 >= 0 && ypos1 != ypos2) {
				// previous
				if (ypos1 >= 0 && ypos1 <= height) {
					//put_pixel(dot.xpos,  ypos1,  HsvToRgb(dot.h, (dot.h == 0) ? 0 : 192, 64), buffer);
                    matrix[(ypos1*width)+dot.xpos] = HsvToRgb(dot.h, (dot.h == 0) ? 0 : 192, 64);
				}
				// new
				if (ypos2 >= 0 && ypos2 <= height) {
                    matrix[(ypos2*width)+dot.xpos] = HsvToRgb(dot.h, (dot.h == 0) ? 0 : 64, 250);
                    //matrix[dot.xpos, ypos2] = HsvToRgb(dot.h, (dot.h == 0) ? 0 : 64, 64);
					//put_pixel(dot.xpos, ypos2, HsvToRgb(dot.h, (dot.h == 0) ? 0 : 64, 250), buffer);
					//u_int32_t val = buffer[(ypos2*1920*4)+dot.xpos*4];
					//printf("wrote  val %d : %d, %d\n", val, ypos2, dot.xpos);

				}
			} else {
                if (dot.ypos < (-height)) {
                    dots[i] = new_matrix_dot();
                }
            }
		}

        matrix_render2();

        if ((ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            printf("ws render failed\n");
            exit(4);
        
        }

/*
	while(running) {


		// animate the falling lights...
		for (int i=0; i<num; i++) {
			int ypos = floor(matrixdemo.ypos[i]);
			matrix[(ypos * width) + matrixdemo.xpos[i]] &= 0x0000ff00;

			matrixdemo.ypos[i] += matrixdemo.speed[i];
			if (matrixdemo.ypos[i] > height * 2) {
				matrixdemo.xpos[i] = rand() * width;
				matrixdemo.ypos[i] = 0;
				matrixdemo.speed[i] = (0.2 + rand());
				matrixdemo.color[i] = (rand() * 200)<<8; 
				if (matrixdemo.color[i] < 32) {
					matrixdemo.color[i] = 0;
				}
			}

			ypos = floor(matrixdemo.ypos[i]);
			matrix[(ypos * width) + matrixdemo.xpos[i]] = 0x00c0ffc0;
		}

		// display output...
        matrix_render2();

        if ((ws2811_render(&ledstring)) != WS2811_SUCCESS) {
            Panic("ws2811_render failed", 2);
        }
        */

        // 32 frames /sec
        usleep(1000000 / 32);
	}
}



/*
int hsv2rgb(double h, double s, double v) {
    double      hh, p, q, t, ff;
    long        i;
    double      out;

    if(s <= 0.0) {       // < is bogus, just shuts up warnings
        return 0;
    }
    hh = h;
    if(hh >= 360.0) {
		hh = 0.0;
	}
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch(i) {
    case 0:
        out = v<<16;
        out |= t;
        out |= p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}
*/


/*
void matrix_render(void)
{
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
        }
    }
}



void matrix_raise(void)
{
    int x, y;

    for (y = 0; y < (height - 1); y++)
    {
        for (x = 0; x < width; x++)
        {
            // This is for the 8x8 Pimoroni Unicorn-HAT where the LEDS in subsequent
            // rows are arranged in opposite directions
            matrix[y * width + x] = matrix[(y + 1)*width + width - x - 1];
        }
    }
}
*/

void matrix_clear(void)
{
    int x, y;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = 0;
        }
    }
}

/*
int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00002000,  // green
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};


void matrix_bottom(void)
{
    int i;

    for (i = 0; i < (int)(ARRAY_SIZE(dotspos)); i++)
    {
        dotspos[i]++;
        if (dotspos[i] > (width - 1))
        {
            dotspos[i] = 0;
        }

		matrix[dotspos[i] + (height - 1) * width] = dotcolors[i];
    }
}
*/

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv, ws2811_t *ws2811)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"dma", required_argument, 0, 'd'},
		{"gpio", required_argument, 0, 'g'},
		{"invert", no_argument, 0, 'i'},
		{"clear", no_argument, 0, 'c'},
		{"strip", required_argument, 0, 's'},
		{"height", required_argument, 0, 'y'},
		{"width", required_argument, 0, 'x'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "cd:g:his:vx:y:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-s (--strip)   - strip type - rgb, grb, gbr, rgbw\n"
				"-x (--width)   - matrix width (default 8)\n"
				"-y (--height)  - matrix height (default 8)\n"
				"-d (--dma)     - dma channel to use (default 10)\n"
				"-g (--gpio)    - GPIO to use\n"
				"                 If omitted, default is 18 (PWM0)\n"
				"-i (--invert)  - invert pin output (pulse LOW)\n"
				"-c (--clear)   - clear matrix on exit.\n"
				"-v (--version) - version information\n"
				, argv[0]);
			exit(-1);

		case 'D':
			break;

		case 'g':
			if (optarg) {
				int gpio = atoi(optarg);
/*
	PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
	Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
	PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
	Only 13 is available on the B+/2B/PiZero/3B, on pin 33
	PCM_DOUT, which can be set to use GPIOs 21 and 31.
	Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
	SPI0-MOSI is available on GPIOs 10 and 38.
	Only GPIO 10 is available on all models.

	The library checks if the specified gpio is available
	on the specific model (from model B rev 1 till 3B)

*/
				ws2811->channel[0].gpionum = gpio;
			}
			break;

		case 'i':
			ws2811->channel[0].invert=1;
			break;

		case 'c':
			clear_on_exit=1;
			break;

		case 'd':
			if (optarg) {
				int dma = atoi(optarg);
				if (dma < 14) {
					ws2811->dmanum = dma;
				} else {
					printf ("invalid dma %d\n", dma);
					exit (-1);
				}
			}
			break;

		case 'y':
			if (optarg) {
				height = atoi(optarg);
				if (height > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid height %d\n", height);
					exit (-1);
				}
			}
			break;

		case 'x':
			if (optarg) {
				width = atoi(optarg);
				if (width > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid width %d\n", width);
					exit (-1);
				}
			}
			break;

		case 's':
			if (optarg) {
				if (!strncasecmp("rgb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RGB;
				}
				else if (!strncasecmp("rbg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RBG;
				}
				else if (!strncasecmp("grb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GRB;
				}
				else if (!strncasecmp("gbr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
				}
				else if (!strncasecmp("brg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BRG;
				}
				else if (!strncasecmp("bgr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BGR;
				}
				else if (!strncasecmp("rgbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_RGBW;
				}
				else if (!strncasecmp("grbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_GRBW;
				}
				else {
					printf ("invalid strip %s\n", optarg);
					exit (-1);
				}
			}
			break;

		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);

		default:
			exit(-1);
		}
	}
}


int main(int argc, char *argv[])
{
    //ws2811_return_t ret;
    srand(time(NULL));

    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    parseargs(argc, argv, &ledstring);

    matrix = malloc(sizeof(ws2811_led_t) * width * height);

    setup_handlers();
    exit(0);

/*
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
*/


	// create array of function pointers to demos... iterate through
	// advance on button press
	//animate_matrix(40);
    animate_plasma();

/*
    for (int x=0;x<width;x++) {
        for (int y=0;y<height;y++) {
            matrix[(y*width)+x] = 0x00200000;// + ((y * 4) << 16);
        }
        x++;
        for (int y=0;y<height;y++) {
            matrix[(y*width)+x] = 0x00002000;// + ((y * 4) << 8);
        }
        x++;
        for (int y=0;y<height;y++) {
            matrix[(y*width)+x] = 0x00000020;// + (y * 4);
        }
    }

    matrix_render2();
    if ((ws2811_render(&ledstring)) != WS2811_SUCCESS) {
        Panic("ws2811_render failed", 1);
    }
    */

/*
    while (running)
    {
        //matrix_raise();
        //matrix_bottom();
        matrix_render();

        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }

        // 15 frames /sec
        usleep(1000000 / 15);
    }
	*/

    if (clear_on_exit) {
	    matrix_clear();
        matrix_render2();
	    ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);

    printf ("\n");
    return 0;
}
