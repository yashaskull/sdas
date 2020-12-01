#include "serialutils.h"

int inputAvailable(int fn_io)
{
	static struct timeval tv;
  fd_set rfds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&rfds);
  FD_SET(fn_io, &rfds);
  //select(fn_io + 1, &rfds, NULL, NULL, &tv);
  //return (FD_ISSET(fn_io, &rfds));
  int retval = select(fn_io + 1, &rfds, NULL, NULL, &tv);
  if (retval == -1)
  	return (0);
  else if (retval)
  	return (1);
  else
  	return (0);

}

char* read_serial(long timeout, FILE *fp)
{
    static char line_data[33];
    //static char *line_data;
    static int n_line_data;
		//int32_t sample_32int=0;
    char buf[1];
    n_line_data = 0;

    long total_sleep = 0;

    // skip any leading \r\n chars
    // waits 1 second before timing out
    while (total_sleep < timeout && n_line_data < MAX_LINE_DATA)
    {
        //if (timeout == TIMEOUT_LARGE)
        //    printf("inputAvailable(fd_port):%d\n", inputAvailable(fd_port));
        if (inputAvailable(fd_port) && read(fd_port, buf, sizeof buf) == 1)
        { // read up to 1 characters if ready to read)

            if (buf[0] == '\r' || buf[0] == '\n')
            {
                continue;
            }

            line_data[n_line_data] = buf[0];
            n_line_data++;
            break;
        }
        else
        {

            usleep(10000);
            total_sleep += 10000;
        }
    }

    if (total_sleep >= timeout)
    {
            printf("serial timeout\n");
    		//fprintf(fp, "%s: TIMEOUT\n", GetLogTime());
    		//fflush(fp);
    		return NULL;
    }

    // get data timestamp
    //*phptime = current_utc_hptime();

    // read value chars
    while (total_sleep < timeout && n_line_data < MAX_LINE_DATA)
    {
        if (inputAvailable(fd_port) && read(fd_port, buf, sizeof buf))
        { // read up to 1 characters if ready to read)

            if (buf[0] == '\r' || buf[0] == '\n')
            {
                break;
            }
            if (isdigit(buf[0]))
            {

            	//printf("%c", buf[0]);
      				line_data[n_line_data] = buf[0];
            }else
            {
            	//line_data[n_line_data] = '0';
            	//printf("b: %c\n", buf[0]);

            	line_data[n_line_data] = buf[0];
            }

            n_line_data++;
        } else
        {

            usleep(10000);
            total_sleep += 10000;
        }
    }
    line_data[n_line_data] = 0;
		//for (int i = 0; i < 11; i++)
			//printf("%c", line_data[i]);

    if (total_sleep >= timeout)
    {
        printf("serial timeout\n");
        //fprintf(fp, "%s: TIMEOUT\n", GetLogTime());
        //fflush(fp);
        return NULL;
    }

    return line_data;
}

int open_serial_port(void)
{

	fd_port = open("/dev/ttyACM0",O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd_port == -1)						/* Error Checking */
  	return -1;

  return 1;
}

int serial_port_settings (void)
{

	int BaudRate = 115200;
	/*---------- Setting the Attributes of the serial port using termios structure --------- */

	struct termios SerialPortSettings;	/* Create the structure                          */

	tcgetattr(fd_port, &SerialPortSettings);	/* Get the current attributes of the Serial port */


	memset(&SerialPortSettings, 0, sizeof(SerialPortSettings));
	SerialPortSettings.c_iflag = 0;
	SerialPortSettings.c_oflag = 0;
	SerialPortSettings.c_cflag = CS8 | CREAD | CLOCAL;
	SerialPortSettings.c_lflag = 0;
	SerialPortSettings.c_cc[VMIN] = 0;
	SerialPortSettings.c_cc[VTIME] = 0;

		/* Setting the Baud rate */
	cfsetispeed(&SerialPortSettings,B115200); /* Set Read  Speed as 9600                       */
	cfsetospeed(&SerialPortSettings,B115200); /* Set Write Speed as 9600                       */

	/* 8N1 Mode */
	SerialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	SerialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
	SerialPortSettings.c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
	SerialPortSettings.c_cflag |=  CS8;      /* Set the data bits = 8                                 */

	SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */


	SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
	SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

	SerialPortSettings.c_oflag &= ~OPOST;/*No Output Processing*/

	/* Setting Time outs */
	SerialPortSettings.c_cc[VMIN] = 10; /* Read at least 10 characters */
	SerialPortSettings.c_cc[VTIME] = 0; /* Wait indefinetly   */

	cfmakeraw(&SerialPortSettings);
	tcflush(fd_port, TCIOFLUSH);
	if((tcsetattr(fd_port,TCSANOW,&SerialPortSettings)) != 0) /* Set the attributes to the termios structure*/
	{
		//printf("ERROR ! in Setting attributes\n");
		return -1;
	}
	else
    printf(" BaudRate = %d \n  StopBits = 1 \n  Parity   = none\n", BaudRate);

  return 1;
}

int flush_serial(void)
{
	if(tcflush(fd_port, TCIOFLUSH) < 0)
		return -1;

	return 1;
}

void write_serial(void)
{
	write(fd_port, "1", 1);
}

void close_serial(void)
{
	close(fd_port);
}

int serial_port_fd(void)
{
	return fd_port;
}
