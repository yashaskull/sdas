#include "utils.h"


char *get_log_time(void)
{
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char *time_temp = asctime(timeinfo);
	int length = strlen(time_temp);
	for (int i = 0; i < length; i++)
	{
		if (time_temp[i] == '\n')
			time_temp[i] = ' ';
	}
	return time_temp;
}

int kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(0, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(0, TCSANOW, &newt);
	oldf = fcntl(0, F_GETFL, 0);
	fcntl(0, F_SETFL, oldf | O_NONBLOCK);
	ch = getchar();

	tcsetattr(0, TCSANOW, &oldt);
	fcntl(0, F_SETFL, oldf);

	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;

}

char *RemoveChannelIdentifier(char *chanData)
{
	int i;
	for (i = 1; i < strlen(chanData); i++)
		chanData[i-1] = chanData[i];

	chanData[i-1] = '\0';

	return chanData;

}

hptime_t current_utc_hptime(void)
{

	static struct timespec time_spec;

	current_utc_time(&time_spec);
	//printf("DEBUG: time_spec.tv_sec %ld, time_spec.tv_nsec %ld, timespec2hptime(&time_spec) %lld\n", time_spec.tv_sec, time_spec.tv_nsec, timespec2hptime(&time_spec));
	return (timespec2hptime(&time_spec));

}

void current_utc_time(struct timespec *ts)
{

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif

}

hptime_t timespec2hptime(struct timespec* ts)
{

    time_t itime_sec = (time_t) ts->tv_sec;

    struct tm* tm_time = gmtime(&itime_sec);


    long hptime_sec_frac = (long) ((double) ts->tv_nsec / ((double) NANO / (double) HPTMODULUS));

    int year = tm_time->tm_year + 1900;

    int month = tm_time->tm_mon + 1;

    int mday = tm_time->tm_mday;

    int jday;

    ms_md2doy(year, month, mday, &jday);


    //printf("DEBUG: timespec2hptime(): %d.%d-%d:%d:%d.(%ld) -> %lld\n",
    //        year, jday, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec, hptime_sec_frac,
    //        ms_time2hptime(year, jday, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec, hptime_sec_frac));

    long temp = (hptime_sec_frac/1000);
    temp = temp * 1000;

    //printf("temp: %ld\n", hptime_sec_frac);
    // LOP MS_TIME2HPTIME ERROR
    return (ms_time2hptime(year, jday, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec, temp/**hptime_sec_frac)*/));

}

int TotalStreams(DLCP *dlconn, FILE *fp)
{
	static char *clientpattern = 0;
	char *infobuf = 0;
	int infolen;

	if ( (infolen = dl_getinfo (dlconn, "STREAMS", clientpattern, &infobuf, 0)) < 0 )
	{
		//dl_log (2, 0, "Problem requesting INFO STATUS from server\n");
		fprintf (fp, "%s: Problem requesting INFO STATUS from server\n", get_log_time());
		fflush(fp);
		return -1;
	}

	ezxml_t xmldoc;
	/* Parse XML string into EzXML representation */
  if ( (xmldoc = ezxml_parse_str (infobuf, infolen)) == NULL )
  {
  	//dl_log (2, 0, "info_handler(): XML parse error\n");
		fprintf (fp, "%s: info_handler(): XML parse error\n", get_log_time());
		fflush(fp);
    return -1;
  }

	ezxml_t streamlist;
	if ( ! (streamlist = ezxml_child (xmldoc, "StreamList")) )
  {
  	//dl_log (1, 0, "Cannot find StreamList element in XML response\n");
		fprintf (fp, "%s: Cannot find StreamList element in XML response\n", get_log_time());
		fflush(fp);
    return -1;
  }
  const char *StreamCount = ezxml_attr (streamlist, "TotalStreams");
	//printf("TotalStreams: %s\n", StreamCount);
	ezxml_free(xmldoc);
	free(infobuf);
	return atoi(StreamCount);

}
int GPIO_setup(FILE *fp)
{
	wiringPiSetupGpio();
	pinMode(17, INPUT);
	pinMode(22, OUTPUT);
    //pullUpDnControl(17, PUD_UP);
    //sleep(1);
    //fprintf(fp, "setup: %d\n",digitalRead(17));

	//pinMode(24, OUTPUT);
	//digitalWrite(24, HIGH);
	if (wiringPiSetup () < 0)
	{
       // printf("fail\n");
		fprintf(fp, "%s: Unable to setup wiringPi: %s\n",get_log_time(), strerror(errno));
		fflush(fp);
		return -1;
	}
	return 1;
}

int connect_dl_server(DLCP **dlconn, FILE *fp)
{

	if (!(*dlconn=dl_newdlcp("localhost", "prog")))
	{
		fprintf(fp, "%s: Cannot allocate Datalink descriptor\n", get_log_time());
		fflush(fp);
		return -1;
	}

	/* connect to DL server */
	if (dl_connect (*dlconn)<0)
	{
		fprintf(fp, "%s: Error connecting to DataLink server\n", get_log_time());
		fflush(fp);
		return -1;
	}

	return 1;

}

int LatestPacketID(char *StreamName, DLCP *dlconn, FILE *fp)
{
    static char *clientpattern = 0;
	const char *name;
	char *infobuf = NULL;
	int infolen;
	const char *pktid;
	char *eptr;

	if ( (infolen = dl_getinfo (dlconn, "STREAMS", clientpattern, &infobuf, 0)) < 0 )
	{
		//dl_log (2, 0, "Problem requesting INFO from server\n");
		fprintf (fp, "%s: Problem requesting INFO from server\n", get_log_time());
		fflush(fp);
		return -1;
	}
	ezxml_t xmldoc;
	/* Parse XML string into EzXML representation */
  if ( (xmldoc = ezxml_parse_str (infobuf, infolen)) == NULL )
  {
  	//dl_log (2, 0, "info_handler(): XML parse error\n");
  	if (infobuf != NULL)
        free(infobuf);
    fprintf (fp, "%s: info_handler(): XML parse error\n", get_log_time());
    fflush(fp);
    return -1;
  }
  ezxml_t streamlist, stream;


  if (StreamName == NULL)
  {
  	if ( ! (streamlist = ezxml_child (xmldoc, "Status")) )
  	{
  		//dl_log (1, 0, "Cannot find StreamList element in XML response\n");
  		free(infobuf);
  		fprintf (fp, "%s: Cannot find StreamList element in XML response\n", get_log_time());
  		fflush(fp);
    	return -1;
  	}

  	const char *count = ezxml_attr(streamlist, "LatestPacketID");
  	ezxml_free(xmldoc);
  	free(infobuf);
  	return atoi(count);
  }

  if ( ! (streamlist = ezxml_child (xmldoc, "StreamList")) )
  {
  	//dl_log (1, 0, "Cannot find StreamList element in XML response\n");
  	free(infobuf);
  	fprintf (fp, "%s: Cannot find StreamList element in XML response\n", get_log_time());
  	fflush(fp);
    return -1;
  }

  for(stream = ezxml_child(streamlist, "Stream"); stream; stream = ezxml_next(stream))
	{
		name = ezxml_attr(stream, "Name");
		if (strcmp(name, StreamName) == 0)
		{
			pktid = ezxml_attr(stream, "LatestPacketID");
			break;
		}
	}

  ezxml_free(xmldoc);

    if (infobuf != NULL)
        free(infobuf);

    if (pktid == NULL)
        return -1;

	//return atoi(pktid);
    return (strtoll(pktid, &eptr,10));
}


int MaximumPackets(DLCP *dlconn, FILE *fp)
{
	// Get latest packet ID from ring buffer
	static char *clientpattern = 0;
	char *infobuf = 0;
  int infolen;
  if ( (infolen = dl_getinfo (dlconn, "STREAMS", clientpattern, &infobuf, 0)) < 0 )
	{
		//dl_log (2, 0, "Problem requesting INFO from server\n");
		fprintf (fp, "%s: Problem requesting INFO from server\n", get_log_time());
		fflush(fp);
		return -1;
	}
	ezxml_t xmldoc;
	/* Parse XML string into EzXML representation */
  if ( (xmldoc = ezxml_parse_str (infobuf, infolen)) == NULL )
  {
  	//dl_log (2, 0, "info_handler(): XML parse error\n");
  	fprintf (fp, "%s: info_handler(): XML parse error\n", get_log_time());
  	fflush(fp);
    return -1;
  }
  ezxml_t streamlist;
	if ( ! (streamlist = ezxml_child (xmldoc, "Status")) )
  {
  	//dl_log (1, 0, "Cannot find StreamList element in XML response\n");
  	fprintf(fp, "%s: Cannot find StreamList element in XML response\n", get_log_time());
  	fflush(fp);
    return -1;
  }
	const char *totalcount = ezxml_attr (streamlist, "MaximumPackets");
	return (atoi(totalcount));
}


