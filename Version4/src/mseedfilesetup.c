#include "mseedfilesetup.h"

int WriteToFile (char *FileName, int PktID, FILE *fp_log)
{

	FILE *fp = fopen(FileName, "w"); // check for file opening errors
	if (fp == NULL)
	{
        fprintf(fp_log, "Error opening file. Please restart program\n");
        return -1;
	}
	fprintf(fp, "%d\n", PktID);
	fclose(fp);
	return 1;
}

int CreateMseedFile(DLCP *dlconn, int LowerPktID, int UpperPktID, int PacketSize, char *StreamID, char *SaveFolder, char ChannelIdentifier, FILE *fp)
{
	//  FILE format
	// 2019-01-11T21:23:56.014e.mseed
	FILE *ofp;
	flag overwrite = 0;
	char *perms = (overwrite) ? "wb" : "ab";
	char *mseed;
	char *SaveLocation;
	int PktIndex;
    char *date;
	DLPacket dlpacket;
	char *FileName = NULL;
	char DateTime[27];
	char PacketData[PacketSize];

	if ( ChannelIdentifier == 'e')
		mseed = "e.mseed";
	else if (ChannelIdentifier == 'n')
		mseed = "n.mseed";
	else if (ChannelIdentifier == 'z')
		mseed = "z.mseed";
	else
	{
		fprintf(fp, "%s: Incorrect channel identifier specified: %c. Expected either 'e', 'n', or 'z'. \n", GetLogTime(), ChannelIdentifier);// what to do in this case ?
		fflush(fp);
		return -1;
	}

	FileName = (char *)malloc(sizeof(mseed) + sizeof(DateTime)); // what to do if filename null ?
	if (FileName == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory for filename.\n", GetLogTime());
		fflush(fp);
		return -1;
	}

	// get timestamp of first packet to be saved (LowerPktID + 1)
	for (int i = LowerPktID + 1; i <= UpperPktID; i++)
	{

		int BytesReturned = dl_read(dlconn, i, &dlpacket, PacketData, PacketSize);
		if (BytesReturned <= 0)
		{
			// ignoring packet at LowerPktID + 1 and moving onto next.
			fprintf(fp, "%s: %d bytes returned. Error retrieving packet (ID: %d) from DL server. \n", GetLogTime(), BytesReturned, LowerPktID + 1);
			fflush(fp);
			continue;
		}
		else
		{
			if (strcmp(dlpacket.streamid, StreamID) == 0)
			{

				ms_hptime2isotimestr (dlpacket.datastart, DateTime, 0);
                date = malloc(sizeof(char) * strlen(DateTime));
                for (int i= 0; i < 27; i++)
                {
                    if (DateTime[i] == 'T')
                        break;

                    date[i] = DateTime[i];
                }
                for (int i = 0; i <= strlen(date); i++)
                {
                    if (date[i] == '-')
                        date[i] = '_';
                }

                //printf("DATE: %s\n", date);
				strcpy(FileName, DateTime);
				strcat(FileName, mseed);
				PktIndex = i;
				break;
			}// end of if
		}
	} // end of for

	//we have fileName
	// save location: TR_RASPI_00_HNN/FileName
	SaveLocation = (char *)malloc(sizeof(FileName) + sizeof(SaveFolder) + 70);


	if (SaveLocation == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory for SaveLocation.\n", GetLogTime());
		fflush(fp);
		free(FileName);
		return -1;
	}

    strcpy(SaveLocation, SaveFolder);
    strcat(SaveLocation, date);


    //printf("date: %s\n", date);
    //printf("SaveLocation: %s\n", SaveLocation);
    //printf("FileName: %s\n", FileName);

    DIR *dir = opendir(SaveLocation);
    //char mkdir_command[8 + strlen(SaveLocation)];
    char *mkdir_command = malloc(sizeof(char) * (8 + strlen(SaveLocation)));
    strcpy(mkdir_command, "mkdir ");
    strcat(mkdir_command, SaveLocation);

    //printf("mkdir command: %s\n", mkdir_command);
    if (dir)
    {
       // printf("Exists\n");
        closedir(dir);
    }
    else if(ENOENT == errno)
    {
        fprintf(fp, "making directory\n");
        system(mkdir_command);//printf("doesnt exits\n");
    }
    else
        ;


   // printf("date: %s\n", date);
   // printf("SaveLocation: %s\n", SaveLocation);
  //  printf("FileName: %s\n", FileName);


    strcat(SaveLocation, "/");
    strcat(SaveLocation, FileName);
	//strcpy(SaveLocation, SaveFolder);
    //strcat(SaveLocation, "/");
	//strcat(SaveLocation, FileName);
    //exit(0);
	for (int i = 0; i <= strlen(SaveLocation); i++)
	{
		if (SaveLocation[i] == ':')
			SaveLocation[i] = '_';
	}
	//fprintf(fp, "Save Location: %s\n", SaveLocation);
	//printf("Save Location: %s\n", SaveLocation);

	// open output fiile
	if (strcmp(SaveLocation, "-") == 0)
	{
		ofp = stdout;
	}
	else if ((ofp = fopen(SaveLocation, perms)) == NULL)
	{
		//ms_log(1, "Cannot open output file %s: %s\n", SaveLocation, strerror(errno));
		fprintf(fp, "%s: Cannot open output file %s: %s\n", GetLogTime(), SaveLocation, strerror(errno));
		fflush(fp);
		free(FileName);
		free(SaveLocation);
		free(mkdir_command);
		return -1;
	}
	for (int i = PktIndex; i <= UpperPktID; i++)
	{
		int BytesReturned = dl_read(dlconn, i, &dlpacket, PacketData, PacketSize);
		if (BytesReturned <= 0 )
		{
			fprintf(fp, "%s: %d bytes returned. This record will not be written to file. Moving onto other record\n", GetLogTime(), BytesReturned);
			fflush(fp);
			continue;
		}
		else
		{
			if (strcmp(dlpacket.streamid, StreamID) == 0)
			{
				if (fwrite(PacketData, PacketSize, 1, (FILE *)ofp)!=1)
				{
					//ms_log(2, "Error writing to output file \n");
					fprintf(fp, "%s: Error writing to output file\n", GetLogTime());
					fflush(fp);
					continue;
				}
				else
				{
					PktIndex = i;
				}
			}
			else
			{
				continue;
			}
		}// end of else
	} // end of for

	//printf("%s\n", FileName);
	//printf("%s\n", SaveLocation);

	free(FileName);
	free(SaveLocation);
	free(mkdir_command);
	free(date);
	fclose(ofp);
	// returns last packet ID written
	return PktIndex;
}// end of CreateMseedFile


int MseedFileSetup(DLCP *dlconn, int *LatestPktIDSavedE, int *LatestPktIDSavedN, int *LatestPktIDSavedZ, int reclen, char *SaveFolderE,
                   char *SaveFolderN, char *SaveFolderZ, char *FileNameE, char *FileNameN, char *FileNameZ,
                   char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], char *CheckMount, char *MseedVolume, FILE *fp_log)
{

    int NumStreams = TotalStreams(dlconn, fp_log);

	//printf("Number of streams: %d\n", NumStreams);
    if (NumStreams == 0) // no streams in DL server
    {
        *LatestPktIDSavedE = 0;
        *LatestPktIDSavedN = 0;
        *LatestPktIDSavedZ = 0;

        if (WriteToFile(FileNameE, 0, fp_log) == -1)
        {
            fprintf(fp_log, "Error writing to file: %s\n", FileNameE);
            return -1;
        }
        if (WriteToFile(FileNameN, 0, fp_log) == -1)
        {
            fprintf(fp_log, "Error writing to file: %s\n", FileNameN);
            return -1;
        }
        if (WriteToFile(FileNameZ, 0, fp_log) == -1)
        {
            fprintf(fp_log, "Error writing to file: %s\n", FileNameZ);
            return -1;
        }

    }
    else // streams exist in server
    {
  	// last packet saved to a file for each stream
  	//*LatestPktIDSavedE = LatestPktIDSaved(FileNameE, fp_log);
    //*LatestPktIDSavedN = LatestPktIDSaved(FileNameN, fp_log);
    //*LatestPktIDSavedZ = LatestPktIDSaved(FileNameZ, fp_log);

        *LatestPktIDSavedE = LatestPacketID(StreamIDE, dlconn, fp_log);
        if (*LatestPktIDSavedE == -1)
        {
            fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDE);
            return -1;
        }

        *LatestPktIDSavedN = LatestPacketID(StreamIDN, dlconn, fp_log);
        if (*LatestPktIDSavedN == -1)
        {
            fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDN);
            return -1;
        }

        *LatestPktIDSavedZ = LatestPacketID(StreamIDZ, dlconn, fp_log);
        if (*LatestPktIDSavedZ == -1)
        {
            fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDZ);
            return -1;
        }
    }
  	//printf("Last packet id saved\n");
    printf("e: %d\n", *LatestPktIDSavedE);
    printf("n: %d\n", *LatestPktIDSavedN);
    printf("z: %d\n", *LatestPktIDSavedZ);

    /**
    // function to return Latest packet ID inserted for each channel in server
    int LatestPktIDSavedDLE = LatestPacketID(StreamIDE, dlconn, fp_log);
    if (LatestPktIDSavedDLE == -1)
    {
        fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDE);
        return -1;
    }

    int LatestPktIDSavedDLN = LatestPacketID(StreamIDN, dlconn, fp_log);
    if (LatestPktIDSavedDLN == -1)
    {
        fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDN);
        return -1;
    }

    int LatestPktIDSavedDLZ = LatestPacketID(StreamIDZ, dlconn, fp_log);
    if (LatestPktIDSavedDLZ == -1)
    {
        fprintf(fp_log, "Error retrieiving Latest ID for stream: %s", StreamIDZ);
        return -1;
    }
    //printf("Last pkt id insertedWr\n");
    //printf("e: %d\n", LatestPktIDSavedDLE);
    //printf("n: %d\n", LatestPktIDSavedDLN);
    //printf("z: %d\n", LatestPktIDSavedDLZ);

    // e
    if (( LatestPktIDSavedDLE - *LatestPktIDSavedE) == 0)
        ;// do nothing
    else
    {
    	// save from LatestPktIDSavedE + 1 till LatestPktIDSavedDLE
        int pktid_rv = CreateMseedFile(dlconn, *LatestPktIDSavedE, LatestPktIDSavedDLE, reclen, StreamIDE, SaveFolderE, 'e', fp_log);
        if (pktid_rv == -1 )
        {
            fprintf(fp_log, "%s: Error saving for streamID%s\n",GetLogTime(), StreamIDE);
            fflush(fp_log);
        }
        else // update last pkt id saved
        {
            WriteToFile(FileNameE, pktid_rv, fp_log);
    		*LatestPktIDSavedE = pktid_rv;
    	}
    }

    // n
    if (( LatestPktIDSavedDLN - *LatestPktIDSavedN) == 0)
    	; // do nothing
    else
    {
    	// save from LatestPktIDSavedE + 1 till LatestPktIDSavedDLE
     	int pktid_rv = CreateMseedFile(dlconn, *LatestPktIDSavedN, LatestPktIDSavedDLN, reclen, StreamIDN, SaveFolderN, 'n', fp_log);
        if (pktid_rv == -1 )
    	{
    		fprintf(fp_log, "%s: Error saving for streamID %s\n",GetLogTime(), StreamIDN);
    		fflush(fp_log);
        }
        else // update last pkt id saved
        {
            WriteToFile(FileNameN, pktid_rv, fp_log);
            *LatestPktIDSavedN = pktid_rv;
    	}
    }

    // z
    if (( LatestPktIDSavedDLZ - *LatestPktIDSavedZ) == 0)
    	; // do nothing
    else
    {
    	// save from LatestPktIDSavedE + 1 till LatestPktIDSavedDLE
    	int pktid_rv = CreateMseedFile(dlconn, *LatestPktIDSavedZ, LatestPktIDSavedDLZ, reclen, StreamIDZ, SaveFolderZ, 'z', fp_log);
        if (pktid_rv == -1 )
        {
            fprintf(fp_log, "%s: Error saving for streamID %s\n",GetLogTime(), StreamIDZ);
            fflush(fp_log);
        }
        else // update last pkt id saved
        {
            WriteToFile(FileNameZ, pktid_rv, fp_log);
            *LatestPktIDSavedZ = pktid_rv;
        }
    }
  } // end of if else*/
  return 1;
} // end of function

int CreateFile(char *CheckMount, char *FileNameE, char *FileNameN, char *FileNameZ, char *SaveFolderE, char *SaveFolderN, char *SaveFolderZ, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ, int *LatestPktIDSavedN, int *LatestPktIDSavedE, int *LatestPktIDSavedZ, char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], int reclen, FILE *fp_log, DLCP *dlconn)
{
    char *SaveLocationEW;
    char *SaveLocationNS;
    char *SaveLocationZ;
    // check the usb drive where mseed files are stored is mounted or not. The mounting point should be at /dev/sda1
 	// if mounted, files will be stored in the usb
    // if not mounted, files will be stored in present working directory
    if (system(CheckMount) == 0)
    {

        //printf("mounted\n");
        SaveLocationEW = SaveFolderUSBE;
        SaveLocationNS = SaveFolderUSBN;
        SaveLocationZ = SaveFolderUSBZ;

        // if save folder does not exist.. create
        int mkdir_command_length = 7;
        DIR *dir;
        char mkdir_command[mkdir_command_length + strlen(SaveFolderUSBE)];
        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderUSBE);
        dir = opendir(SaveFolderUSBE);
        if (dir)
        {
           // printf("Exists\n");// exists
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;

        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderUSBN);
        dir = opendir(SaveFolderUSBN);
        if (dir)
        {
            //printf("Exists\n");
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;

        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderUSBZ);
        dir = opendir(SaveFolderUSBZ);
        if (dir)
        {
           // printf("Exists\n");
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;

    //free(SaveFolderUSBE);*/
    }
    else
    {
        //printf("not mounted\n");
        SaveLocationEW = SaveFolderE;
        SaveLocationNS = SaveFolderN;
        SaveLocationZ = SaveFolderZ;

         // if save folder does not exist.. create
        int mkdir_command_length = 7;
        DIR *dir;
        char mkdir_command[mkdir_command_length + strlen(SaveFolderE)];
        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderE);
        dir = opendir(SaveFolderE);
        if (dir)
        {
           // printf("Exists\n");// exists
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;

        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderN);
        dir = opendir(SaveFolderN);
        if (dir)
        {
            //printf("Exists\n");
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;

        strcpy(mkdir_command, "mkdir ");
        strcat(mkdir_command, SaveFolderZ);
        dir = opendir(SaveFolderZ);
        if (dir)
        {
           // printf("Exists\n");
            closedir(dir);
        }
        else if(ENOENT == errno)
            system(mkdir_command);
        else
            ;
    }
    int LatestID = LatestPacketID(NULL, dlconn, fp_log);
    if (LatestID == -1)
    {
        fprintf(fp_log, "%s: Error. Cannot save\n", GetLogTime());
		return -1;
	}
    else
    {
        int LastIDSaved;

        LastIDSaved = CreateMseedFile(dlconn, *LatestPktIDSavedN, LatestID, reclen, StreamIDN, SaveLocationNS, 'n', fp_log);
        if (LastIDSaved == -1)
        {
            fprintf(fp_log, "%s: Error in saving mseed file for %s\n", GetLogTime(), StreamIDN);
            fflush(fp_log);
        }
        else
        {
            WriteToFile(FileNameN, LastIDSaved, fp_log);
            *LatestPktIDSavedN = LastIDSaved;
        }
        LastIDSaved = CreateMseedFile(dlconn, *LatestPktIDSavedE, LatestID, reclen, StreamIDE, SaveLocationEW, 'e', fp_log);
        if (LastIDSaved == -1)
        {
            fprintf(fp_log, "%s: Error in saving mseed file for %s\n", GetLogTime(), StreamIDE);
            fflush(fp_log);
        }
        else
        {

            WriteToFile(FileNameE, LastIDSaved, fp_log);
            *LatestPktIDSavedE = LastIDSaved;
        }
        LastIDSaved = CreateMseedFile(dlconn, *LatestPktIDSavedZ, LatestID, reclen, StreamIDZ, SaveLocationZ, 'z', fp_log);
        if (LastIDSaved == -1)
        {
            fprintf(fp_log, "%s: Error in saving mseed file for %s\n", GetLogTime(), StreamIDZ);
            fflush(fp_log);
        }
        else
        {

            WriteToFile(FileNameZ, LastIDSaved, fp_log);
            *LatestPktIDSavedZ = LastIDSaved;
        }
    }// end else
	return 1;
	//break;
}

int LatestPktIDSaved (char *FileName, FILE *fp_log)
{

    FILE *fp = fopen(FileName, "r"); // check for file opening errors
    if (fp == NULL)
    {
        fprintf(fp_log, "Error opening file for reading.");
        return -1;
    }
    char buff[10];

    while(fgets(buff, sizeof(buff), fp) != NULL)
    {
        buff[strlen(buff) - 1] = '\0';
    }

    fclose(fp);
    return atoi(buff);
}

int SaveFolderConfig(char *CheckMount, char *MountCommand, char *MseedVolume, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ, FILE *fp_log)
{
    strcpy(CheckMount, MountCommand);
    strcat(CheckMount, MseedVolume);
    if (system(CheckMount) == 0) // mounted
    {
        fprintf(fp_log, "%s: USB drive at %s mounted!\n",GetLogTime(), MseedVolume);
        char mkdir[] = "mkdir ";

        char mkdir_command[strlen(mkdir) + strlen(SaveFolderUSBE)];
        strcpy(mkdir_command, mkdir);
        strcat(mkdir_command, SaveFolderUSBE);
        DIR *dir;
        dir = opendir(SaveFolderUSBE);
        if (dir)
        {
            fprintf(fp_log, "%s: %s directory already exists\n", GetLogTime(), SaveFolderUSBE);
            closedir(dir);
        }
        else if(ENOENT == errno)
        {
            if (system(mkdir_command) == 0)
                fprintf(fp_log, "%s: Directory created %s\n",GetLogTime(), SaveFolderUSBE);

            else
            {
                fprintf(fp_log, "%s: Cannot create directory %s. Restart program.\n",GetLogTime(), SaveFolderUSBE);// successfully created directory
                return -1;
            }
        }
        else
            ;

        strcpy(mkdir_command, mkdir);
        strcat(mkdir_command, SaveFolderUSBN);

        dir = opendir(SaveFolderUSBN);
        if (dir)
        {
            fprintf(fp_log, "%s: %s directory already exists\n", GetLogTime(), SaveFolderUSBN);
            closedir(dir);
        }
        else if(ENOENT == errno)
        {
            if (system(mkdir_command) == 0)
                fprintf(fp_log, "%s: Directory created %s\n",GetLogTime(), SaveFolderUSBN);
            else
            {
                fprintf(fp_log, "%s: Cannot create directory %s. Restart program.\n",GetLogTime(), SaveFolderUSBN);// successfully created directory
                return -1;
            }
        }
        else
        ;

        strcpy(mkdir_command, mkdir);
        strcat(mkdir_command, SaveFolderUSBZ);
        dir = opendir(SaveFolderUSBZ);
        if (dir)
        {
            fprintf(fp_log, "%s: %s directory already exists\n", GetLogTime(), SaveFolderUSBZ);
            closedir(dir);
        }
        else if(ENOENT == errno)
        {
            if (system(mkdir_command) == 0)
                fprintf(fp_log, "%s: Directory created %s\n",GetLogTime(), SaveFolderUSBZ);
            else
            {
                fprintf(fp_log, "%s: Cannot create directory %s. Restart program.\n",GetLogTime(), SaveFolderUSBZ);// successfully created directory
                return -1;
            }
        }
        else
            ;
    }
    else // not mounted
    {
        fprintf(fp_log, "usb drive not mounted. Check usb drive and restart program\n");
        return -1;
    }
    return 1;
}
