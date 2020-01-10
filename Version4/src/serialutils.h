#ifndef SERIALUTILS_H_INCLUDED
#define SERIALUTILS_H_INCLUDED

// Standard libraries
#include <stdio.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions 	   */
#include <errno.h>   /* ERROR Number Definitions           */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include <wiringPi.h>
#include <libmseed.h>
#include "utils.h"

#define MAX_LINE_DATA 98

static int fd_port = -1;

int inputAvailable(int fn_io);
char* read_serial( long timeout, FILE *fp);
int open_serial_port(void);
int serial_port_settings (void);
int flush_serial(void);
void write_serial(void);
void close_serial(void);
 int SerialPort(void);

#endif // SERIALUTILS_H_INCLUDED
