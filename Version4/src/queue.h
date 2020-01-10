#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <libdali.h>
#include "libmseed.h"
#include "ezxml.h"

/* External Buffer. Queue type implementation */
// Node inside external buffer which stores data sample.
struct DataNode
{
	char *DataSample;
	struct DataNode *next;
};


// External buffer
struct DataQueue
{
	struct DataNode *front, *rear;
	pthread_mutex_t FrontLock;
	pthread_mutex_t RearLock;

};

/* Queue definition for storing timestamps */
struct Q_timestamp_node
{
    hptime_t ms_record_timestamp;
    struct Q_timestamp_node *next;
};

struct Q_timestamp
{
    struct Q_timestamp_node *front, *rear;
    pthread_mutex_t front_lock;
    pthread_mutex_t rear_lock;
};

/* External buffer functions */
struct DataQueue *CreateDataQueue(struct DataQueue *queue, FILE *fp);
int InitializeDataQueue(struct DataQueue *queue, FILE *fp);
int InsertDataQueue(struct DataQueue *queue, char *sample, FILE *fp);
int GetDataSample(struct  DataQueue *queue, char **DataSample, FILE *fp);
void DataQueueFree(struct DataQueue **queue);

int insert_timestamp_queue(struct Q_timestamp *q, hptime_t timestamp, FILE *fp);
void queue_timestamp_free(struct Q_timestamp **queue);
struct Q_timestamp *create_queue_timestamp(struct Q_timestamp *queue, FILE *fp);
int initialize_Queue_timestamp(struct Q_timestamp *queue, FILE *fp);
hptime_t getStartTime(struct  Q_timestamp *q_timestamp);

#endif // QUEUE_H_INCLUDED
