#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <wiringPi.h>
#include <softPwm.h>


#include <fcntl.h>
#include <linux/i2c-dev.h>

#ifdef __cplusplus
    extern "C" {
        #include "main.h"
        #include "system.h"
        #include "borg_driver.h"
        #include "input.h"
    }
#else
    #include "main.h"
    #include "system.h"
    #include "borg_driver.h"
    #include "input.h"
#endif


#define I2C_DEVICE "/dev/i2c-1"

int bus;
char drive_ui = 0;

thread_local unsigned int seed = time(NULL);


//void (*TapCallback)(int sx, int sy);

void global_wait(int x, int y) {
    printf("ui tap %dx%d!\n", x, y);
    drive_ui = 1;
}

/**
 * blocks and turns a random direction with no leg or wall
 */
void random_direction() {

    // pick a random direction to turn
    //TODO:del int direction = rand_r(&seed) % 2;
    if (rand_r(&seed) % 2 == 1) {
            Left();
    } else {
            Right();
    }
    // turn for a bit...
    int min_turn_time = 700000 + ((rand_r(&seed) % 5) * 600000);
    usleep(min_turn_time);

    // keep turning until no wall or leg is found...
    int max_wait = 400;
    while(condition == 'W' || condition == 'L' && max_wait > 0) {
        usleep(20000);
        max_wait--;
    }

    // Walls everywhere!
    if (max_wait <= 0) {
        //fb_write_string("")
        sprintf(text_buff, "unable to move");
        sleep(5);
        return;
    }

    // ahh, nothinng in front
    // turn just a bit more to get that out of the way
    usleep(800000);

    // stop and return
    AllStop();
}


/**
 * drive arround randomly and dont bump into things forever
 */
void *random_drive(void *foo) {
    //close(pipe_fd[1]);
    // borg_init();

    sprintf(text_buff, "camera wait");
    while(condition == 'W') {
        usleep(200000);
    }

    // wait for button press
        sprintf(text_buff, (char  *)"press button");
        while(drive_ui == 0) {
            usleep(600000);
        }

    // 3 second count down
    for (int i = 3; i != 0; i--) {
        sprintf(text_buff, (char  *)"Run %d", i);
        usleep(1000000);
    }

    // loop until running loop ends
    while(GCONF->running) {
        for(int i=0; i<1000; i++) {
            usleep(10000);

            if (condition == 'W' || condition == 'L') {
                int j = 20;
                AllStop();
                // wait for the thing to get out of the way...
                while (condition == 'W' || condition == 'L' && j > 0) {
                    //sprintf(text_buff, (char  *)"wait: %d", j);
                    usleep(200000);
                    // TODO: DO WE NEED THIS? AllStop();
                    j--;
                }

                // it didn't get out of theway...
                if (j <= 0) {
                    //sprintf(text_buff, (char  *)"Redirect");
                    random_direction();
                    // we found a new direction to move in, reset walk counter
                    i = 1000;
                }
            }

            sprintf(text_buff, (char *)"Fwd %s", (condition == 'X') ? "fast" : "slow");
            DriveForward(32);
        }

        sprintf(text_buff, (char  *)"Redirect");
        random_direction();
    }

    AllStop();
}








/**
 * run a command and return the result.  ported from python
 */
char* RawRead(int command, int length, char *rbuf) {

    char wbuf[10] = {0};

    wbuf[0] = command;

    if (write(bus, wbuf, 1) != 1) {
        Panic(71, "failed to write to the i2c bus");
    }


    if (read(bus,rbuf,length) != length) {
        Panic(72, "failed to read from the i2c bus");
    }

    return (rbuf);
}

/**
 * read and display the battery power level
 */
void ReadBattery() {
    u_int16_t raw;
    char rbuf[10] = {0};
    printf("reading batt\n");
    RawRead(21, 6, rbuf);
    raw = (rbuf[1] << 8) + rbuf[2];
    printf("batt: [%0x %0x] [%d]\n", rbuf[1], rbuf[2], raw);
}


/**
 * all forward at power level (1-255)
 */
void DriveForward(int power) {
    char wbuf[10] = {0};
    wbuf[0] = 17;
    wbuf[1] = power;
    write(bus, wbuf, 2);
}

/**
 * all backward at power level (1-255)
 */
void DriveBackward(int power) {
    char wbuf[10] = {0};
    wbuf[0] = 18;
    wbuf[1] = power;
    write(bus, wbuf, 2);
}

/**
 * turn right 90 degrees 
 */
void Right90() {
    char wbuf[10] = {0};
    wbuf[0] = 8;
    wbuf[1] = 200;
    write(bus, wbuf, 2);
    wbuf[0] = 12;
    write(bus, wbuf, 2);
    usleep(600000);
}


/**
 * turn left 90 degrees 
 */
void Left90() {
    char wbuf[10] = {0};
    wbuf[0] = 9;
    wbuf[1] = 200;
    write(bus, wbuf, 2);
    wbuf[0] = 11;
    write(bus, wbuf, 2);
    usleep(460000);
}

/**
 * turn left and dont stop
 */
void Left() {
    char wbuf[10] = {0};
    wbuf[0] = 9;
    wbuf[1] = 200;
    write(bus, wbuf, 2);
    wbuf[0] = 11;
    write(bus, wbuf, 2);
}
/**
 * turn right and dont stop
 */
void Right() {
    char wbuf[10] = {0};
    wbuf[0] = 8;
    wbuf[1] = 200;
    write(bus, wbuf, 2);
    wbuf[0] = 12;
    write(bus, wbuf, 2);
}


void AllStop() {
    char wbuf[10] = {0};
    wbuf[0] = 14;
    write(bus, wbuf, 1);
}
 


void Wiggle() {
    char wbuf[10] = {0};
    wbuf[0] = 11;
    wbuf[1] = 100;
    write(bus, wbuf, 2);
    wbuf[0] = 12;
    write(bus, wbuf, 2);
    usleep(1000);

    wbuf[0] = 8;
    write(bus, wbuf, 2);
    wbuf[0] = 9;
    write(bus, wbuf, 2);
    usleep(2000);

    wbuf[0] = 11;
    write(bus, wbuf, 2);
    wbuf[0] = 12;
    write(bus, wbuf, 2);

    usleep(1000);
}



/**
 * all forward at power level (1-255)
 */
void FailSafe() {
    char wbuf[10] = {0};
    wbuf[0] = 19;
    write(bus, wbuf, 1);
}

void BorgInit() {

    const char *filename = I2C_DEVICE;
    if ((bus = open(filename, O_RDWR)) < 0) {
        Panic(73, "Failed to open the i2c bus");
    }


    int addr = 0x15;          // The I2C address of the piborg
    if (ioctl(bus, I2C_SLAVE, addr) < 0) {
        Panic(74, "Failed to acquire the i2c bus");
    }

    // char *data;
    char rbuf[10] = {0};
    RawRead(0x99, 6, rbuf);

    if (rbuf[1] != 0x15) {
        Panic(74, "unable to find the thunderborg");
    }

    FailSafe();
    AllStop();

    add_tap_callback(global_wait);
    //wiringPiSetupGpio() ;
}

