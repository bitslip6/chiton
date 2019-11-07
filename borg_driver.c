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

int bus;

char* RawRead(int command, int length, char *rbuf) {

    char wbuf[10] = {0};

    wbuf[0] = command;

    if (write(bus, wbuf, 1) != 1) {
        perror("Failed to write to the i2c bus.\n");
        printf("\n\n");
    }


    if (read(bus,rbuf,length) != length) {
            /* ERROR HANDLING: i2c transaction failed */
            perror("Failed to read from the i2c bus");
            printf("\n\n");
    } else {
	printf("read Data:  [%0x]\n",rbuf);
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


int main(int argc, const char *argv[]) {

    wiringPiSetupGpio() ;
    softPwmCreate (18, 1024, 1024) ;

    char *filename = "/dev/i2c-1";
    if ((bus = open(filename, O_RDWR)) < 0) {
        /* ERROR HANDLING: you can check errno to see what went wrong */
        printf("Failed to open the i2c bus\n");
        exit(1);
    }
    printf("opened i2c bus\n");


    int addr = 0x15;          // The I2C address of the piborg
    if (ioctl(bus, I2C_SLAVE, addr) < 0) {
        printf("Failed to acquire bus access and/or talk to slave errno[%d].\n", errno);
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(2);
    }

    char *data;
    char rbuf[10] = {0};
    RawRead(0x99, 6, rbuf);

    if (rbuf[1] != 0x15) {
	    printf("unable to find Thunderborg, found [%0x]\n", rbuf[1]);
	    exit(1);
    }

    printf("FOUND THUNDER! [%0x %0x]\n", rbuf[0], rbuf[1]);
    //ReadBattery();
    FailSafe();
    AllStop();
    exit(9);
    //Right90();
    //DriveForward(50);
    AllStop();
    sleep(1);
    Left90();
    AllStop();
    exit(0);

    Right90();

    Wiggle();
    printf("setting forward 50%\n");
    DriveForward(100);
    sleep(1);
    printf("right 90\n");
    Right90();
    printf("setting forward 50%\n");
    DriveForward(100);
    sleep(1);
    printf("right 90\n");
    Right90();
    printf("setting forward 50%\n");
    DriveForward(100);
    sleep(1);
    printf("right 90\n");
    Right90();
    printf("setting forward 50%\n");
    DriveForward(100);
    sleep(1);
    Wiggle();

    exit(0);
}


/*
    def RawRead(self, command, length, retryCount = 3): 
        """ 
RawRead(command, length, [retryCount])

Reads data back from the ThunderBorg after sending a GET command
Command codes can be found at the top of ThunderBorg.py, length is the number of bytes to read back

The function checks that the first byte read back matches the requested command
If it does not it will retry the request until retryCount is exhausted (default is 3 times)

Under most circumstances you should use the appropriate function instead of RawRead
        """
        while retryCount > 0:
            self.RawWrite(command, []) 
            rawReply = self.i2cRead.read(length)
            reply = []
            for singleByte in rawReply:
                reply.append(ord(singleByte))
            if command == reply[0]:
                break
            else:
                retryCount -= 1
        if retryCount > 0:
            return reply
        else:
            raise IOError('I2C read for command %d failed' % (command))
            */
