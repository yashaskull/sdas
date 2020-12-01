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
#include "queue.h"
#include "utils.h"
#include "serialutils.h"
#include "mseedfilesetup.h"
#include "msrecord.h"
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

void TimeStamp(void);
void *read_serial_buffer(void *arg);
void free_timestamp_buffer_samples(void);
void free_data_buffer_samples(void);
int parse_config_file();
void msrecord_free();
void dlconn_free();


/******* Global Variables **************/

// Data and TimeStamp structs
struct  timestamp_buffer *timestamp_queue;

// data buffer is a queue type implementation
struct data_buffer *data_queue;

// Thread stop variables
int stop_read_serial_buffer_thread = 1;
int StopTimeStamp = 1;
int StopTimeStampCont = 0;

// sdas log file
FILE *fp_log;

// usb mount command
char *mount_command = "mount | grep -q ";
char check_mount[70];

// datalink connection definition
DLCP *dlconn;

// thread stuff
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_timestamp;

// date time variable
char date_time [27];

// hptime variables
hptime_t  hptime_start = 0;
hptime_t *hptime_p = &hptime_start;
hptime_t hptime_temp = 0;

// struct containing variables associated with mseed record members
struct msrecord_struct msrecord;
struct msrecord_members_struct msrecord_members;

// struct containing variables assoiated with saving data to mseed files
struct save2mseedfile_struct save_2_mseed_file;

// struct associated with serial port settings
struct serial_port_settings_struct serial_port_settings;

/* Main Program */
int main()
{
    // open log file
    fp_log = fopen("digitizer.log", "w");
    if (fp_log == NULL)
    {
        printf("Cannot create log file. Program exiting.\n");
        return -1;
    }

    // parse configuration file
    if (parse_config_file() == -1)
    {
        fprintf(fp_log, "%s: Error parsing configuration file. Exiting\n", get_log_time());
        fclose(fp_log);

        return -1;
    }

    // by default, number of samples = sample rate
    // this is our block size
    msrecord_members.numsamples = msrecord_members.samprate; //200
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

    // mseed record structure initialization
    if (msrecord_struct_init(&msrecord, fp_log) == -1)
    {
        fprintf(fp_log, "%s: One or more ms record structures failed to initialize.\n", get_log_time());
        fclose(fp_log);
        dlconn_free();

        return -1;
    }
    fprintf(fp_log, "%s: Memory spaced allocated for MSR structure.\n", get_log_time());
    fflush(fp_log);

    /* Update MSRecord struct */
    msrecord_struct_update(&msrecord, &msrecord_members);
    fprintf(fp_log, "%s: MSR initalized\n", get_log_time());
    //MaximumPackets(dlconn, fp_log);

    char *temp = generate_stream_id(msrecord.msr_NS);
    strcpy(msrecord.stream_id_ns, temp);

    temp = generate_stream_id(msrecord.msr_EW);
    strcpy(msrecord.stream_id_ew, temp);

    temp = generate_stream_id(msrecord.msr_Z);
    strcpy(msrecord.stream_id_z, temp);

    // create data buffer
    if(!(data_queue = create_data_buffer(data_queue, fp_log)))
    {
        fprintf(fp_log, "%s: Could not allocate data buffer, out of memory? \n", get_log_time());
        msrecord_free();
        dlconn_free();
        fclose(fp_log);

        return -1;
    }
    // initialize data buffer
    if(initialize_data_buffer(data_queue, fp_log) == -1)
    {
        fprintf(fp_log, "%s: Error in initializing data buffer. Try again.\n", get_log_time());
        // de-allocate timestamp queue
        free_data_buffer(&data_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
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
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
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
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
        fclose(fp_log);

        return -1;
    }
    fprintf(fp_log, "%s: Timestamp queue initialized!\n", get_log_time());
    fprintf(fp_log,"gpio 17: %d\n",digitalRead(17));

    /* Open serial port */
    if (open_serial_port(serial_port_settings.serial_port) == -1)
    {
        fprintf(fp_log, "%s: Error opening port. Please try again\n", get_log_time());
        free_data_buffer_samples();
        free_data_buffer(&data_queue);
        free_timestamp_buffer_samples();
        free_timestamp_buffer(&timestamp_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
        fclose(fp_log);

        return -1;
    }

    fprintf(fp_log, "%s: Serial port opened successfully!\n", get_log_time());

    // Serial port settings.. 115200 baud
    if (set_serial_port_settings(serial_port_settings.baudrate) == -1)
    {
        fprintf(fp_log, "%s:  Error in setting port attributes. Please try again\n", get_log_time());
        free_data_buffer_samples();
        free_data_buffer(&data_queue);
        free_timestamp_buffer_samples();
        free_timestamp_buffer(&timestamp_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
        // close serial port
        close_serial();
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
        free_timestamp_buffer_samples();
        free_timestamp_buffer(&timestamp_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
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
        fprintf(fp_log, "%s: mutex init has failed\n", get_log_time());
        free_data_buffer_samples();
        free_data_buffer(&data_queue);
        free_timestamp_buffer_samples();
        free_timestamp_buffer(&timestamp_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
        // close serial port
        close_serial();
        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }

    pthread_t read_serial_buffer_thread_ID;
    if (pthread_create(&read_serial_buffer_thread_ID, NULL, read_serial_buffer, NULL) != 0)
    {
        fprintf(fp_log, "%s: Error in creating thread: HandleMSEEDRecord\n", get_log_time());
        free_data_buffer_samples();
        free_data_buffer(&data_queue);
        free_timestamp_buffer_samples();
        free_timestamp_buffer(&timestamp_queue);
        // Free msr struct
        msrecord_free();
        // Free DL descriptor and disconnect
        dlconn_free();
        // close serial port
        close_serial();
        if (fp_log != NULL)
            fclose(fp_log);

        return -1;
    }
    //fflush(fp_log);
    fprintf(fp_log, "%s: Thread for reading serial created.!\n", get_log_time());
    fflush(fp_log);
    //printf("Waiting on condition variable cond1\n");
    //write_serial();
    //pthread_cond_wait(&cond1, &lock_timestamp);
    //int numsaples = 200;

    // add a check if save location is mounted
    // if not, set save check to 0

    strcpy(check_mount, mount_command);
    strcat(check_mount, save_2_mseed_file.save_dir);
    // drive not mounted
    if (save_2_mseed_file.save_check == 1 && system(check_mount) != 0)
    {
        fprintf(fp_log, "%s: Location for saving mseed files at %s is not mounted. Data will only be stored in ring buffer. Please check if save location is mounted correctly.\n", get_log_time(), save_2_mseed_file.save_dir);
        save_2_mseed_file.save_check = 0;
    }

    if (save_2_mseed_file.save_check)
        fprintf(fp_log, "%s: Saving data to mseed files option selected. Data will be stored at: %s\n", get_log_time(), save_2_mseed_file.save_dir);

    //write_serial();
    //int da;
    //scanf("%d\n", &da);
    // beging acuqisition system
    process_data(&msrecord, &msrecord_members, &cond1, &lock_timestamp, data_queue, timestamp_queue, fp_log, dlconn,
                 &save_2_mseed_file);

    // returned after process data finishes
    stop_read_serial_buffer_thread = 0;
    sleep(1);
    pthread_mutex_destroy(&lock_timestamp);

    pthread_detach(read_serial_buffer_thread_ID);

    free_data_buffer_samples();
    free_data_buffer(&data_queue);

    free_timestamp_buffer_samples();
    free_timestamp_buffer(&timestamp_queue);

    // Free msr struct
    msrecord_free();

    // Free DL descriptor and disconnect
    dlconn_free();

    // close serial port
    close_serial();

    fprintf(fp_log, "%s: Program Exiting", get_log_time());
    fclose(fp_log);
    printf("Program end.\n");

    return 0;
}

void free_timestamp_buffer_samples(void)
{
    while(timestamp_queue->front_p != NULL)
    {
        struct timestamp_buffer_node *timestamp_buffer_node_temp = timestamp_queue->front_p;
        timestamp_queue->front_p = timestamp_queue->front_p->next_p;
        ms_hptime2isotimestr(timestamp_buffer_node_temp->ms_record_starttime, date_time, 1);
        printf("%s\n", date_time);
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
        if (data_buffer_node_temp->sample != NULL)
            printf("%s\n", data_buffer_node_temp->sample);
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
            ms_hptime2isotimestr(hptime_start-hptime_temp, date_time, 1);
            //hptime_temp = hptime_start;
            printf("%s\n", date_time);
            int insert_timestamp_queue_rv = insert_timestamp_queue(timestamp_queue, hptime_start, fp_log);
            if(insert_timestamp_queue_rv == -1)
            {
                // What to do if fail to be placed inside of queue ?
                fprintf(fp_log, "%s: Error inserting timestamp into queue\n", get_log_time());
                fflush(fp_log);
                exit(0);
            }
            //pthread_cond_signal(&cond1);
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


/***********************************************************************
 * parse_config_file:
 *
 * Function for parsing SDAS configuration file.
 *
 * Configuration file contains information on creating mseed records and
 * parameters associated with saving data to mseed files.
 *
 * Data is parsed into appropriate variables.
 *
 * Return -1 on error
************************************************************************/
int parse_config_file()
{
    config_t cfg, *cf;
    cf = &cfg;
    config_init(cf);

    // read config file
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
    const char *temp;

    /*
     * Read parameters associated with saving data to mseed files
     */

    // save command
    if (config_lookup_string(cf, "save2mseedparams.savecommand", &temp))
        strcpy(save_2_mseed_file.slarchive_command, temp);
    else
    {
        fprintf(fp_log, "%s: save command is not defined.\n", get_log_time());
        return -1;
    }

    // save option
    if (config_lookup_string(cf, "save2mseedparams.saveoption", &temp))
        strcpy(save_2_mseed_file.save_option, temp);
    else
    {
        fprintf(fp_log, "%s: save option is not defined.\n", get_log_time());
        return -1;
    }

    // seedlink host and port
    if (config_lookup_string(cf, "save2mseedparams.slhostport", &temp))
        strcpy(save_2_mseed_file.sl_port_host, temp);
    else
    {
        fprintf(fp_log, "%s: seedlink host and port is not defined.\n", get_log_time());
        return -1;
    }

    // data save location
    if (config_lookup_string(cf, "save2mseedparams.datasavelocation", &temp))
        strcpy(save_2_mseed_file.save_dir, temp);
    else
    {
        fprintf(fp_log, "%s: save directory is not defined.\n", get_log_time());
        return -1;
    }

    // save 2 mseed file flag (or save check)
    int temp_save_check;
    if (config_lookup_int(cf, "save2mseedparams.savecheck", &temp_save_check))
        save_2_mseed_file.save_check = (int)temp_save_check;
    else
    {
        fprintf(fp_log, "%s: save check is not defined.\n", get_log_time());
        return -1;
    }

    /*
     * MSR structure variables
     */

    // network
    if (config_lookup_string(cf, "msrstruct.network", &temp))
        strcpy(msrecord_members.network, temp);
    else
    {
        fprintf(fp_log, "%s: network not defined.\n", get_log_time());
        return -1;
    }

    // station
    if (config_lookup_string(cf, "msrstruct.station", &temp))
        strcpy(msrecord_members.station, temp);
    else
    {
        fprintf(fp_log, "%s: station not defined.\n", get_log_time());
        return -1;
    }

    // location
    if (config_lookup_string(cf, "msrstruct.location", &temp))
        strcpy(msrecord_members.location,temp);
    else
    {
        fprintf(fp_log, "%s: location not defined.\n", get_log_time());
        return -1;
    }

    // dataquality
    if (config_lookup_string(cf, "msrstruct.dataquality", &temp))
        msrecord_members.dataquality = temp[0];//strcpy(dataquality, temp);
    else
    {
        fprintf(fp_log, "%s: dataquality not defined.", get_log_time());
        return -1;
    }

    // encoding
    int temp_encoding;
    if (config_lookup_int(cf, "msrstruct.encoding", &temp_encoding))
        msrecord_members.encoding = (int8_t)temp_encoding;
    else
    {
        fprintf(fp_log, "%s: encoding not defined\n", get_log_time());
        return -1;
    }

    // sample type
    if (config_lookup_string(cf, "msrstruct.sampletype", &temp))
        msrecord_members.sampletype = temp[0];
    else
    {
        fprintf(fp_log, "%s: sampletype not defined\n", get_log_time());
        return -1;
    }

    // sample rate
    double temp_samprate;
    if (config_lookup_float(cf, "msrstruct.samprate", &temp_samprate))
        msrecord_members.samprate = (double)temp_samprate;
    else
    {
        fprintf(fp_log, "%s: samprate not defined\n", get_log_time());
        return -1;
    }

    // byte order
    int temp_byteorder;
    if (config_lookup_int(cf, "msrstruct.byteorder", &temp_byteorder))
        msrecord_members.byteorder = (int8_t)temp_byteorder;
    else
    {
        fprintf(fp_log, "%s: byteorder not defined\n", get_log_time());
        return -1;
    }

    // record length
    int temp_reclen;
    if (config_lookup_int(cf, "msrstruct.reclen", &temp_reclen))
        msrecord_members.reclen = (int)temp_reclen;
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

    //char ns_temp[3];
    //char ew_temp[3];
    //char z_temp[3];
    for (int i = 0; i < count; i ++)
    {
        const char *temp = config_setting_get_string_elem(channel_names, i);
        if (temp[2] == 'N')
        {
            strcpy(msrecord_members.channel_name_ns, temp);
            //strcpy(ns_temp, temp);
        }
        else if(temp[2] == 'E')
        {
            strcpy(msrecord_members.channel_name_ew, temp);
            //strcpy(ew_temp, temp);
        }
        else
        {
            //strcpy(z_temp, temp);
            strcpy(msrecord_members.channel_name_z, temp);
        }

        //strcpy(ChannelNameEW1, config_setting_get_string_elem(channelnames, i));
    }

    // read serial port parameters
    // baudrate
    int temp_baudrate;
    if (config_lookup_int(cf, "baudrate", &temp_baudrate))
        serial_port_settings.baudrate = (int)temp_baudrate;
    else
    {
        fprintf(fp_log, "%s: baudrate not defined\n", get_log_time());
        return -1;
    }

    // serial port
    if (config_lookup_string(cf, "serialport", &temp))
        strcpy(serial_port_settings.serial_port, temp);
    else
    {
        fprintf(fp_log, "%s: serial port not defined.\n", get_log_time());
        return -1;
    }

    config_destroy(cf);

    return 1;
} /* end of parse_config_file() */

void msrecord_free()
{
    msrecord.msr_NS->datasamples = NULL;
    msr_free(&msrecord.msr_NS);

    msrecord.msr_EW->datasamples = NULL;
    msr_free(&msrecord.msr_EW);

    msrecord.msr_Z->datasamples = NULL;
    msr_free(&msrecord.msr_Z);
}

void dlconn_free()
{
    if ( dlconn->link != -1 )
        dl_disconnect (dlconn);

    if ( dlconn )
        dl_freedlcp (dlconn);
}
