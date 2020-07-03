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

struct datetime
{
    int year;
    int month;
    int day;
    int hour;
    int mins;
    int sec;
};
struct msrecord_struct
{
    MSRecord *msr_NS;
    MSRecord *msr_EW;
    MSRecord *msr_Z;

    char stream_id_ew[50];
    char stream_id_ns[50];
    char stream_id_z[50];
};

struct msrecord_struct_members
{
    // MSR Struct variables
    char network[11];
    char station[11];
    char location[11];
    char dataquality;
    int samprate;
    int8_t encoding; // INT32 encoding //FLOAT32 4
    int8_t byteorder; // MSB first
    int64_t numsamples;
    char sampletype; // 32-bit integer, f - float
    int reclen;

    // Channel name from the sensor
    char channel_name_ew[4];
    char channel_name_ns[4];
    char channel_name_z[4];
};
int msrecord_struct_init(struct msrecord_struct *msrecord, FILE *fp_log);
void msrecord_struct_update(struct msrecord_struct *msrecord, struct msrecord_struct_members *msrecord_members);

int process_data(struct msrecord_struct *msrecord, struct msrecord_struct_members *msrecord_members,
                 pthread_cond_t *cond1, pthread_mutex_t *lock_timestamp, struct data_buffer *d_queue,
                 struct timestamp_buffer *ts_queue, FILE *fp_log, DLCP *dlconn, flag save_check);

//void Digitizer(char *CheckMount, FILE *fp_log, struct  Q_timestamp *q_timestamp, struct DataQueue *qDataSample, MSRecord *msr_NS, MSRecord *msr_EW, MSRecord *msr_Z,
  //             int BlockLength, int Save2MseedFile, int Save2MseedFile_temp, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ,
    //           char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], int reclen, DLCP *dlconn, int *tag);

int time_correction(hptime_t starttime, hptime_t *endtime, hptime_t hptime_sample_period,
                    struct msrecord_struct *msrecord);

char *generate_stream_id(MSRecord *msr);

void extract_datetime(hptime_t hptime, struct datetime *dt);

#endif // MSRECORD_H_INCLUDED
