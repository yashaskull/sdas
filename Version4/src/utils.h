#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

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
#include <ezxml.h>
#include <libdali.h>

#define NANO 1000000000

char *GetLogTime(void);
// KeyBoard Interrupt
int kbhit(void);

char *RemoveChannelIdentifier(char *chanData);

// Time-stamping functions
hptime_t current_utc_hptime(void);
void current_utc_time(struct timespec *ts);
hptime_t timespec2hptime(struct timespec* ts);

// ringserver functions
int TotalStreams(DLCP *dlconn, FILE *fp);
int LatestPacketID(char *StreamName, DLCP *dlconn, FILE *fp);
int MaximumPackets(DLCP *dlconn, FILE *fp);
// Datalink Functions
int connect2DLServer(DLCP **dlconn, FILE *fp);

// GPIO functions
int GPIO_setup();

// other stuff

#endif // UTILS_H_INCLUDED
