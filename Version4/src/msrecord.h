#ifndef MSRECORD_H_INCLUDED
#define MSRECORD_H_INCLUDED

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
#include <pthread.h>

#include "utils.h"
#include "queue.h"
#include "serialutils.h"
#include "mseedfilesetup.h"

#define NUM_CHANNELS 3
#define MAX_CHANNEL_LENGTH 10

static flag verbose = 1;
flag overwrite;
//int MSRecord2DLServer(char *record, char streamID[50], hptime_t record_endtime, hptime_t record_starttime, DLCP *dlconn, int reclen );
void initialize_msrecord( MSRecord **msr_temp, char network[11], char station[11], char location[11], char channelname[3], char dataquality,
                          double samprate, int8_t encoding, int8_t byteorder, int64_t numsamples, char sampletype, int reclen);

int process_data(MSRecord *msr_NS, MSRecord *msr_EW, MSRecord *msr_Z, int num_samples, hptime_t *hptime_starttime,pthread_cond_t *cond1,
                 pthread_mutex_t *lock_timestamp, struct data_buffer *d_queue, FILE *fp_log);

//void Digitizer(char *CheckMount, FILE *fp_log, struct  Q_timestamp *q_timestamp, struct DataQueue *qDataSample, MSRecord *msr_NS, MSRecord *msr_EW, MSRecord *msr_Z,
  //             int BlockLength, int Save2MseedFile, int Save2MseedFile_temp, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ,
    //           char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], int reclen, DLCP *dlconn, int *tag);

void *blink_LED(void *arg);

char *generate_stream_id(MSRecord *msr);

#endif // MSRECORD_H_INCLUDED
