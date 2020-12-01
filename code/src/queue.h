#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <libdali.h>
#include "libmseed.h"
#include "ezxml.h"


struct file_save_parameters
{

};
/* External Buffer. Queue type implementation */
// Node inside external buffer which stores data sample.
// node in data queue
struct data_buffer_node
{
	char *sample;
	struct data_buffer_node *next_p;
};

// External buffer. Queue type implementation
struct data_buffer
{
	struct data_buffer_node *front_p, *rear_p;
	pthread_mutex_t front_lock;
	pthread_mutex_t rear_lock;
};

/* Queue type implementation for timestamps */
struct timestamp_buffer_node
{
    hptime_t ms_record_starttime;
    struct timestamp_buffer_node *next_p;
};

struct timestamp_buffer
{
    struct timestamp_buffer_node *front_p, *rear_p;
    pthread_mutex_t front_lock;
    pthread_mutex_t rear_lock;
};

/* External buffer functions */
struct data_buffer *create_data_buffer (struct data_buffer *buffer, FILE *fp);
int initialize_data_buffer (struct data_buffer *buffer, FILE *fp);
int insert_data_buffer(struct data_buffer *buffer, char *sample, FILE *fp);
int get_data_buffer_sample(struct  data_buffer *buffer, char **sample, FILE *fp);
void free_data_buffer (struct data_buffer **buffer);

int insert_timestamp_queue(struct timestamp_buffer *buffer, hptime_t timestamp, FILE *fp);
void free_timestamp_buffer(struct timestamp_buffer **buffer);
struct timestamp_buffer *create_timestamp_buffer(struct timestamp_buffer *buffer, FILE *fp);
int initialize_timestamp_buffer(struct timestamp_buffer *buffer, FILE *fp);
hptime_t get_starttime(struct timestamp_buffer *buffer);

#endif // QUEUE_H_INCLUDED
