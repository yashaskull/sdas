#ifndef MSEEDFILESETUP_H_INCLUDED
#define MSEEDFILESETUP_H_INCLUDED

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
#include "utils.h"
#include <dirent.h>



int WriteToFile (char *FileName, int PktID, FILE *fp_log);
int CreateMseedFile(DLCP *dlconn, int LowerPktID, int UpperPktID, int PacketSize, char *StreamID, char *SaveFolder, char ChannelIdentifier, FILE *fp);

int MseedFileSetup(DLCP *dlconn, int *LatestPktIDSavedE, int *LatestPktIDSavedN, int *LatestPktIDSavedZ, int reclen,
                   char *SaveFolderE, char *SaveFolderN, char *SaveFolderZ, char *FileNameE, char *FileNameN, char *FileNameZ,
                   char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], char *CheckMount, char *MseedVolume, FILE *fp_log);

int CreateFile(char *CheckMount, char *FileNameE, char *FileNameN, char *FileNameZ, char *SaveFolderE, char *SaveFolderN, char *SaveFolderZ, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ, int *LatestPktIDSavedN, int *LatestPktIDSavedE, int *LatestPktIDSavedZ, char StreamIDE[50], char StreamIDN[50], char StreamIDZ[50], int reclen, FILE *fp_log, DLCP *dlconn);

int LatestPktIDSaved (char *FileName, FILE *fp_log);

///////////////////////////////////////
int SaveFolderConfig(char *CheckMount, char *MountCommand, char *MseedVolume, char *SaveFolderUSBE, char *SaveFolderUSBN, char *SaveFolderUSBZ, FILE *fp_log);

#endif // MSEEDFILESETUP_H_INCLUDED
