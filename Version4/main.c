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
#include <wiringPi.h>
#include <wiringSerial.h>
#include <pthread.h>
#include <sys/mount.h>
#include <pigpio.h>
#include <libconfig.h>
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
#define BLOCK_LENGTH_102 102
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
void timestamp_queue_free(void);
void *HandleMSEEDRecord(void *arg);
void FreeDataQueueSamples(void);
int ParseConfigFile(void);
void Configure_Streams();
void *StatusLED_blink(void *arg);

/******* Global Variables **************/

// Data and TimeStamp structs
struct  Q_timestamp *q_timestamp;
struct DataQueue *qDataSample;

// max 30 characters
char SaveFolderUSBE[43];
char SaveFolderUSBN[43];
char SaveFolderUSBZ[43];

// Thread stop variables
int StopHandleMseedRecord = 1;
int StopTimeStamp = 1;
int StopTimeStampCont = 0;

// MSR definitions
MSRecord *msr_NS;
MSRecord *msr_EW;
MSRecord *msr_Z;

// MSR Struct variables
char network[11];
char station[11];
char location[11];
char dataquality;
double samprate;
int8_t encoding; // INT32 encoding //FLOAT32 4
int8_t byteorder; // MSB first
int64_t numsamples = BLOCK_LENGTH_100;
char sampletype; // 32-bit integer, f - float
int reclen;

// digitizer log file
char *LogFile = "digitizer.log";

// usb drive location to store mseed files
char MseedVolume[15];

// flag used to create and store mseed files. 1 = create, 0 = don't create
int Save2MseedFile;
int Save2MseedFile_temp;

// Channel name from the sensor
char ChannelNameEW[3];
char ChannelNameNS[3];
char ChannelNameZ[3];



// usb mount command
char *MountCommand = "mount | grep -q ";
char CheckMount[70];

// log file definition
FILE *fp_log;

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
char StreamIDE[50];
char StreamIDN[50];
char StreamIDZ[50];

// source name for each channel in sensor
char SourceNameE[50];
char SourceNameN[50];
char SourceNameZ[50];

//
int tag = 0;

hptime_t hptime_temp = 0;
/* Main Program */
int main()
{


    // parse configuration file
    if (ParseConfigFile() == -1)
    {
        printf("Error parsing configuration file. Exiting\n");
        return -1;
    }

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

    //////////////////////////////////////////////////////////////////////////
    /* The Ring server is the main storage of MSEED packets.
     * The DataLink (DL) protocol is used to send MSEED packets to the ringserver.
     * As a first entry point into the program, a connection is established to the DL server.
     * If the connection fails, the program exits and is restarted.
     * hostname = localhost, port = 16000
    */

	if (connect2DLServer(&dlconn, fp_log) != 1)
	{
		fprintf(fp_log, "%s: Failed to connect to DL server. Please try again.\n", GetLogTime());
      //  fflush(fp_log);
		fclose(fp_log);
		return -1;
	}

    fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    fprintf(fp_log, "%s: Connected to DataLink server @ localhost:16000!\n", GetLogTime());

    /* Allocate memory for MSRecord Struct */

    if(!(msr_NS = msr_init(msr_NS)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct\n", GetLogTime());
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        fclose(fp_log);
        return -1;
    }
    if(!(msr_EW = msr_init(msr_EW)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct\n", GetLogTime());
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        fclose(fp_log);
        return -1;
    }
    if(!(msr_Z = msr_init(msr_Z)))
    {
        fprintf(fp_log, "%s: Error initializing memory for MSEED struct\n", GetLogTime());
        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

        fclose(fp_log);
        return -1;
    }
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    fprintf(fp_log, "%s: MSR struct allocated\n", GetLogTime());
    fflush(fp_log);

    /* Intialize MSRecord struct */
    // NS
    initialize_MSRecord(&msr_NS, network, station,location, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);
    // EW
    initialize_MSRecord(&msr_EW, network, station,location, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);
    // Z
    initialize_MSRecord(&msr_Z, network, station,location, dataquality,samprate,encoding,byteorder,numsamples,sampletype,reclen);

    fprintf(fp_log, "%s: MSR initalized\n", GetLogTime());
    //MaximumPackets(dlconn, fp_log);
    Configure_Streams();
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));


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

        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));
    if(!(qDataSample = CreateDataQueue(qDataSample, fp_log)))
    {
        fprintf(fp_log, "%s: Could not allocate queue, out of memory? \n", GetLogTime());
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
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    /* Initialize timestamp queue */
    if(InitializeDataQueue(qDataSample, fp_log) == -1)
    {
        fprintf(fp_log, "%s: Error in initializing queue. Try again\n", GetLogTime());
        // de-allocate timestamp queue
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

		fclose(fp_log);
		return -1;
    }
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* Create queue that contains timestamps */

    if (!(q_timestamp = create_queue_timestamp(q_timestamp, fp_log)))
    {
        fprintf(fp_log, "%s: Could not allocate q, out of memory?\n", GetLogTime());
        // Free msr struct
        msr_NS->datasamples = NULL;
        msr_free(&msr_NS);

        msr_EW->datasamples = NULL;
        msr_free(&msr_EW);

        msr_Z->datasamples = NULL;
        msr_free(&msr_Z);
        FreeDataQueueSamples();
        DataQueueFree(&qDataSample);

        // Free DL descriptor and disconnect
        if ( dlconn->link != -1 )
            dl_disconnect (dlconn);

        if ( dlconn )
            dl_freedlcp (dlconn);

		fclose(fp_log);
		return -1;
    }
    fprintf(fp_log, "%s: Timestamp queue created\n", GetLogTime());

        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    /* Initialize timestamp queue */
    if(initialize_Queue_timestamp(q_timestamp, fp_log) == -1)
    {
        fprintf(fp_log, "%s: Error in initializing queue. Try again\n", GetLogTime());
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

		fclose(fp_log);
		return -1;
    }
    fprintf(fp_log, "%s: Timestamp queue initialized!\n", GetLogTime());
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    /* Open serial port */
   int port_rv = open_serial_port();
	if (port_rv == -1)
	{
		fprintf(fp_log, "%s: Error opening port. Please try again\n", GetLogTime());
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

        fclose(fp_log);
		return -1;
	}
	fprintf(fp_log, "%s: Serial port opened successfully!\n", GetLogTime());
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));


	// Serial port settings
	int serial_port_settings_rv = serial_port_settings(); // Will add  option to change baud rate later
	if (serial_port_settings_rv == -1)                           // Currently set at 19200. Will have to go into function
	{                                                            // to manually change it.
		fprintf(fp_log, "%s:  Error in setting port attributes. Please try again\n", GetLogTime());

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

        // close serial port
        close_serial();

		fclose(fp_log);
		return -1;
	}

	fprintf(fp_log, "%s: Serial Port settings set!\n", GetLogTime());
		        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));


	// allowing arduino to upload code..... This happens when serial port is opened
	sleep(3); // find a way for the arduino to tell the pi when finishing uploading code
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

	if (flush_serial() < 0)
	{
		fprintf(fp_log, "%s: Error flushing input/output lines: %s\n",GetLogTime(), strerror(errno));
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

        // close serial port
        close_serial();
        fclose(fp_log);
		return -1;
	}
    fprintf(fp_log, "%s: input/output serial lines flushed!\n", GetLogTime());
    fprintf(fp_log, "\n");
    fprintf(fp_log, "\n");
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    sleep(10);
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    ///////////////////////////////////////////////
    pthread_t CreateMSEEDRecordThreadID;
    int ThreadRV = pthread_create(&CreateMSEEDRecordThreadID, NULL, HandleMSEEDRecord, NULL);
    if(ThreadRV !=0)
    {
        fprintf(fp_log, "%s: Error in creating thread: HandleMSEEDRecord\n", GetLogTime());
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

        // close serial port
        close_serial();
        fclose(fp_log);
        return -1;

    }
    fflush(fp_log);
        fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));



    /* Initiate interrupt function that timestamps */

    fprintf(fp_log, "%s: after set up: %d\n",GetLogTime(), digitalRead(17));
    fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

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

    Digitizer(CheckMount, fp_log, q_timestamp, qDataSample, msr_NS, msr_EW, msr_Z, BLOCK_LENGTH_100,
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


    return 0;

}

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
      //  printf("***************************************\n");

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

void timestamp_queue_free(void)
{
    while(q_timestamp->front != NULL)
    {
    	struct Q_timestamp_node *temp = q_timestamp->front;
        q_timestamp->front = q_timestamp->front->next;
        free(temp);
        if(q_timestamp->front == NULL)
            q_timestamp->rear = NULL;
    }
}

void FreeDataQueueSamples(void)
{
    while(qDataSample->front != NULL)
    {
        struct DataNode *temp = qDataSample->front;
        qDataSample->front = qDataSample->front->next;
        free(temp->DataSample);
        free(temp);
        if(qDataSample->front == NULL)
            qDataSample->rear = NULL;
    }
}

void *HandleMSEEDRecord(void *arg)
{
    for(;;)
    {
        if (StopHandleMseedRecord == 1)
        {

            char *SerialData = read_serial(TIMEOUT, fp_log);

            //if (SerialData != NULL)

            if (SerialData == NULL)
            {
                // Arduino still initializing
                continue;
            }

            if(InsertDataQueue(qDataSample, SerialData, fp_log) == -1)
            {
                fprintf(fp_log, "%s: Could no insert sample in data queue\n", GetLogTime());
                fflush(fp_log);
                continue;
            }
        }
    }
}

int ParseConfigFile(void)
{

    config_t cfg, *cf;
    cf = &cfg;
    config_init(cf);

    if (!config_read_file(cf, "config.cfg"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cf),
                                        config_error_line(cf),
                                        config_error_text(cf));
        config_destroy(cf);
        return -1;
    }
    // parse logfile first
    int logfile_flag;
    if (config_lookup_int(cf, "logfile", &logfile_flag))
    {
        if (logfile_flag == 1)
        {
            fp_log = fopen(LogFile, "w");
            if (fp_log == NULL)
            {
                printf("Cannot create log file\n");
                return -1;
            }
        }
        else
        {
            printf("Please set logfile flag to 1 in configuration file\n");
            return -1;
        }
    }
    else
    {
        printf("logfile not defined\n");
        return -1;
    }
    // msr struct variables
    const char *temp;
    if (config_lookup_string(cf, "msrstruct.network", &temp))
        strcpy(network, temp);
    else
    {
        fprintf(fp_log, "network not defined\n");
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.station", &temp))
        strcpy(station, temp);
    else
    {
        fprintf(fp_log, "station not defined\n");
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.location", &temp))
        strcpy(location,temp);
    else
    {
        fprintf(fp_log, "location not defined\n");
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.dataquality", &temp))
        dataquality = temp[0];//strcpy(dataquality, temp);
    else
    {
        fprintf(fp_log, "dataquality not defined");
        return -1;
    }

    int temp_encoding;
    if (config_lookup_int(cf, "msrstruct.encoding", &temp_encoding))
        encoding = (int8_t)temp_encoding;
    else
    {
        fprintf(fp_log, "encoding not defined\n");
        return -1;
    }

    if (config_lookup_string(cf, "msrstruct.sampletype", &temp))
        sampletype = temp[0];
    else
    {
        fprintf(fp_log, "sampletype not defined\n");
        return -1;
    }

    double temp_samprate;
    if (config_lookup_float(cf, "msrstruct.samprate", &temp_samprate))
        samprate = (double)temp_samprate;
    else
    {
        fprintf(fp_log, "samprate not defined\n");
        return -1;
    }

    int temp_byteorder;
    if (config_lookup_int(cf, "msrstruct.byteorder", &temp_byteorder))
        byteorder = (int8_t)temp_byteorder;
    else
    {
        fprintf(fp_log, "byteorder not defined\n");
        return -1;
    }

    if (config_lookup_int(cf, "msrstruct.reclen", &reclen))
        ;
    else
    {
        fprintf(fp_log, "reclen not defined\n");
        return -1;
    }
    // channel names
    const config_setting_t *channelnames;
    channelnames = config_lookup(cf, "channelname");
    int count = config_setting_length(channelnames);
    for (int i = 0; i < count; i ++)
    {
        const char *temp = config_setting_get_string_elem(channelnames, i);
        if (temp[2] == 'N')
            strcpy(ChannelNameNS, temp);
        else if(temp[2] == 'E')
            strcpy(ChannelNameEW, temp);
        else
            strcpy(ChannelNameZ, temp);
        //strcpy(ChannelNameEW1, config_setting_get_string_elem(channelnames, i));
    }

    //printf("%s %s %s\n", ChannelNameEW, ChannelNameNS, ChannelNameZ);

    // usb save location
    if (config_lookup_string(cf, "usblocation", &temp))
        strcpy(MseedVolume, temp);
    else
    {
        fprintf(fp_log, "usblocation not found\n");
        return -1;
    }

    // save2mseed file flag
    if (config_lookup_int(cf, "save2mseedfile", &Save2MseedFile))
        ;
    else
    {
        fprintf(fp_log, "save2mseedfile not found\n");
        return -1;
    }
    Save2MseedFile_temp = Save2MseedFile;
    config_destroy(cf);
    return 1;
}

void Configure_Streams()
{
    strcpy(msr_NS->channel, ChannelNameNS);
    strcpy(msr_EW->channel, ChannelNameEW);
    strcpy(msr_Z->channel, ChannelNameZ);

    msr_srcname(msr_NS, SourceNameN, 0);
    msr_srcname(msr_EW, SourceNameE, 0);
    msr_srcname(msr_Z, SourceNameZ, 0);

    strcpy(StreamIDE, SourceNameE);
    strcpy(StreamIDN, SourceNameN);
    strcpy(StreamIDZ, SourceNameZ);

    strcat(SourceNameE, "/");
    strcat(SourceNameN, "/");
    strcat(SourceNameZ, "/");

    strcat(StreamIDE, "/MSEED");
    strcat(StreamIDN, "/MSEED");
    strcat(StreamIDZ, "/MSEED");

    SaveFolderE = SourceNameE;
    SaveFolderN = SourceNameN;
    SaveFolderZ = SourceNameZ;


    // set up usb save location

    //SaveFolderUSBE = malloc(sizeof(char) * (strlen(MseedVolume) + strlen(SaveFolderE) + 1));
    strcpy(SaveFolderUSBE, MseedVolume);
    strcat(SaveFolderUSBE, "/");
    strcat(SaveFolderUSBE, SaveFolderE);

    strcpy(SaveFolderUSBN, MseedVolume);
    strcat(SaveFolderUSBN, "/");
    strcat(SaveFolderUSBN, SaveFolderN);

    strcpy(SaveFolderUSBZ, MseedVolume);
    strcat(SaveFolderUSBZ, "/");
    strcat(SaveFolderUSBZ, SaveFolderZ);
}



