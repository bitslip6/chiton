/***************************************************************************
    email                : cory@bitslip6.com

    $Id: eeprog.c,v 1.28 2004/02/29 11:06:41 tat Exp $
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "24cXX.h"

#define VERSION 	"2.0"

#define ENV_DEV		"EEPROG_DEV"

#define ENV_I2C_ADDR	"EEPROG_I2C_ADDR"

#define PAGE_SIZE 256
#define min(a,b) (a<b)?a:b
#define max(a,b) (a>b)?a:b
#define count(src,c) int occurrences=0;char *pTemp=src;while( (*pTemp = strchr(pTemp, c)) != NULL) { occurrences++;pTemp++; }


#define BLD_TXT "\e[1m"
#define NRML_TXT "\e[0m"
#define GRN_TXT "\e[32m"
#define RED_TXT "\e[31m"
#define BLU_TXT "\e[34m"
#define BLD_G_TXT BLD_TXT GRN_TXT
#define BLD_R_TXT BLD_TXT RED_TXT
#define BLD_B_TXT BLD_TXT BLU_TXT
#define DFLT_TXT "\e[0m\e[39m"
#define BG_S BLD_G_TXT "%s" DFLT_TXT
#define BR_S BLD_R_TXT "%s" DFLT_TXT
#define BB_S BLD_B_TXT "%s" DFLT_TXT
#define BG_X BLD_G_TXT "0x%x" DFLT_TXT

#define EOL_BYTES 18
#define TUPLE_PER_LINE 8
#define BYTES_PER_LINE 48 
#define BYTES_PER_READ 2 
#define WRITE_USEC_DELAY 5000 
#define PATH_TO_MD5SUM "/usr/bin/md5sum" 

int g_quiet;
int g_verbose;
int g_dummy;

#define usage_if(a) do { do_usage_if( a , __LINE__); } while(0);
void do_usage_if(int b, int line)
{
const static char *eeprog_usage = 
"eeprog " VERSION ", a 24Cxx EEPROM reader/writer\n"
"by Stefano Barbato.\n"
"Usage: eeprog [-fqxdh] [-16|-8] [ -r addr[:count] | -w addr ]  /dev/i2c-N  i2c-address\n" 
"\n"
"  Address modes: \n"
"	-8		Use 8bit address mode for 24c0x...24C16 [default]\n"
"	-16		Use 16bit address mode for 24c32...24C256\n"
"  Actions: \n"
"	-r addr[:count]	Read [count] (1 if omitted) bytes from [addr]\n" 
"			and print them to the standard output\n" 
"	-w addr		Write input (stdin) at address [addr] of the EEPROM\n"
"	-h		Print this help\n"
"  Options: \n"
"	-x		Set hex output mode\n" 
"	-d		Dummy mode, display what *would* have been done\n" 
"	-f		Disable warnings and don't ask confirmation\n"
"	-q		Quiet mode\n"
"\n"
"The following environment variables could be set instead of the command\n"
"line arguments:\n"
"	EEPROG_DEV		device name(/dev/i2c-N)\n"
"	EEPROG_I2C_ADDR		i2c-address\n"
"\n"
"	Examples\n"
"	1- read 64 bytes from the EEPROM at address 0x54 on bus 0 starting\n"
"	   at address 123 (decimal)\n"
"		eeprog /dev/i2c-0 0x54 -r 123:64\n"
"	2- prints the hex codes of the first 32 bytes read from bus 1 \n"
"	   at address 0x22\n"
"		eeprog /dev/i2c-1 0x51 -x -r 0x22:0x20\n"
"	3- write the current timestamp at address 0x200 of the EEPROM on \n"
"	   bus 0 at address 0x33 \n"
"		date | eeprog /dev/i2c-0 0x33 -w 0x200\n";

	if(!b)
		return;
	fprintf(stderr, "%s\n[line %d]\n", eeprog_usage, line);
	exit(1);
}




#define die_if(a, msg) do { do_die_if( a , msg, __LINE__); } while(0);
void do_die_if(int b, char* msg, int line)
{
	if(!b)
		return;
	fprintf(stderr, "Error at line %d: "BR_S"\n", line, msg);
	//fprintf(stderr, "	sysmsg: %s\n", strerror(errno));
	exit(1);
}

/**
 * panic with error code
 */
void Panic_If(bool condition, const char *format, ...) {
	if (!condition) { 
		return;
	}
    va_list args;
    va_start(args, format);

    fprintf(stderr, BR_S, "SYSTEM PANIC\n");
	if (errno > 0) {
 	   fprintf(stderr, "Last error was: [%s]\n", strerror(errno));
	}
    vfprintf(stderr, format, args);
	va_end(args);
    exit(-1);
}

#define print_info(args...) do { if(!g_quiet) fprintf(stderr, args); } while(0);
#define print_verbose(args...) do { if(g_verbose) fprintf(stdout, args); } while(0);

void parse_arg(char *arg, int* paddr, int *psize)
{
	char *end;
	*paddr = strtoul(arg, &end, 0);
	if(*end == ':')
		*psize = strtoul(++end, 0, 0);
}

int confirm_action()
{
	fprintf(stderr,DFLT_TXT
	"\n"
	"____________________________WARNING____________________________\n"
	"Please confirm this operation\n"
	"Use "RED_TXT"-f"DFLT_TXT" to disable this warning message\n"
	"\n"
	"Press ENTER to continue or hit CTRL-C to exit\n"
	"\n"
	);
	getchar();
	return 1; 
}

int read_from_eeprom(struct eeprom *e, int addr, int size, int hex)
{
	int ch=0, i=0;
	print_info("reading %d bytes\n", size);
	for(i = 0; i < size; ++i, ++addr) {
		if (!g_dummy) {
			die_if((ch = eeprom_read_byte(e, addr)) < 0, "read error");
		}
		if(hex) {
			if( (i % 16) == 0 ) {
				printf("\n %.4x|  ", addr);
			}
			else if( (i % 8) == 0 ) {
				printf("  ");
			}
			printf("%.2x ", ch);
		} else {
			printf("%c", ch);
		}
	}
	print_info("\nread [%d] bytes\n\n", i);
	fflush(stdout);
	return 0;
}

int write_stdin_to_eeprom(struct eeprom *e, int addr) {
	int c;
	int ctr=0;
	while((c = getchar()) != EOF)
	{
		print_verbose("%x ",  c);
		die_if(eeprom_write_byte(e, addr++, c), "write error");
		if (ctr++ > 16) { ctr=0; print_verbose("\n"); fflush(stdout); }
		usleep(WRITE_USEC_DELAY);
	}
	print_info("\n\n");
	return 0;
}



bool verify_checksum(char *filename) {
	
	char outname[BUFSIZ*2], cmd[BUFSIZ];
	snprintf(outname, BUFSIZ/2, "%s.md5", filename);
	snprintf(cmd, BUFSIZ, "%1024s %1024s > %1024s", PATH_TO_MD5SUM, filename, outname);

	Panic_If(system(cmd), 
		"%s error runing checksum on [%s]\n", PATH_TO_MD5SUM, filename);

	char *sum = calloc(1, 32);

	// open the file
	FILE* in_file = fopen(outname, "r");
	Panic_If((in_file == NULL), "unable to open [%s]\n", outname);
	Panic_If((fread(sum, 32, 1, in_file) != 1), "unable ro read calculated md5 sum: [%s]", outname);
	Panic_If((fclose(in_file) != 0), "unable to close md5 checksum file [%s]", outname);
	
	snprintf(outname, BUFSIZ, "%s.%s", filename, sum);
	print_info("checksum file: [%s]\n", outname);
	in_file = fopen(outname, "r");
	Panic_If((in_file == NULL), "checksum mismatch. expected checksum file [%s]", outname);
	Panic_If((fclose(in_file) != 0), "unable to close md5 checksum file [%s]", outname);

	free (sum);
	return true;
}

__u8* read_hex_file(char *filename, int *datalen) {
	 
	// our working double bytes, we read 2 bytes as a time IE: 0xABCD 
	__u8 byte1, byte2; 
	FILE *in_file;
	// our double byte fscanf buffer
	unsigned short int tmp;
	// current byte offset in file
	int ctr=0, linectr=0, lineoff=0, bytesread=0;
	long sz=0;
	// loop until not working
	bool working = true;
	*datalen=0;

	printf("reading hexdump ["BG_S"]\n", filename);
	// open the file to write
	Panic_If(((in_file = fopen(filename, "r")) == NULL), RED_TXT"unable to open firmware sourcefile"DFLT_TXT);
	
	// determine number of bytes in file and allocate buffer space on the heap
	fseek(in_file, 0L, SEEK_END);
	sz = ftell(in_file);
	fseek(in_file, 0L, SEEK_SET);
	__u8 *data = calloc(sizeof(__u8), sz);


	while (working) {
		int ret = fscanf(in_file, "%hx", &tmp);
		if (ret == 1) { 
			byte1 = (tmp >> 8);
			byte2 = (tmp);

			// skip the fileoffset address on the first lineread
			if (lineoff > 0) {
				data[ctr++]=byte2;
				data[ctr++]=byte1;
				// display the first line
				if (bytesread < BYTES_PER_LINE) {
					print_info("%0x%0x ", byte1, byte2); 
				}
				// increment byte read counter
				bytesread += 5;
			}
			// header lines are 8 bytes long
			else { bytesread += 8; }
			lineoff += 2;
			if (lineoff >= EOL_BYTES) {
				lineoff=0;
			}
		} else if (bytesread == sz) {
			print_info("\nsuccessfully read [%d] bytes from [%s]\n", bytesread, filename);
		} else {
			fprintf(stderr, "\nferror: %d\n", ferror(in_file));
			fprintf(stderr, "ret: %d [%s]\n", ret, strerror(errno));
			fprintf(stderr, "only read [%d] of %ld bytes.  Exiting\n", bytesread, sz);
			fprintf(stderr, "%d/%d [%d]\n", linectr, lineoff, bytesread);
			exit(-1);
		}
		working = (ret == 1);
	}
	printf("\n\n");
	*datalen = ctr;

	return data;
}

int write_hex_to_eeprom(struct eeprom *e, int addr, char *filename) {
	int len=0;
	__u8 *data = read_hex_file(filename, &len);

	// write one byte at a time... (sucks i know, device supports page write, but
	// this would need a refactor...^\/[',']\/^ )
	for (int i=0; i<len; i++) {
		print_verbose((i%2) != 1 ? "%02x" : "%02x ", data[i]);
		if (i%(TUPLE_PER_LINE*BYTES_PER_READ) == 1) { print_verbose("\n"); }

		if (!g_dummy) {
			die_if(eeprom_write_byte(e, addr++, data[i]), "write error");
		}
		usleep(WRITE_USEC_DELAY);
	}
	print_info("EEPROM write "BG_S"\n", "success");
	free(data);
	return 0;
}

int read_from_eeprom_kernel(int bus, int id, char *file_name) {
	assert(PAGE_SIZE > 0);
	assert(bus >= 0 && bus <= 3);
	assert(id >= 0 && id < 80);
	assert(file_name != NULL);

	//int len=0;
	char i2c_path[BUFSIZ];
	char buffer[PAGE_SIZE];

	snprintf(i2c_path, BUFSIZ, "/sys/bus/i2c/devices/%d-00%d/eeprom", bus, id);
	FILE *fd=fopen(i2c_path, "r");
	Panic_If(fd == NULL, "unable to open eeprom [%s]\n", i2c_path);

	FILE *fd2=fopen(file_name, "w+");
 
	while (fread(buffer, PAGE_SIZE, 1, fd) == 1) {
		Panic_If(fwrite(buffer, PAGE_SIZE, 1, fd2) != 1, "unable to write buffer to output file [%s]", file_name);
	}
	Panic_If(feof(fd) != 1, "read returned error, and did not reach the end of the file");

	return 0;
}
	
int write_hex_to_eeprom_kernel(int bus, int id, char *filename) {
	assert(PAGE_SIZE > 0);
	assert(bus >= 0 && bus <= 3);
	assert(id >= 0 && id < 80);

	int len=0;
	__u8 *data = read_hex_file(filename, &len);
	char i2c_path[BUFSIZ];

	snprintf(i2c_path, BUFSIZ, "/sys/bus/i2c/devices/%d-00%d/eeprom", bus, id);
	FILE *fd=fopen(i2c_path, "w");
	Panic_If(fd == NULL, "unable to open eeprom [%s]\n", i2c_path);
	return 0;



	int num_page_writes = len/PAGE_SIZE;
	int num_remaining_bytes = len%PAGE_SIZE;
	int number_pages_written = fwrite(data, PAGE_SIZE, num_page_writes, fd);
	die_if(number_pages_written != num_page_writes, "unable to perform page writes");
	if (num_remaining_bytes > 0) {
		int offset = PAGE_SIZE * num_page_writes;
		int number_tails_written = fwrite(data+offset, num_remaining_bytes, 1, fd);
		die_if(number_tails_written != 1, "unable to write ");
	}

	print_info("EEPROM write "BG_S"\n", "success");
	free(data);
	return 0;
}

void Display_Usage() {
	printf("Usage:\n");
	printf("	-w write data, (default is read)\n");
	printf("	-b <i2c bus address> (0-3)\n");
	printf("	-i <i2c device address> (0-79)\n");
	printf("	-f <file to read> (source data in hexdump format)\n\n");
}

int main(int argc, char **argv) {
	
	int ret=0, bus=0, id=0;
	bool read = true;
	char *file_name = NULL;

	printf("EEPROM Programmer version %s\n", VERSION);
	while((ret = getopt(argc, argv, "rwf:b:i:")) != -1) {

		switch (ret) {
			case 'w':
				read = false;
				break;
			case '3':
				read = true;
				break;
			case 'f':
				file_name = optarg;
				break;
			case 'b':
				bus = atoi(optarg);
				break;
			case 'i':
				id = atoi(optarg);
				break;
			default:
				Display_Usage();
				exit(-1);
		}
	}
	if (bus == 0 || id == 0) { Display_Usage(); exit(-1); }
	printf("%s file [%s] to 0x%d on i2c%d\n", (read) ? "read" : "write", file_name, id, bus);

	if (read == false) {
		write_hex_to_eeprom_kernel(bus, id, file_name);
	} else {
		read_from_eeprom_kernel(bus, id, file_name);
	}
}


int main2(int argc, char** argv)
{
	struct eeprom e;
	int ret, op, i2c_addr, memaddr, size, want_hex, force, sixteen;
	char *device, *arg = 0, *i2c_addr_s, *file_name = NULL;
	struct stat st;
	int eeprom_type = EEPROM_TYPE_16BIT_ADDR;

	op = want_hex = force = sixteen = 0;
	g_quiet = g_verbose = g_dummy = 0;

	/**
	 * PARSE ARGS
	 */ 
	while((ret = getopt(argc, argv, "1:8fr:qvhw:xdm:")) != -1)
	{
		switch(ret)
		{
		case '1':
			usage_if(*optarg != '6' || strlen(optarg) != 1);
			//die_if(eeprom_type, "EEPROM type switch (-8 or -16) used twice");
			eeprom_type = EEPROM_TYPE_16BIT_ADDR;	
			break;
		case 'x':
			want_hex++;
			break;
		case 'd':
			g_dummy++;
			break;
		case '8':
			//die_if(eeprom_type, "EEPROM type switch (-8 or -16) used twice");
			eeprom_type = EEPROM_TYPE_8BIT_ADDR;
			break;
		case 'f':
			force++;
			break;
		case 'q':
			g_quiet++;
			break;
		case 'v':
			g_verbose++;
			break;
		case 'h':
			usage_if(1);
			break;
		case 'm':
        	file_name = optarg;
			break;
		default:
			die_if(op != 0, "Both read and write requested"); 
			arg = optarg;
			op = ret;
		}
	}

	usage_if(op == 0); // no switches 
	// set device and i2c_addr reading from cmdline or env
	device = i2c_addr_s = 0;
	switch(argc - optind) {
		case 0:
			device = getenv(ENV_DEV);
			i2c_addr_s = getenv(ENV_I2C_ADDR);
			break;
		case 1:
			if(stat(argv[optind], &st) != -1)
			{
				device = argv[optind];
				i2c_addr_s = getenv(ENV_I2C_ADDR);
			} else {
				device = getenv(ENV_DEV);
				i2c_addr_s = argv[optind];
			}
			break;
		case 2:
			device = argv[optind++];
			i2c_addr_s = argv[optind];
			break;
		default:
			usage_if(1);
	}
	usage_if(!device || !i2c_addr_s);
	i2c_addr = strtoul(i2c_addr_s, 0, 0);

	/**
	 * display information about what we are about to do
	 */
	die_if(eeprom_type == EEPROM_TYPE_8BIT_ADDR, "this version only supports 16bit eeprom, add -16 switch");
	print_info("eeprog "BLD_TXT"%s"DFLT_TXT", a 24Cxx EEPROM reader/writer\n", VERSION);
	print_info("Based on original work from Stefano Barbato.\n");
	print_info("Bus: ["BG_S"], Address: ["BG_X"], Mode: ["BLD_G_TXT"%dbit"NRML_TXT"]\n", device, i2c_addr, (eeprom_type == EEPROM_TYPE_8BIT_ADDR ? 8 : 16) );
	if (file_name != NULL) {
		Panic_If(!verify_checksum(file_name), "Invalid MD5 checksum");
		print_info("Input hex file with valid checksum: "BG_S"\n", file_name);
	}

	// disable writing if using dummy mode
	if(g_dummy) { 
		print_info(RED_TXT"i2c Disabled, NO OP\n"DFLT_TXT);
	} else {
		die_if(eeprom_open(device, i2c_addr, eeprom_type, &e) < 0, "unable to open i2c device");
	}

	// read and write operations here:
	switch(op) {
		case 'r':
			if(!g_dummy && force == 0) {
				confirm_action();
			}
			size = 1; // default
			parse_arg(arg, &memaddr, &size);
			print_info("  Reading %d bytes from 0x%x\n", size, memaddr);
			read_from_eeprom(&e, memaddr, size, want_hex);
			break;
		case 'w':
			parse_arg(arg, &memaddr, &size);
			if (memaddr != 0) {
				printf(RED_TXT"EEPROM address is != 0 (0x%x), confirm\n"DFLT_TXT, memaddr);
			}
			if(memaddr != 0 || (!g_dummy && force == 0)) {
				confirm_action();
			}
			if (file_name == NULL) {
				print_info("  Writing stdin starting at address 0x%x\n", memaddr);
				write_stdin_to_eeprom(&e, memaddr);
			} else {
				write_hex_to_eeprom(&e, memaddr, file_name);
			}
			break;
		default:
			usage_if(1);
			exit(1);
	}
	eeprom_close(&e);

	return 0;
}


