
// Standard libraries
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

#include "queue.h"
#include "utils.h"

struct data_buffer *create_data_buffer(struct data_buffer *buffer, FILE *fp)
{
	buffer = (struct data_buffer*)malloc(sizeof(struct data_buffer));
	if (buffer == NULL)
	{
		fprintf(fp, "%s: Error. Cannot allocate memory for Data buffer \n", get_log_time());
		fflush(fp);
		return NULL;
	}
	buffer->front_p = buffer->rear_p = NULL;
	return buffer;
}

int initialize_data_buffer(struct data_buffer *buffer, FILE *fp)
{
	struct data_buffer_node *new_data_buffer_node = (struct data_buffer_node*)malloc(sizeof(struct data_buffer_node));
	if (new_data_buffer_node == NULL)
	{
		fprintf(fp, "%s: Error. Cannot allocate memory for a new data buffer node \n", get_log_time());
		fflush(fp);
		return -1;
	}
	// Queue will always have 1 node
	new_data_buffer_node->sample = NULL;
	new_data_buffer_node->next_p = NULL;
	buffer->front_p = buffer->rear_p = new_data_buffer_node;
	pthread_mutex_init(&buffer->front_lock, NULL);
	pthread_mutex_init(&buffer->rear_lock, NULL);
	return 1;

}

int insert_data_buffer(struct data_buffer *buffer, char *sample, FILE *fp)
{
    struct data_buffer_node *data_buffer_node_temp = (struct data_buffer_node*)malloc(sizeof(struct data_buffer_node));
    if (data_buffer_node_temp == NULL)
    {
        //fprintf(fp, "%s: Error allocating memory for node to store data sample\n", get_log_time());
        //fflush(fp);
        return -1;
    }

    data_buffer_node_temp->sample = (char *)malloc(strlen(sample) + 1);
    strcpy(data_buffer_node_temp->sample, sample);
 	data_buffer_node_temp->next_p = NULL;
    pthread_mutex_lock(&buffer->rear_lock);
    if (buffer->rear_p == NULL)
    {
        buffer->front_p = buffer->rear_p = data_buffer_node_temp;
        return 1;
    }

    buffer->rear_p->next_p = data_buffer_node_temp;
    buffer->rear_p = data_buffer_node_temp;
    pthread_mutex_unlock(&buffer->rear_lock);
    return 1;
}

int get_data_buffer_sample(struct  data_buffer *buffer, char **sample, FILE *fp)
{
    struct data_buffer_node *data_buffer_node_temp = NULL;
    struct data_buffer_node *data_buffer_node_temp_temp = NULL;

    pthread_mutex_lock(&buffer->front_lock);
    data_buffer_node_temp = buffer->front_p;
    data_buffer_node_temp_temp = buffer->front_p->next_p;

    // buffer empty
    if(data_buffer_node_temp_temp == NULL)
    {
   		pthread_mutex_unlock(&buffer->front_lock);
    	return -1;
    }
    *sample = (char *)malloc(strlen(data_buffer_node_temp_temp->sample)+1);
    //*DataSample = malloc(strlen(TempNode->DataSample)+1);
    // failed to allocate memory to store sample. exit program
    if (sample == NULL)
    {
        fprintf(fp, "%s: Could not allocate memory to store data sample\n", get_log_time());
   		fflush(fp);
        return 0;
    }
    strcpy(*sample, data_buffer_node_temp_temp->sample);

    //*DataSample = (char *)malloc(sizeof(TempNode->DataSample));
    //strcpy(*DataSample, TempNode->DataSample);
    //printf("%s\n", data_ptr);
    buffer->front_p = data_buffer_node_temp_temp;
    free (data_buffer_node_temp_temp->sample);
    data_buffer_node_temp_temp->sample = NULL;
    free(data_buffer_node_temp);
    pthread_mutex_unlock(&buffer->front_lock);

    //return data_ptr;
    return 1;
}

void free_data_buffer (struct data_buffer **buffer)
{
	if(buffer != NULL)
	{
		free(*buffer);
		*buffer = NULL;
	}
}

struct timestamp_buffer *create_timestamp_buffer(struct timestamp_buffer *buffer, FILE *fp)
{
	buffer = (struct timestamp_buffer*)malloc(sizeof(struct timestamp_buffer));
	if(buffer == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory for timestamp queue\n", get_log_time());
		fflush(fp);
		return NULL;
	}
	buffer->front_p = buffer->rear_p = NULL;
	return buffer;
}

int initialize_timestamp_buffer(struct timestamp_buffer *buffer, FILE *fp)
{
	struct timestamp_buffer_node *new_timestamp_buffer_node = (struct timestamp_buffer_node*)malloc(sizeof(struct timestamp_buffer_node));
	if(new_timestamp_buffer_node == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory for a new node\n", get_log_time());
		fflush(fp);
		return -1;
	}

	new_timestamp_buffer_node->ms_record_starttime = 0;
	new_timestamp_buffer_node->next_p = NULL;
	buffer->front_p = buffer->rear_p = new_timestamp_buffer_node;
	pthread_mutex_init(&buffer->front_lock, NULL);
	pthread_mutex_init(&buffer->rear_lock, NULL);
	return 1;
}



void free_timestamp_buffer(struct timestamp_buffer **buffer)
{
	if(buffer != NULL)
	{
		free(*buffer);
		*buffer = NULL;
	}
}

int insert_timestamp_queue(struct timestamp_buffer *buffer, hptime_t timestamp, FILE *fp)
{

    struct timestamp_buffer_node *timestamp_buffer_node_temp = (struct timestamp_buffer_node*)malloc(sizeof(struct timestamp_buffer_node));
    if (timestamp_buffer_node_temp == NULL)
    {
        fprintf(fp, "%s: Error allocating memory for node to store timestamp\n", get_log_time());
        fflush(fp);
        return -1;
    }

	timestamp_buffer_node_temp->ms_record_starttime = timestamp;
 	timestamp_buffer_node_temp->next_p = NULL;

    pthread_mutex_lock(&buffer->rear_lock);
    if (buffer->rear_p == NULL)
    {
        buffer->front_p = buffer->rear_p = timestamp_buffer_node_temp;
        return 1;
    }

  buffer->rear_p->next_p = timestamp_buffer_node_temp;
  buffer->rear_p = timestamp_buffer_node_temp;
  pthread_mutex_unlock(&buffer->rear_lock);
  return 1;
}
hptime_t get_starttime(struct timestamp_buffer *buffer)
{
    struct timestamp_buffer_node *timestamp_buffer_node_temp = NULL;
    struct timestamp_buffer_node *timestamp_buffer_node_temp_temp = NULL;

    pthread_mutex_lock(&buffer->front_lock);
    timestamp_buffer_node_temp = buffer->front_p; // front of queue
    timestamp_buffer_node_temp_temp = buffer->front_p->next_p;

    // empty queue
    if(timestamp_buffer_node_temp_temp == NULL)
    {
        pthread_mutex_unlock(&buffer->front_lock);
        return 0;
    }

    hptime_t starttime = timestamp_buffer_node_temp_temp->ms_record_starttime;
    timestamp_buffer_node_temp_temp->ms_record_starttime = 0;

    buffer->front_p = timestamp_buffer_node_temp_temp;
    free(timestamp_buffer_node_temp);
    pthread_mutex_unlock(&buffer->front_lock);

    return starttime;

}
