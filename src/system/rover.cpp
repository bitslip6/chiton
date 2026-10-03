#include <iostream>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>			//Used for UART

#include "utils.hpp"
#include "util.h"
#include "rover.h"
 
namespace rover {

rx_data rx_uart(int uart_fd) {
    //WARN_IF(uart_fd < 3, "rover rx not connected");


    unsigned char rx_buffer[2];
    static rx_data buffer;

    //WARN_IF((read(uart_fd, (void*)rx_buffer, 2) != 2), "unable to read rx_buffer");	//Filestream, buffer to store in, number of bytes to read (max)
    read(uart_fd, (void*)rx_buffer, 2);

    buffer.d1 = rx_buffer[0];
    buffer.d2 = rx_buffer[1];
    buffer.data = rx_buffer[0] << 8 | rx_buffer[1];

    return buffer;
}

void tx_uart(int uart_fd, tx_data data) {
    //WARN_IF(uart_fd < 3, "rover tx not connected");

    unsigned char tx_buffer[7];
    u8 checksum = 255-((data.left_motor + data.right_motor + 0 + data.param_1 + data.param_2) % 255);

    tx_buffer[0] = 253;
    tx_buffer[1] = data.left_motor; 
    tx_buffer[2] = data.right_motor; 
    tx_buffer[3] = 0;
    tx_buffer[4] = data.param_1;
    tx_buffer[5] = data.param_2;
    tx_buffer[6] = checksum;

    int wrote = write(uart_fd, (void *)tx_buffer, sizeof(tx_buffer));


/*
for (int i=0; i<255; i++) {
    tx_buffer[6] = i;
    int wrote = write(uart_fd, (void *)tx_buffer, 7);
    printf("wrote [%d] chksum: [%d]\n", wrote, i);
    sleep(1);
}
*/

}


//int main(int argc, char** argv)
int uart_connect() {
	//-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
	int uart0_filestream = -1;
	
	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	//PANIC_IF(((uart0_filestream = open("/dev/ttyUSB0", O_RDONLY | O_NOCTTY | O_NDELAY)) == -1), "unable to connect to rover", EXIT_FAILURE);
	(uart0_filestream = open("/dev/ttyUSB0", 02));//O_RDWR | O_NOCTTY | O_NDELAY));
	

	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	//options.c_cflag = B57600 | CS8 | CLOCAL | CREAD;		//<Set baud rate
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag |= CREAD | CLOCAL;


    options.c_lflag &= ~ICANON;
    options.c_lflag &= ~ECHO;    // Disable echo
    options.c_lflag &= ~ECHOE;   // Disable erasure
    options.c_lflag &= ~ECHONL;  // Disable new-line echo
    options.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
    options.c_iflag &= ~(IXON | IXOFF | IXANY);  // Turn off s/w flow ctrl
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);  // Disable any special handling of received bytes

    options.c_oflag &= ~OPOST;  // Prevent special interpretation of output bytes
                            // (e.g. newline chars)
    options.c_oflag &= ~ONLCR;  // Prevent conversion of newline to carriage return/line feed

    options.c_cc[VTIME] = 0;  // remove wait time

    cfsetispeed(&options, B57600);
    cfsetospeed(&options, B57600);



	//options.c_iflag = ISIZE;

	//options.c_oflag = 0;
	//options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);

    return uart0_filestream;
}



/*
	//----- CHECK FOR ANY RX BYTES -----
	if (uart0_filestream != -1)
	{
		// Read up to 255 characters from the port if they are there
		unsigned char rx_buffer[256];
		int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)

	    while (rx_length < 255) 
	    {
		if (rx_length < 0)
		{
			//cout << "An error occured (will occur if there are no bytes)" << endl;
		}
		else if (rx_length == 0)
		{
			//cout << "No data waiting" << endl;
		}
		else
		{
			//Bytes received
			rx_buffer[rx_length] = '\0';
			printf("%i bytes read : %s\n", rx_length, rx_buffer);
		}
	    }
	}


	//----- CLOSE THE UART -----
	close(uart0_filestream);
*/
}