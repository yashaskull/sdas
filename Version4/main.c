/* Standard Libraries */
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
#include <wiringPi.h> // -lwiringPi
#include <wiringSerial.h>
#include <pthread.h> // -lpthread
#include <sys/mount.h>
#include <pigpio.h>
#include <libconfig.h> // for creating config files. sudo apt-get install libconfig-dev.. linker: -lconfig
#include <dirent.h>

/* Libraries */
#include <libmseed.h>
#include <ezxml.h>
#include "src/queue.h"
#include "src/utils.h"
#include "src/serialutils.h"
#include "src/mseedfilesetup.h"
#include "src/msrecord.h"
//#include "queue.h"

/* Definitions */
//#define BLOCK_LENGTH_114 114
//#define BLOCK_LENGTH_600 600
//#define BLOCK_LENGTH_330 330
//#define BLOCK_LENGTH_335 335
// #define BLOCK_LENGTH_92 92
#define BLOCK_LENGTH_100 100
#define BLOCK_LENGTH_102 200

#define TIMEOUT 1000000
#define PGA 1
#define MAX_SAVE_NUM 1200
#define MSEED_FILE_SAVE_LOCATION_SIZE 60
//#define VREF 2.048
#define VREF 3.312
#define VFSR VREF/PGA
#define FSR (((long int)1<<23)-1)
#define ENCODING_TYPE 3 // 3 for 32bit int, 4 for 32 float

// Pin definitions
#define STATUS_LED 17
#define TIMESTAMP_PIN 19
/******* Function definitions *********/
int data_counter = 0;
int data_block_counter = 0;

void TimeStamp(void);
void *read_serial_buffer(void *arg);
void free_timestamp_buffer_samples(void);
void free_data_buffer_samples(void);
int parse_config_file(FILE *fp);

/******* Global Variables **************/


// Data and TimeStamp structs
struct  timestamp_buffer *timestamp_queue;

// data buffer is a queue type implementation
struct data_buffer *data_queue;

// max 30 characters
char SaveFolderUSBE[43];
char SaveFolderUSBN[43];
char SaveFolderUSBZ[43];

// Thread stop variables
int stop_read_serial_buffer_thread = 1;
int StopTimeStamp = 1;
int StopTimeStampCont = 0;

// MSR definitions
MSRecord *msr_NS; // North South
MSRecord *msr_EW; // East West
MSRecord *msr_Z; // Z or Vertical

// MSR Struct variables
char network[11];
char station[11];
char location[11];
char dataquality;
int samprate;
int8_t encoding; // INT32 encoding //FLOAT32 4
int8_t byteorder; // MSB first
int64_t numsamples = 200;
char sampletype; // 32-bit integer, f - float
int reclen;

// usb drive location to store mseed files
char mseed_volume[15];

// flag used to create and store mseed files. 1 = create, 0 = don't create
int save_mseed_file;
int save_mseed_file_temp;

// Channel name from the sensor
char channel_name_ew[3];
char channel_name_ns[3];
char channel_name_z[3];


FILE *fp_log;
// usb mount command
char *MountCommand = "mount | grep -q ";
char CheckMount[70];

// log file definition

// datalink connection definition
DLCP *dlconn;

// variable to store Latest packet id for each channel saved to a file
int LatestPktIDSavedE = 0;
int LatestPktIDSavedN = 0;
int LatestPktIDSavedZ = 0;

// folder where mseed files are saved
char *SaveFolderE;
char *SaveFolderN;
char *SaveFolderZ;

// stream id for each channel in sensor
char stream_id_ew[50];
char stream_id_ns[50];
char stream_id_z[50];

// source name for each channel in sensor
char source_name_ew[50];
char Source_name_ns[50];
char source_name_z[50];

//
int tag = 0;


pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;

/////////////////////////
char date_time [27];
pthread_mutex_t lock_timestamp;

hptime_t  hptime_start = 0;
hptime_t *hptime_p = &hptime_start;

////////////////////////
hptime_t hptime_temp = 0;

int counter = 0;


/* Main Program */
int main()
{
   // while (1)
   // {
   //     *hptime_p = current_utc_hptime();
   //     ms_hptime2isotimestr(hptime_start, date_time, 1);
 // /      printf("%s\n", date_time);
  //      sleep(1);
  //  }
  //  return -1;
    fp_log = fopen("digitizer.log", "w");
    if (fp_log == NULL)
    {
        printf("Cannot create log file. Program exiting.\n");
        return -1;
    }
    // parse configuration file
    if (parse_config_file(fp_log) == -1)
    {
        fprintf(fp_log, "%s: Error parsing configuration file. Exiting\n", get_log_time());
        return -1;
    }

/**
    // GPIO setup
    /////////////////
    if (GPIO_setup(fp_log) == -1)
    {
        fprintf(fp_log, "%s: Failed to set up gpio. Please try again\n", GetLogTime());
        fclose(fp_log);
        return -1;
    }
    printf("gpio 17: %d\n",digitalRead(17));
    fprintf(fp_log, "%s: GPIO set up successfully\n", GetLogTime());
    ////////////////
*/


    //////////////////////////////////////////////////////////////////////////
    /* The Ring server is the main storage of MSEED packets.
     * The DataLink (DL) protocol is used to send MSEED packets to the ringserver.
     * As a first entry point into the program, a connection is established to the DL server.
     * If the connection fails, the program exits and is restarted.
     * hostname = localhost, port = 16000
    */

	if (connect_dl_server(&dlconn, fp_log) != 1)
	{
		fprintf(fp_log, "%s: Failed to connect to DL server. Please try again.\n", get_log_time());
      //  fflush(fp_log);
		fclose(fp_log);
		return -1;
	}
    fprintf(fp_log, "%s: Connected to DataLink server @ localhost:16000!\n", get_log_time());

    /* Allocate memory for MSRecord Struct */
    if(!(msr_NS = msr_init(msr_NS)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct (NS component).\n", get_log_time());
        // Free DL descriptor and disconnect
        if (dlconn->link != -1)
            dl_disconnect(dlconn);

        if (dlconn)
            dl_freedlcp(dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }

    if(!(msr_EW = msr_init(msr_EW)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct (EW component).\n", get_log_time());
        // Free DL descriptor and disconnect
        if (dlconn->link != -1)
            dl_disconnect(dlconn);

        if (dlconn)
            dl_freedlcp(dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }
    if(!(msr_Z = msr_init(msr_Z)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct (Z component).\n", get_log_time());
        // Free DL descriptor and disconnect
        if (dlconn->link != -1)
            dl_disconnect(dlconn);

        if (dlconn)
            dl_freedlcp(dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }
    fprintf(fp_log, "%s: Memory spaced allocated for MSR structure.\n", get_log_time());
    fflush(fp_log);

    /* Intialize MSRecord struct */
    // NS
    initialize_msrecord(&msr_NS, network, station,location, channel_name_ns, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);
    // EW
    initialize_msrecord(&msr_EW, network, station,location, channel_name_ew, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);
    // Z
    initialize_msrecord(&msr_Z, network, station,location, channel_name_z, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);

    fprintf(fp_log, "%s: MSR initalized\n", get_log_time());
    //MaximumPackets(dlconn, fp_log);

    char *temp = generate_stream_id(msr_NS);
    strcpy(stream_id_ns, temp);

    temp = generate_stream_id(msr_EW);
    strcpy(stream_id_ew, temp);

    temp = generate_stream_id(msr_Z);
    strcpy(stream_id_z, temp);

/**
    // if save2mseedfile is one, create save folders in usb drive if mounted
    if (Save2MseedFile == 1)
    {
        if (SaveFolderConfig(CheckMount, MountCommand, MseedVolume, SaveFolderUSBE, SaveFolderUSBN, SaveFolderUSBZ, fp_log) == -1)
        {
            fprintf(fp_log, "%s: Error setting up save folders. Restart program.\n", GetLogTime());
            msr_NS->datasamples = NULL;
            msr_free(&msr_NS);

            msr_EW->datasamples = NULL;
            msr_free(&msr_EW);

            msr_Z->datasamples = NULL;
            msr_free(&msr_Z);

            if ( dlconn->link != -1 )
                dl_disconnect (dlconn);

            if ( dlconn )
                dl_freedlcp (dlconn);

            fclose(fp_log);
            return -1;
        }
    }
*/

    // data buffer is a queue type implementation
    // Queue to store samples received from Arduino over serial
    // Create and initialize
    if(!(data_queue = create_data_buffer(data_queue, fp_log)))
    {
        fprintf(fp_log, "%s: Could not allocate data buffer, out of memory? \n", get_log_time());

        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);

        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }
    /* Initialize timestamp queue */
    if(initialize_data_buffer(data_queue, fp_log) == -1)
    {
        fprintf(fp_log, "%s: Error in initializing data buffer. Try again.\n", get_log_time());
        // de-allocate timestamp queue
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
    }

    fprintf(fp_log, "%s: Data buffer created and initialized\n", get_log_time());

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* Create queue that contains timestamps */

    if (!(timestamp_queue = create_timestamp_buffer(timestamp_queue, fp_log)))
    {
        fprintf(fp_log, "%s: Could not allocate timestamp queue, out of memory?\n", get_log_time());

        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);

        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
    }
    fprintf(fp_log, "%s: Timestamp queue created\n", get_log_time());

    /* Initialize timestamp queue */
    if(initialize_timestamp_buffer(timestamp_queue, fp_log) == -1)
    {

        fprintf(fp_log, "%s: Error in initializing queue. Try again\n", get_log_time());
        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        free_timestamp_buffer(&timestamp_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
    }
    fprintf(fp_log, "%s: Timestamp queue initialized!\n", get_log_time());
    fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    /* Open serial port */
    if (open_serial_port() == -1)
	{
		fprintf(fp_log, "%s: Error opening port. Please try again\n", get_log_time());

        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
	}

	fprintf(fp_log, "%s: Serial port opened successfully!\n", get_log_time());

	// Serial port settings.. 115200 baud
	if (serial_port_settings() == -1)                           // Currently set at 19200. Will have to go into function
	{                                                            // to manually change it.
        fprintf(fp_log, "%s:  Error in setting port attributes. Please try again\n", get_log_time());

        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        // close serial port
        close_serial();

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
	}
    fprintf(fp_log, "%s: Serial port opened and settings configured!.\n", get_log_time());
	//fprintf(fp_log, "%s: Serial Port settings set!\n", GetLogTime());

	// allowing arduino to upload code..... This happens when serial port is opened
	sleep(3); // find a way for the arduino to tell the pi when finishing uploading code


	if (flush_serial() < 0)
	{
		fprintf(fp_log, "%s: Error flushing input/output lines: %s\n",get_log_time(), strerror(errno));
        //timestamp_queue_free();
        // de-allocate timestamp queue
        //queue_timestamp_free(&q_timestamp);
        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        // close serial port
        close_serial();

        if (fp_log != NULL)
            fclose(fp_log);

		return -1;
	}
    fprintf(fp_log, "%s: input/output serial lines flushed!\n", get_log_time());
    //fprintf(fp_log, "\n");
    //fprintf(fp_log, "\n");

    sleep(1);
    ///////////////////////////////////////////////

    // REMEMBER TO PTHREAD_MUTEX_DESTROY WHEN EXITING
    if (pthread_mutex_init(&lock_timestamp, NULL) != 0)
    {
        printf("mutex init has failed\n");
        return -1;
    }

    pthread_t read_serial_buffer_thread_ID;
    if (pthread_create(&read_serial_buffer_thread_ID, NULL, read_serial_buffer, NULL) != 0)
    {
        fprintf(fp_log, "%s: Error in creating thread: HandleMSEEDRecord\n", get_log_time());
        // timestamp_queue_free();
        // de-allocate timestamp queue
        //queue_timestamp_free(&q_timestamp);

        free_data_buffer_samples();
        free_data_buffer(&data_queue);

        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);

        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        // close serial port
        close_serial();

        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }
    //fflush(fp_log);
    fprintf(fp_log, "%s: Thread for reading serial created.!\n", get_log_time());

    //printf("Waiting on condition variable cond1\n");
    //write_serial();
    //pthread_cond_wait(&cond1, &lock_timestamp);

    process_data(msr_NS, msr_EW, msr_Z, numsamples, &hptime_start, &cond1, &lock_timestamp, data_queue, timestamp_queue, fp_log);
   // write_serial();
   // while(!kbhit())
   // {

   // }
    /* Initiate interrupt function that timestamps */

/**
    sleep(1);

	if (wiringPiISR(17, INT_EDGE_BOTH, &TimeStamp) < 0)
	{
        fprintf(fp_log, "%s: Error initializing interrupt function.", GetLogTime());
		timestamp_queue_free();
        // de-allocate timestamp queue
        queue_timestamp_free(&q_timestamp);
        FreeDataQueueSamples();
        DataQueueFree(&qDataSample);

		// Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        close_serial();
        fclose(fp_log);
		return -1;
	}

    fprintf(fp_log, "%s: after interrupt setup: %d\n",GetLogTime(), digitalRead(17));

	fprintf(fp_log, "%s: WiringPi set up successfully!\n", GetLogTime());
    /* Flush input and output lines of the serial */

  /**  Digitizer(CheckMount, fp_log, q_timestamp, qDataSample, msr_NS, msr_EW, msr_Z, BLOCK_LENGTH_100,
             Save2MseedFile, Save2MseedFile_temp, SaveFolderUSBE, SaveFolderUSBN, SaveFolderUSBZ,
              StreamIDE, StreamIDN, StreamIDZ, reclen, dlconn, &tag);

    fprintf(fp_log, "%s: CreateMSRecord returning\n", GetLogTime());

    StopHandleMseedRecord = 0;

    //digitalWrite(17, LOW);

    // comment out if not time stamping every sec. if timestamping every sec then leave
    //StopTimeStamp = 0;
    //while(StopTimeStampCont != 1)
        //continue;

    // Program shutdown
    // FREE all allocated memory.
    timestamp_queue_free();
    queue_timestamp_free(&q_timestamp);
    FreeDataQueueSamples();
    DataQueueFree(&qDataSample);

    msr_NS->datasamples = NULL;

    msr_free(&msr_NS);

    msr_EW->datasamples = NULL;
    msr_free(&msr_EW);

    msr_Z->datasamples = NULL;
    msr_free(&msr_Z);

    flush_serial();
    close_serial();
    if ( dlconn->link != -1 )
        dl_disconnect (dlconn);

    if ( dlconn )
        dl_freedlcp (dlconn);

    pthread_detach(CreateMSEEDRecordThreadID);
    fprintf(fp_log, "%s: Program exiting.\n", GetLogTime());


    if (fp_log != NULL)
        fclose(fp_log);

*/
    stop_read_serial_buffer_thread = 0;
    sleep(1);
    pthread_mutex_destroy(&lock_timestamp);

    pthread_detach(read_serial_buffer_thread_ID);

    free_data_buffer_samples();
    free_data_buffer(&data_queue);

    free_timestamp_buffer_samples();
    free_timestamp_buffer(&timestamp_queue);

// Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);

        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        // close serial port
        close_serial();

        if (fp_log != NULL)
            fclose(fp_log);

    printf("Hello world\n");
    return 0;

}

/**
void TimeStamp(void)
{
    //printf("in trig ts: %d\n",digitalRead(17));
    if(StopTimeStamp == 1)
    {
        // if fail to get timestamp, use previous timestamp
        char date_time [27];
        static volatile hptime_t hptime=0;
        static volatile hptime_t *phptime = &hptime;
        *phptime = current_utc_hptime();

       // hptime_t hptime_diff = hptime - global_hptime;
       // global_hptime = hptime;
        //ms_hptime2isotimestr(hptime, date_time,1);
        //printf("%s\n", date_time);
       // ms_hptime2isotimestr(hptime, date_time,1);
       // printf("starttime: %s\n", date_time);
       // fprintf(fp_log, "%s: interrupt: %s\n", GetLogTime(), date_time);
       // printf("interrupt: %s\n",date_time);
       // printf("%s\n", GetLogTime());
        //ms_hptime2isotimestr(hptime - hptime_temp, date_time,1);
       // printf("TS starttime: %s\n", date_time);
       //hptime_temp = hptime;
      //  printf("***************************************");

        ms_hptime2isotimestr(hptime, date_time,1);
        fprintf(fp_log, "%s: Triggered timestamp: %s\n", GetLogTime(), date_time);

        //printf("%s\n", date_time);
        int insert_timestamp_queue_rv = insert_timestamp_queue(q_timestamp, hptime, fp_log);
        if(insert_timestamp_queue_rv == -1)
        {
            // What to do if fail to be placed inside of queue ?
            fprintf(fp_log, "%s: Error inserting timestamp into queue\n", GetLogTime());
            fflush(fp_log);
        }
    }else
    {
        // flag main program to continue
        //printf("stopping my interrupt thread\n");
        StopTimeStampCont = 1;
    }

}
*/

void free_timestamp_buffer_samples(void)
{
    while(timestamp_queue->front_p != NULL)
    {
    	struct timestamp_buffer_node *timestamp_buffer_node_temp = timestamp_queue->front_p;
        timestamp_queue->front_p = timestamp_queue->front_p->next_p;
        //printf("%d\n", timestamp_buffer_node_temp->ms_record_starttime);
        free(timestamp_buffer_node_temp);
        if(timestamp_queue->front_p == NULL)
            timestamp_queue->rear_p = NULL;
    }
}


void free_data_buffer_samples(void)
{
    //printf("here\n");
    while(data_queue->front_p != NULL)
    {
        struct data_buffer_node *data_buffer_node_temp = data_queue->front_p;
        data_queue->front_p = data_queue->front_p->next_p;
        //if (data_buffer_node_temp->sample != NULL)
            //printf("%s\n", data_buffer_node_temp->sample);
        free(data_buffer_node_temp->sample);
        free(data_buffer_node_temp);
        if(data_queue->front_p == NULL)
            data_queue->rear_p = NULL;
    }
}


void *read_serial_buffer(void *arg)
{
    while(stop_read_serial_buffer_thread == 1)
    {
        //if (stop_read_serial_buffer_thread == 1)
        //{
            char *serial_data = read_serial(TIMEOUT, fp_log);
            if (serial_data == NULL)
            {
                // Arduino still initializing
                continue;
            }
            //printf("%s\n", serial_data);
            //printf("%s\n", serial_data);
            if (strcmp(serial_data, "*") == 0)
            {
                //pthread_mutex_lock(&lock_timestamp);
                // time stored in global variable hptime_start
                *hptime_p = current_utc_hptime();// time for mseed records
                //printf("%lld\n", hptime_start);
                //ms_hptime2isotimestr(hptime_start, date_time, 1);
                //hptime_temp = hptime_start;
                //printf("%s\n", date_time);
                int insert_timestamp_queue_rv = insert_timestamp_queue(timestamp_queue, hptime_start, fp_log);
                if(insert_timestamp_queue_rv == -1)
                {
                    // What to do if fail to be placed inside of queue ?
                    fprintf(fp_log, "%s: Error inserting timestamp into queue\n", get_log_time());
                    fflush(fp_log);
                    exit(0);
                }
                pthread_cond_signal(&cond1);
                //pthread_mutex_unlock(&lock_timestamp);
            }
            //printf("%s\n", serial_data);
            else
            {
                if (insert_data_buffer(data_queue, serial_data, fp_log) == -1)
                {
                    fprintf(fp_log, "%s: Failed to allocate memory to store sample in buffer. Potential memory issue. Fix and reboot program.\n", get_log_time());
                    //fflush(fp_log);
                    //printf("failed to insert into buffer\n");
                    exit(0); // hard kill program. Need something better.
                }
            }

        //}
    }
    return NULL;
}


int parse_config_file(FILE *fp)
{
    config_t cfg, *cf;
    cf = &cfg;
    config_init(cf);

    if (!config_read_file(cf, "config.cfg"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cf),
                                        config_error_line(cf),
                                        config_error_text(cf));


        fprintf(fp_log, "%s: %s:%d - %s\n", get_log_time(), config_error_file(cf),
                                                          config_error_line(cf),
                                                          config_error_text(cf));

        config_destroy(cf);
        return -1;
    }
    // msr struct variables
    const char *temp;
    if (config_lookup_string(cf, "msrstruct.network", &temp))
        strcpy(network, temp);
    else
    {
        fprintf(fp_log, "%s: network not defined.\n", get_log_time());
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.station", &temp))
        strcpy(station, temp);
    else
    {
        fprintf(fp_log, "%s: station not defined.\n", get_log_time());
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.location", &temp))
        strcpy(location,temp);
    else
    {
        fprintf(fp_log, "%s: location not defined.\n", get_log_time());
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.dataquality", &temp))
        dataquality = temp[0];//strcpy(dataquality, temp);
    else
    {
        fprintf(fp_log, "%s: dataquality not defined.", get_log_time());
        return -1;
    }

    int temp_encoding;
    if (config_lookup_int(cf, "msrstruct.encoding", &temp_encoding))
        encoding = (int8_t)temp_encoding;
    else
    {
        fprintf(fp_log, "%s: encoding not defined\n", get_log_time());
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.sampletype", &temp))
        sampletype = temp[0];
    else
    {
        fprintf(fp_log, "%s: sampletype not defined\n", get_log_time());
        return -1;
    }

    double temp_samprate;
    if (config_lookup_float(cf, "msrstruct.samprate", &temp_samprate))
        samprate = (double)temp_samprate;
    else
    {
        fprintf(fp_log, "%s: samprate not defined\n", get_log_time());
        return -1;
    }

    int temp_byteorder;
    if (config_lookup_int(cf, "msrstruct.byteorder", &temp_byteorder))
        byteorder = (int8_t)temp_byteorder;
    else
    {
        fprintf(fp_log, "%s: byteorder not defined\n", get_log_time());
        return -1;
    }

    if (config_lookup_int(cf, "msrstruct.reclen", &reclen))
        ;
    else
    {
        fprintf(fp_log, "%s: reclen not defined\n", get_log_time());
        return -1;
    }
    // channel names
    const config_setting_t *channel_names;
    channel_names = config_lookup(cf, "channelname");
    int count = config_setting_length(channel_names);
    if (count < 3)
    {
        fprintf(fp_log, "%s: Less than three channel names specified in configuration file. For single channel operation, adjust code.\n", get_log_time());
        return -1;
    }
    for (int i = 0; i < count; i ++)
    {
        const char *temp = config_setting_get_string_elem(channel_names, i);
        if (temp[2] == 'N')
            strcpy(channel_name_ns, temp);
        else if(temp[2] == 'E')
            strcpy(channel_name_ew, temp);
        else
            strcpy(channel_name_z, temp);
        //strcpy(ChannelNameEW1, config_setting_get_string_elem(channelnames, i));
    }

    //printf("%s %s %s\n", ChannelNameEW, ChannelNameNS, ChannelNameZ);

    // usb save location
    if (config_lookup_string(cf, "usblocation", &temp))
        strcpy(mseed_volume, temp);
    else
    {
        fprintf(fp_log, "%s: usblocation not found\n", get_log_time());
        return -1;
    }

    // save2mseed file flag
    if (config_lookup_int(cf, "save2mseedfile", &save_mseed_file))
        ;
    else
    {
        fprintf(fp_log, "%s: save2mseedfile not found\n", get_log_time());
        return -1;
    }
    save_mseed_file_temp = save_mseed_file;
    config_destroy(cf);
    return 1;
}

