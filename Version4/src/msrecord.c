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
void initialize_msrecord(MSRecord **msr_temp, char network[11], char station[11],
                         char location[11], char channelname[3], char dataquality,
                         double samprate, int8_t encoding, int8_t byteorder,
                         int64_t numsamples, char sampletype, int reclen)
{
    strcpy((*msr_temp)->network, network);
	strcpy((*msr_temp)->station, station);
	strcpy((*msr_temp)->location, location);
	strcpy((*msr_temp)->channel, channelname);
	(*msr_temp)->reclen = reclen;
	(*msr_temp)->dataquality = dataquality;
	(*msr_temp)->samprate = samprate;
	(*msr_temp)->encoding = encoding; /* INT32 encoding */
	(*msr_temp)->byteorder = byteorder; // MSB
	(*msr_temp)->numsamples = numsamples;
	(*msr_temp)->sampletype = sampletype; /* 32-bit integer */
	(*msr_temp)->samplecnt = (*msr_temp)->numsamples;
}


int process_data(MSRecord *msr_NS, MSRecord *msr_EW, MSRecord *msr_Z, int num_samples, hptime_t *hptime_starttime,
                 pthread_cond_t *cond1, pthread_mutex_t *lock_timestamp, struct data_buffer *d_queue, FILE *fp_log)
{
    int32_t *sample_block_ns = malloc(sizeof(int32_t) * num_samples);
    int32_t *sample_block_ew = malloc(sizeof(int32_t) * num_samples);
    int32_t *sample_block_z = malloc(sizeof(int32_t) * num_samples);

    if (sample_block_ns == NULL || sample_block_ew == NULL || sample_block_z == NULL)
    {
        printf("Failed to allocate memory for one or more sample blocks. Fix and restart program.\n");
        return -1;
    }

    char *data_sample;
    int get_data_buffer_sample_rv;
    int data_sample_counter = 0;
    char data_sample_token[NUM_CHANNELS][MAX_CHANNEL_LENGTH];
    char *sample_ptr;
    int sample_ptr_counter;

    char date_time [27];

    ///////////////////////
    int packed_samples;
    int packed_records;
    ///////////////////////
    write_serial();

    printf("Waiting on condition variable cond1\n");
    pthread_cond_wait(cond1, lock_timestamp);

    //ms_hptime2isotimestr(*hptime_starttime, date_time, 1);
    //printf("%s\n", date_time);
    msr_NS->starttime = *hptime_starttime;
    msr_EW->starttime = *hptime_starttime;
    msr_Z->starttime = *hptime_starttime;
    while(!kbhit())
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
            sample_ptr_counter = 0;
            sample_ptr = strtok(data_sample, "*");
            while (sample_ptr != NULL)
            {
                strcpy(data_sample_token[sample_ptr_counter], sample_ptr);
                sample_ptr_counter ++;
                sample_ptr = strtok(NULL, "*");
            }
            //printf("%s %s %s\n", data_sample_token[0], data_sample_token[1], data_sample_token[2]);
            //data_sample_counter ++;
            //printf("1\n");
            sample_block_ns[data_sample_counter] = atoi(data_sample_token[0]);
            sample_block_ew[data_sample_counter] = atoi(data_sample_token[1]);
            sample_block_z[data_sample_counter] = atoi(data_sample_token[2]);
            data_sample_counter ++;

            free (data_sample);
            //printf("1\n");*/
        }

        // block length = 200 samples
        if (data_sample_counter == num_samples)
        {

            ms_hptime2isotimestr(msr_NS->starttime, date_time, 1);
            printf("%s\n", date_time);
            // create ms records for each channel

            msr_NS->datasamples = sample_block_ns;
            msr_EW->datasamples = sample_block_ew;
            msr_Z->datasamples = sample_block_z;

            packed_records = msr_pack(msr_NS, &packed_samples, 1, verbose -1, fp_log);
            //printf("%d %d\n", packed_records, packed_samples);

            packed_records = msr_pack(msr_EW, &packed_samples, 1, verbose -1, fp_log);
            //printf("%d %d\n", packed_records, packed_samples);

            packed_records = msr_pack(msr_Z, &packed_samples, 1, verbose -1, fp_log);
            //printf("%d %d\n", packed_records, packed_samples);

            data_sample_counter = 0;
        }
    }

    if (sample_block_ns != NULL)
        free (sample_block_ns);
    if (sample_block_ew != NULL)
        free (sample_block_ew);
    if (sample_block_z != NULL)
        free (sample_block_z);

    return 1;
}

/**

void Digitizer(char *CheckMount, FILE *fp_log, struct  Q_timestamp *q_timestamp, struct DataQueue *qDataSample, MSRecord *msr_NS, MSRecord *msr_EW, MSRecord *msr_Z,
               int BlockLength, int Save2MseedFile, int Save2MseedFile_temp, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ,
               char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50],int reclen, DLCP *dlconn, int *tag)

{
    int record_counter_EW = 0;
    int record_counter_NS = 0;
    int record_counter_Z = 0;


    char *DataSample;
    char *ptrSample;

    char SaveLocationEW[70];
    char SaveLocationNS[70];
    char SaveLocationZ[70];

    char DateTime[27];
    char DateTime_noms[27];

    char FileNameEW[30];
    char FileNameNS[30];
    char FileNameZ[30];

    char PacketData[reclen];
    DLPacket dlpacket;

    FILE *ofp_e;
    FILE *ofp_n;
    FILE *ofp_z;
    overwrite = 0;
    char *perms = (overwrite) ? "wb":"ab";

    //printf("Press any key to begin\n");
    //while((getchar())!='\n'); // clear keyboard buffer
   // fprintf(fp_log, "before write to serial: %d\n",digitalRead(17));

    write_serial();
    while(!kbhit())
    {
        // get timestamp
        hptime_t starttime;
        starttime = getStartTime(q_timestamp);
        if (starttime == 0)
        {
          //  printf("not in here\n");
            usleep(1000);
            continue;
        }

     //   fprintf(fp_log, "after getting starttime: %d\n",digitalRead(17));
        // temporary check
        // check gps timestamp
        // can use any streams assuming each channel has same start and endtime
        int64_t pktid = LatestPacketID(StreamIDE, dlconn, fp_log);
        if (pktid == -1)
            fprintf(fp_log, "%s: Error retrieving last packet id\n", GetLogTime());

        else if(pktid == 0)
            fprintf(fp_log, "%s: Empty ring buffer!\n", GetLogTime());

        else
        {
            int BytesReturned = dl_read(dlconn, pktid, &dlpacket, PacketData, reclen);
            if (BytesReturned <= 0)
            {
                fprintf(fp_log, "0 bytes returned\n");
            }
            else
            {
                while(1)
                {
                    // possibly wrong starttime
                    if (starttime < dlpacket.datastart)
                    {
                        fprintf(fp_log, "Possible wrong starttime\n");
                        starttime = getStartTime(q_timestamp);
                        if (starttime == 0)
                        {
                            //printf("Struck herer\n");
                            continue;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        // actual timestamp from gps
        ms_hptime2isotimestr(starttime, DateTime, 1);
        ms_hptime2isotimestr(starttime, DateTime_noms, 0);

        printf("aa\n");
        fprintf(fp_log, "%s starttime: %s\n",GetLogTime(), DateTime);
        fflush(fp_log);


        if (Save2MseedFile == 1)
        {
            // extract date from timestamp and create folder//////////////////////
            char date[11];
            int i;
            for (i = 0; i < strlen(DateTime); i++)
            {
                if (DateTime[i] == 'T')
                    break;

                date[i] = DateTime[i];
            }
            date[i] = '\0';
            for (int i = 0; i <= strlen(date); i++)
            {
                if (date[i] == '-')
                    date[i] = '_';
            }
            for (int i = 0; i <= strlen(DateTime_noms); i++)
            {
                if (DateTime_noms[i] == '-' || DateTime_noms[i] == ':')
                    DateTime_noms[i] = '_';
            }
            strcpy(SaveLocationEW, SaveFolderUSBE);
            strcpy(SaveLocationNS, SaveFolderUSBN);
            strcpy(SaveLocationZ, SaveFolderUSBZ);
            strcat(SaveLocationEW, date);
            strcat(SaveLocationEW, "/");
            strcat(SaveLocationNS, date);
            strcat(SaveLocationNS, "/");
            strcat(SaveLocationZ, date);
            strcat(SaveLocationZ, "/");

            char mkdir[] = "mkdir ";
            char mkdir_command[strlen(mkdir) + strlen(SaveLocationEW)];
            strcpy(mkdir_command, mkdir);
            strcat(mkdir_command, SaveLocationEW);

            DIR *dir;
            dir = opendir(SaveLocationEW);
            if (dir)
            {
                //printf("Exists\n");// exists
                closedir(dir);
            }
            else if(ENOENT == errno)
                system(mkdir_command);
            else
                ;
            strcpy(mkdir_command, mkdir);
            strcat(mkdir_command, SaveLocationNS);
            dir = opendir(SaveLocationNS);
            if (dir)
            {
                //printf("Exists\n");// exists
                closedir(dir);
            }
            else if(ENOENT == errno)
                system(mkdir_command);
            else
                ;
            strcpy(mkdir_command, mkdir);
            strcat(mkdir_command, SaveLocationZ);
            dir = opendir(SaveLocationZ);
            if (dir)
            {
                //printf("Exists\n");// exists
                closedir(dir);
            }
            else if(ENOENT == errno)
                system(mkdir_command);
            else
                ;

            strcpy(FileNameEW, DateTime_noms);
            strcat(FileNameEW, "e.mseed");
            strcpy(FileNameNS, DateTime_noms);
            strcat(FileNameNS, "n.mseed");
            strcpy(FileNameZ, DateTime_noms);
            strcat(FileNameZ, "z.mseed");
            strcat(SaveLocationEW, FileNameEW);
            strcat(SaveLocationNS, FileNameNS);
            strcat(SaveLocationZ, FileNameZ);

            /* Open output file or use stdout *//**
            if ( strcmp (SaveLocationEW, "-") == 0 )
            {
                ofp_e = stdout;
            }
            else if ( (ofp_e = fopen (SaveLocationEW, perms)) == NULL )
            {
                fprintf(fp_log, "%s: Cannot open save file for EW component. Setting save2mseedfile to 0. %s\n", GetLogTime(),strerror(errno));
                fprintf(fp_log,"%s\n", SaveLocationEW);
                Save2MseedFile_temp = 0;
                //ms_log (1, "Cannot open output file %s: %s\n", msfile, strerror(errno));
                //return -1;
            }

            /* Open output file or use stdout *//**
            if ( strcmp (SaveLocationNS, "-") == 0 )
            {
                ofp_n = stdout;
            }
            else if ( (ofp_n = fopen (SaveLocationNS, perms)) == NULL )
            {
                fprintf(fp_log, "%s: Cannot open save file for NS component. Setting save2mseedfile to 0. %s\n", GetLogTime(),strerror(errno));
                fprintf(fp_log,"%s\n", SaveLocationEW);
                Save2MseedFile_temp = 0;
                //ms_log (1, "Cannot open output file %s: %s\n", msfile, strerror(errno));
                //return -1;
            }
            /* Open output file or use stdout *//**
            if ( strcmp (SaveLocationZ, "-") == 0 )
            {
                ofp_z = stdout;
            }
            else if ( (ofp_z = fopen (SaveLocationZ, perms)) == NULL )
            {
                fprintf(fp_log, "%s: Cannot open save file for Z component. Setting save2mseedfile to 0. %s\n", GetLogTime(),strerror(errno));
                fprintf(fp_log,"%s\n", SaveLocationEW);
                Save2MseedFile_temp = 0;
                //ms_log (1, "Cannot open output file %s: %s\n", msfile, strerror(errno));
                //return -1;
            }
        }

        msr_EW->starttime = starttime;
        msr_NS->starttime = starttime;
        msr_Z->starttime = starttime;

        int DataCounterE = 0;
        int DataCounterN = 0;
        int DataCounterZ = 0;
        int DataBlockCounter = 0;

        // storing 32 bit integers
        // will add funcionality to store floating point values
        int32_t *SampleBlockE = malloc(BlockLength * sizeof(int32_t));
        if (SampleBlockE == NULL)
            fprintf(fp_log, "%s: Cannot allocate memory for ADC1 buffer\n", GetLogTime());fflush(fp_log);

        int32_t *SampleBlockN = malloc(BlockLength * sizeof(int32_t));
        if (SampleBlockN == NULL)
            fprintf(fp_log, "%s: Cannot allocate memory for ADC2 buffer\n", GetLogTime());fflush(fp_log);

        int32_t *SampleBlockZ = malloc(BlockLength * sizeof(int32_t));
        if (SampleBlockZ == NULL)
            fprintf(fp_log, "%s: Cannot allocate memory for ADC3 buffer\n", GetLogTime());fflush(fp_log);


        // set counter initially
        while (!kbhit())// ADD KHBIT HERE AS WELL
        {
            int rv = GetDataSample(qDataSample, &DataSample, fp_log);
            if (rv == -1)
            {
                //fprintf(fp_log, "%s: No data.\n", GetLogTime());
                //fflush(fp_log);
                usleep(1000);
                continue;
            }
            else if(rv == 0)
            {
                fprintf(fp_log, "%s: Cannot get sample\n", GetLogTime());
                fflush(fp_log);
                usleep(1000);
                continue;
                // whatelse to do here ?? will need to come back
            }
            else
            {

                //DataSample = "*";
                // end of a block, if save to mseed file save, if not dont save
                if (strcmp(DataSample, "*") == 0)
                {
                    fprintf(fp_log, "%s: Record counter: EW: %d  NS: %d  Z: %d\n", GetLogTime(), record_counter_EW, record_counter_NS, record_counter_Z);
                    fflush(fp_log);
                    record_counter_EW = 0;
                    record_counter_NS = 0;
                    record_counter_Z = 0;
                    free(DataSample);
                    // remember to close file
                    if (ofp_e != NULL)
                        fclose(ofp_e);

                    if (ofp_e != NULL)
                        fclose(ofp_n);

                    if (ofp_e != NULL)
                        fclose(ofp_z);

                    if (Save2MseedFile == 1)
                        Save2MseedFile_temp = 1;
                    break;

                }
                else
                {
                    // store sample
                    //printf("%s\n", DataSample);
                    ptrSample = strtok(DataSample, "*"); // % channel separator
                    while(ptrSample!=NULL)
                    {
                        //printf("%s  ", ptrSample);
                        if (ptrSample[0] == 'e' && SampleBlockE != NULL) // ADC1?
                        {
                            ptrSample = RemoveChannelIdentifier(ptrSample);
                            int32_t int32_tDataSample = atoi(ptrSample);
                            //printf("%d ", int32_tDataSample);
                            SampleBlockE[DataCounterE] = int32_tDataSample;
                            DataCounterE++;
                        }
                        if (ptrSample[0] == 'n' && SampleBlockN != NULL) // ADC2?
                        {
                            ptrSample = RemoveChannelIdentifier(ptrSample);
                            int32_t int32_tDataSample = atoi(ptrSample);
                            //printf("%d ", int32_tDataSample);
                            SampleBlockN[DataCounterN] = int32_tDataSample;
                            DataCounterN++;
                        }
                        if (ptrSample[0] == 'z' && SampleBlockZ != NULL) // ADC3?
                        {
                            ptrSample = RemoveChannelIdentifier(ptrSample);
                            int32_t int32_tDataSample = atoi(ptrSample);
                            //printf("%d ", int32_tDataSample);
                            SampleBlockZ[DataCounterZ] = int32_tDataSample;
                            DataCounterZ++;
                        }
                        ptrSample = strtok(NULL, "*");
                        //free(ptrSample);
                    }
                    DataBlockCounter ++;
                    //printf("%d\n", DataBlockCounter);
                    free(DataSample);
                } // end of else
            } // end of else


            if (DataBlockCounter == BlockLength)
            {
                char DateTime[27];
                ms_hptime2isotimestr(msr_EW->starttime, DateTime, 1);
                //printf("%s\n", DateTime);

                msr_EW->numsamples = DataCounterE;
                msr_NS->numsamples = DataCounterN;
                msr_Z->numsamples = DataCounterZ;

                msr_EW->datasamples = SampleBlockE;
                msr_NS->datasamples = SampleBlockN;
                msr_Z->datasamples = SampleBlockZ;

                int PackedRecords;
                int PackedSamples;

                int ew_flag = 0;
                int ns_flag = 0;
                int z_flag = 0;
                // create records
                // ew
                PackedRecords = msr_pack(msr_EW, &PackedSamples, 1, verbose -1, dlconn, StreamIDE, Save2MseedFile_temp, ofp_e, fp_log, &tag);
                if (PackedRecords == -1)
                {
                    ew_flag = 1;
                    // if return value is -1, it means 1 or more records failed to pack
                    // update starttime for new records
                    fprintf(fp_log, "%s: Record/s failed to pack for %s.\n", GetLogTime(), StreamIDE);
                    // update starttime
                    msr_EW->starttime = msr_EW->starttime + (hptime_t) (msr_EW->numsamples / msr_EW->samprate * HPTMODULUS + 0.5);
                }
                else
                    record_counter_EW ++;
                // ns
                PackedRecords = msr_pack(msr_NS, &PackedSamples, 1, verbose -1, dlconn, StreamIDN, Save2MseedFile_temp, ofp_n, fp_log, &tag);
                if (PackedRecords == -1)
                {
                    ns_flag = 1;
                    // if return value is -1, it means 1 or more records failed to pack
                    // update starttime for new records
                    fprintf(fp_log, "%s: Record/s failed to pack for %s.\n", GetLogTime(), StreamIDN);
                    // update starttime
                    msr_NS->starttime = msr_NS->starttime + (hptime_t) (msr_NS->numsamples / msr_NS->samprate * HPTMODULUS + 0.5);
                }
                else
                    record_counter_NS ++;
                // z
                PackedRecords = msr_pack(msr_Z, &PackedSamples, 1, verbose -1, dlconn, StreamIDZ, Save2MseedFile_temp, ofp_z, fp_log, &tag);
                if (PackedRecords == -1)
                {
                    z_flag = 1;
                    // if return value is -1, it means 1 or more records failed to pack
                    // update starttime for new records
                    fprintf(fp_log, "%s: Record/s failed to pack for %s.\n  ", GetLogTime(), StreamIDZ);
                    // update starttime
                    msr_Z->starttime = msr_Z->starttime + (hptime_t) (msr_Z->numsamples / msr_Z->samprate * HPTMODULUS + 0.5);
                }
                else
                    record_counter_Z ++;

                if (ew_flag == 0 && ns_flag == 0 && z_flag == 0)
                {
                    // blink led for 0.1 seconds
                    pthread_t thread_id;
                    if (pthread_create(&thread_id, NULL, blink_LED, NULL) != 0 )
                        printf("Error creating blink led thread\n");

                }

                DataCounterE = 0;
                DataCounterN = 0;
                DataCounterZ = 0;
                DataBlockCounter = 0;
                //printf("%d\n",record_counter);
            } //end if
        }// end while
        // create mseed file
        // another timestamp will be in timestamp queue

        if (SampleBlockE != NULL)
        {
            free(SampleBlockE);
        }
        if (SampleBlockN != NULL)
            free(SampleBlockN);

        if (SampleBlockZ != NULL)
            free(SampleBlockZ);

    }// end of while
    if (ofp_e != NULL)
        fclose(ofp_e);

    if (ofp_e != NULL)
        fclose(ofp_n);

    if (ofp_e != NULL)
        fclose(ofp_z);

    return ;
}// end of func

*/

void *blink_LED(void *arg)
{
    //digitalWrite(22, HIGH);
    usleep(100000);
    //digitalWrite(22, LOW);

    pthread_detach(pthread_self());
    return NULL;
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
