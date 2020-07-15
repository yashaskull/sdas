#include "msrecord.h"
/**
int MSRecord2DLServer(char *record, char streamID[50], hptime_t record_endtime, hptime_t record_starttime, DLCP *dlconn, int reclen )
{

    if ( dl_write (dlconn, record, reclen, streamID, record_starttime, record_endtime, 1) < 0 )
    {
        fprintf(stderr,"Error sending %s record to datalink server.\n", streamID);
		return -1;
    }

    return 1; // record sent
    // printf("Record sent!\n");
}
*/
int msrecord_struct_init(struct msrecord_struct *msrecord, FILE *fp_log)
{
    if(!(msrecord->msr_NS = msr_init(msrecord->msr_NS)))
    {
        fprintf(fp_log,"%s: Failed to initialize ms record for NS component.\n", get_log_time());
        return -1;
    }
    if(!(msrecord->msr_EW = msr_init(msrecord->msr_EW)))
    {
        fprintf(fp_log,"%s: Failed to initialize ms record for EW component.\n", get_log_time());
        return -1;
    }

    if(!(msrecord->msr_Z = msr_init(msrecord->msr_Z)))
    {
        fprintf(fp_log,"%s: Failed to initialize ms record for Z component.\n", get_log_time());
        return -1;
    }
    return 1;
}

void msrecord_struct_update(struct msrecord_struct *msrecord, struct msrecord_struct_members *msrecord_members)
{

    strcpy(msrecord->msr_NS->network, msrecord_members->network);
    strcpy(msrecord->msr_EW->network, msrecord_members->network);
    strcpy(msrecord->msr_Z->network, msrecord_members->network);

    strcpy(msrecord->msr_NS->station, msrecord_members->station);
    strcpy(msrecord->msr_EW->station, msrecord_members->station);
    strcpy(msrecord->msr_Z->station, msrecord_members->station);

    strcpy(msrecord->msr_NS->location, msrecord_members->location);
    strcpy(msrecord->msr_EW->location, msrecord_members->location);
    strcpy(msrecord->msr_Z->location, msrecord_members->location);

    strcpy(msrecord->msr_NS->channel, msrecord_members->channel_name_ns);
	strcpy(msrecord->msr_EW->channel, msrecord_members->channel_name_ew);
	strcpy(msrecord->msr_Z->channel, msrecord_members->channel_name_z);

	msrecord->msr_NS->reclen = msrecord->msr_EW->reclen = msrecord->msr_Z->reclen = msrecord_members->reclen;
	msrecord->msr_NS->dataquality = msrecord->msr_EW->dataquality = msrecord->msr_Z->dataquality = msrecord_members->dataquality;
	msrecord->msr_NS->samprate = msrecord->msr_EW->samprate = msrecord->msr_Z->samprate = msrecord_members->samprate;
    msrecord->msr_NS->encoding = msrecord->msr_EW->encoding = msrecord->msr_Z->encoding = msrecord_members->encoding;
    msrecord->msr_NS->byteorder = msrecord->msr_EW->byteorder = msrecord->msr_Z->byteorder = msrecord_members->byteorder;
    msrecord->msr_NS->numsamples = msrecord->msr_EW->numsamples = msrecord->msr_Z->numsamples = msrecord_members->numsamples;
    msrecord->msr_NS->sampletype = msrecord->msr_EW->sampletype = msrecord->msr_Z->sampletype = msrecord_members->sampletype;
    msrecord->msr_NS->samplecnt = msrecord->msr_EW->samplecnt = msrecord->msr_Z->samplecnt = msrecord_members->numsamples;
}
int process_data(struct msrecord_struct *msrecord, struct msrecord_struct_members *msrecord_members,
                 pthread_cond_t *cond1, pthread_mutex_t *lock_timestamp, struct data_buffer *d_queue,
                 struct timestamp_buffer *ts_queue, FILE *fp_log, DLCP *dlconn, flag save_check,
                 char *save_dir)
{
    // arrays to store data
    int32_t *sample_block_ns = malloc(sizeof(int32_t) * msrecord_members->numsamples);
    int32_t *sample_block_ew = malloc(sizeof(int32_t) * msrecord_members->numsamples);
    int32_t *sample_block_z = malloc(sizeof(int32_t) * msrecord_members->numsamples);

    if (sample_block_ns == NULL || sample_block_ew == NULL || sample_block_z == NULL)
    {
        printf("Failed to allocate memory for one or more sample blocks. Fix and restart program.\n");
        return -1;
    }

    // data samples returned from data buffer.
    // contains data for each channel
    char *data_sample;
    int get_data_buffer_sample_rv;
    // counter to keep track of data samples returned from buffer
    int data_sample_counter = 0;
    // token string for separating each channel data from data_sample
    char data_sample_token[NUM_CHANNELS][MAX_CHANNEL_LENGTH];
    // pointer to data_sample_token
    char *sample_ptr;
    int sample_ptr_counter;

    char date_time [27];

    ///////////////////////
    int packed_samples;
    int packed_records;

    hptime_t starttime = 0;
    hptime_t endtime = 0;
    //hptime_t hptime_diff;

    //float sample_period = 1/200.0;
    hptime_t hptime_sample_period = ms_time2hptime(1970, 01, 00, 00, 00, 5000);
    ///////////////////////
    //////

    /**
    * Variables associated with saving data to mseed files
    */
    // Begin time of time window for saving packets
    struct datetime starttime_save;
    // End time of time window for saving packets
    struct datetime endtime_save;
    // The following variables represent the save command initiated by the OS.
    // It uses slarchive and a time window (begin:end)
    char *slarchive_command = "~/Desktop/slarchive-2.2/./slarchive -tw ";
    char *save_option = " -CSS ";
    char *sl_host_port = " localhost:18000 ";
    int save_command_length = strlen(slarchive_command) + strlen(save_option) + strlen(sl_host_port) + strlen(save_dir) + 50;
    char save_command[save_command_length];

    // Begin Acquisition System
    fprintf(fp_log, "%s: Beginning acquisition system!\n", get_log_time());
    fflush(fp_log);

    // Send start command
    write_serial();

    //printf("Waiting on condition variable cond1\n");
    //pthread_cond_wait(cond1, lock_timestamp);

    while(!kbhit())
    {
        // get timestamp
        starttime = get_starttime(ts_queue);
        if (starttime == 0)
        {
          //  printf("not in here\n");
            usleep(1000);
            continue;
        }
        //ms_hptime2isotimestr(starttime, date_time, 1);
        //printf("%s\n", date_time);
        // set initial timestamp to begin saving with
        // since we are saving every hour, get the initial hour
        if (endtime == 0 && save_check)
        {
            extract_datetime(starttime, &starttime_save);
        }

        if(endtime != 0)
        {
            int sample_correction = time_correction(starttime, &endtime, hptime_sample_period, msrecord);
            int new_numsamples = msrecord_members->numsamples + sample_correction;

            int32_t *sample_block_ns_temp = malloc(sizeof(int32_t) * (msrecord->msr_NS->numsamples));
            int32_t *sample_block_ew_temp = malloc(sizeof(int32_t) * (msrecord->msr_EW->numsamples));
            int32_t *sample_block_z_temp = malloc(sizeof(int32_t) * (msrecord->msr_Z->numsamples));

            for (int j = 0; j < new_numsamples; j++)
            {
                sample_block_ns_temp[j] = sample_block_ns[j];
                sample_block_ew_temp[j] = sample_block_ew[j];
                sample_block_z_temp[j] = sample_block_z[j];

                if (j == msrecord_members->numsamples-1)
                {
                    if (new_numsamples <= msrecord_members->numsamples)
                        break;
                    else
                    {
                        int k = 0;
                        while (k < sample_correction)
                        {
                            sample_block_ns_temp[msrecord_members->numsamples + k] = sample_block_ns_temp[msrecord_members->numsamples - 1];
                            sample_block_ew_temp[msrecord_members->numsamples + k] = sample_block_ew_temp[msrecord_members->numsamples - 1];
                            sample_block_z_temp[msrecord_members->numsamples+ k] = sample_block_z_temp[msrecord_members->numsamples - 1];
                            k++;
                        }
                        break;
                    }
                }
            }
            msrecord->msr_NS->datasamples = sample_block_ns_temp;
            msrecord->msr_EW->datasamples = sample_block_ew_temp;
            msrecord->msr_Z->datasamples = sample_block_z_temp;

            // temp variable to store starttime of each packet about to be created
            hptime_t starttime_temp = msrecord->msr_NS->starttime;
            // any record can be used to get the starttime since they all have the same starttime
            //hptime_t starttime_dl_server = msrecord->msr_NS->starttime;

            //ms_hptime2isotimestr(msrecord->msr_NS->starttime, date_time, 1);
            //printf("%s\n", date_time);
            packed_records = msr_pack(msrecord->msr_NS, &packed_samples, 1, verbose -1, fp_log,
                                      msrecord->stream_id_ns, dlconn);

            //printf("%d\n", packed_samples);

            packed_records = msr_pack(msrecord->msr_EW, &packed_samples, 1, verbose -1, fp_log,
                                      msrecord->stream_id_ew, dlconn);

            //printf("%s  ", record);
            //printf("%d\n", packed_samples);

            packed_records = msr_pack(msrecord->msr_Z, &packed_samples, 1, verbose -1, fp_log,
                                      msrecord->stream_id_z, dlconn);
                //printf("%s\n", record);
            //printf("%d\n", packed_samples);

            free(sample_block_ns_temp);
            free(sample_block_ew_temp);
            free(sample_block_z_temp);

            msrecord->msr_NS->numsamples = msrecord->msr_EW->numsamples = msrecord->msr_Z->numsamples = msrecord_members->numsamples;
            msrecord->msr_NS->samplecnt = msrecord->msr_EW->samplecnt = msrecord->msr_Z->samplecnt = msrecord_members->numsamples;
            data_sample_counter = 0;

            // run save check
            if (save_check)
            {
                extract_datetime(starttime_temp, &endtime_save);
                // run save routine
                if (atoi(starttime_save.hour) != atoi(endtime_save.hour))
                {
                    fprintf(fp_log, "%s: Save routine time window: %s,%s,%s,%s,%s,%s:%s,%s,%s,%s,%s,%s\n", get_log_time(),
                            starttime_save.year, starttime_save.month, starttime_save.day, starttime_save.hour,
                            starttime_save.mins, starttime_save.sec, endtime_save.year, endtime_save.month,
                            endtime_save.day, endtime_save.hour, endtime_save.mins, endtime_save.sec);

                    fflush(fp_log);

                    strcpy(save_command, slarchive_command);
                    strcat(save_command, starttime_save.year);
                    strcat(save_command, ",");
                    strcat(save_command, starttime_save.month);
                    strcat(save_command, ",");
                    strcat(save_command, starttime_save.day);
                    strcat(save_command, ",");
                    strcat(save_command, starttime_save.hour);
                    strcat(save_command, ",");
                    strcat(save_command, starttime_save.mins);
                    strcat(save_command, ",");
                    strcat(save_command, starttime_save.sec);
                    strcat(save_command, ":");
                    strcat(save_command, endtime_save.year);
                    strcat(save_command, ",");
                    strcat(save_command, endtime_save.month);
                    strcat(save_command, ",");
                    strcat(save_command, endtime_save.day);
                    strcat(save_command, ",");
                    strcat(save_command, endtime_save.hour);
                    strcat(save_command, ",");
                    strcat(save_command, endtime_save.mins);
                    strcat(save_command, ",");
                    strcat(save_command, endtime_save.sec);

                    strcat(save_command, save_option);
                    strcat(save_command, save_dir);
                    strcat(save_command, sl_host_port);
                    strcat(save_command, "&");
                    system(save_command);
                    //printf("%s\n", save_command);
                    //printf("%s\n", save_location);
                    // set starttime_save = endtime_save
                    strcpy(starttime_save.year, endtime_save.year);
                    strcpy(starttime_save.month, endtime_save.month);
                    strcpy(starttime_save.day, endtime_save.day);
                    strcpy(starttime_save.hour, endtime_save.hour);
                    strcpy(starttime_save.mins, endtime_save.mins);
                    int temp_sec = atoi(endtime_save.sec) + 1;
                    sprintf(starttime_save.sec, "%d", temp_sec);
                    //strcpy(starttime_save.sec, (char)(atoi(endtime_save.sec) + 1));
                }
            }
        }

        msrecord->msr_NS->starttime = starttime;
        msrecord->msr_EW->starttime = starttime;
        msrecord->msr_Z->starttime =  starttime;
        endtime = msr_endtime(msrecord->msr_NS);

        while(data_sample_counter != msrecord_members->numsamples)
        {
            get_data_buffer_sample_rv = get_data_buffer_sample(d_queue, &data_sample, fp_log);
            if (get_data_buffer_sample_rv == -1)
            {
                //fprintf(fp_log, "%s: No data.\n", GetLogTime());
                //fflush(fp_log);
                usleep(1000);
                continue;
            }
            else if(get_data_buffer_sample_rv == 0)
            {
                fprintf(fp_log, "%s: Cannot get sample\n", get_log_time());
                fflush(fp_log);
                usleep(1000);
                continue;
                // whatelse to do here ?? will need to come back
            }
            else
            {
                //printf("%s\n", data_sample);
                sample_ptr_counter = 0;
                sample_ptr = strtok(data_sample, "*");
                while (sample_ptr != NULL)
                {
                    //printf("%s\n", sample_ptr);
                    strcpy(data_sample_token[sample_ptr_counter], sample_ptr);
                    sample_ptr_counter ++;
                    sample_ptr = strtok(NULL, "*");
                }
                //sample_block_ns[data_sample_counter] = atoi(data_sample_token[0]);
                sample_block_ns[data_sample_counter] = atoi(data_sample_token[0]);
                sample_block_ew[data_sample_counter] = atoi(data_sample_token[1]);
                sample_block_z[data_sample_counter] = atoi(data_sample_token[2]);

                data_sample_counter ++;
                free (data_sample);
            }
        }
    }

    //free(record);
    if (sample_block_ns != NULL)
        free (sample_block_ns);
    if (sample_block_ew != NULL)
        free (sample_block_ew);
    if (sample_block_z != NULL)
        free (sample_block_z);


    return 1;
}

int time_correction(hptime_t starttime, hptime_t *endtime, hptime_t hptime_sample_period,
                    struct msrecord_struct *msrecord)
{
    // run time correction algorithm
    hptime_t diff_stn1_etn = starttime - *endtime;

    // remove samples
    if(diff_stn1_etn < 0)
    {
        int i = 0;
        while(1)
        {
            msrecord->msr_NS->numsamples = msrecord->msr_NS->numsamples - 1;
            msrecord->msr_EW->numsamples = msrecord->msr_EW->numsamples - 1;
            msrecord->msr_Z->numsamples = msrecord->msr_Z->numsamples - 1;

            msrecord->msr_NS->samplecnt = msrecord->msr_NS->numsamples;
            msrecord->msr_EW->samplecnt = msrecord->msr_EW->numsamples;
            msrecord->msr_Z->samplecnt = msrecord->msr_Z->numsamples;


            *endtime = msr_endtime(msrecord->msr_NS);
            diff_stn1_etn = starttime - *endtime;
            i++;

            if (diff_stn1_etn > 0 && diff_stn1_etn <= hptime_sample_period)
                break;
        }
        return (-1*i);
    }
    // remove 1 sample
    else if(diff_stn1_etn == 0)
    {
        msrecord->msr_NS->numsamples = msrecord->msr_NS->numsamples - 1;
        msrecord->msr_EW->numsamples = msrecord->msr_EW->numsamples - 1;
        msrecord->msr_Z->numsamples = msrecord->msr_Z->numsamples - 1;

        msrecord->msr_NS->samplecnt = msrecord->msr_NS->numsamples;
        msrecord->msr_EW->samplecnt = msrecord->msr_EW->numsamples;
        msrecord->msr_Z->samplecnt = msrecord->msr_Z->numsamples;
        return -1;
    }

    // add samples
    else if (diff_stn1_etn > hptime_sample_period)
    {
        int i = 0;
        while(1)
        {
            msrecord->msr_NS->numsamples = msrecord->msr_NS->numsamples + 1;
            msrecord->msr_EW->numsamples = msrecord->msr_EW->numsamples + 1;
            msrecord->msr_Z->numsamples = msrecord->msr_Z->numsamples + 1;

            msrecord->msr_NS->samplecnt = msrecord->msr_NS->numsamples;
            msrecord->msr_EW->samplecnt = msrecord->msr_EW->numsamples;
            msrecord->msr_Z->samplecnt = msrecord->msr_Z->numsamples;

            *endtime = msr_endtime(msrecord->msr_NS);
            diff_stn1_etn = starttime - *endtime;
            i++;

            if (diff_stn1_etn > 0 && diff_stn1_etn <= hptime_sample_period)
                break;
        }
        return i;
    }
    else // diff_stn1_etn > 0 && diff_stn1_etn <= hptime_sample_period
        return 0;
}


char *generate_stream_id(MSRecord *msr)
{
    static char stream_id[50];
    // generate source name
    msr_srcname(msr, stream_id, 0);
    // generate stream id
    //strcpy(stream_id, source_name);
    //strcat(stream_id, "/");
    strcat(stream_id, "/MSEED");
    //return stream_id;
    return stream_id;

}


void extract_datetime(hptime_t hptime, struct datetime *dt)
{
    char date_time[27];
    ms_hptime2isotimestr(hptime, date_time,0);
    sscanf(date_time, "%4s-%2s-%2sT%2s:%2s:%2s", dt->year, dt->month, dt->day, dt->hour, dt->mins, dt->sec);
}
