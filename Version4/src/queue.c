
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

struct DataQueue *CreateDataQueue(struct DataQueue *queue, FILE *fp)
{
	queue = (struct DataQueue*)malloc(sizeof(struct DataQueue));
	if (queue == NULL)
	{
		fprintf(fp, "%s: Error. Cannot allocate memory for Data Queue \n", GetLogTime());
		fflush(fp);
		return NULL;
	}
	queue->front = queue->rear=NULL;
	return queue;
}

int InitializeDataQueue(struct DataQueue *queue, FILE *fp)
{
	struct DataNode *NewNode = (struct DataNode*)malloc(sizeof(struct DataNode));
	if (NewNode == NULL)
	{
		fprintf(fp, "%s: Error. Cannot allocate memory for a new data node \n", GetLogTime());
		fflush(fp);
		return -1;
	}
	// Queue will always have 1 node
	NewNode->DataSample = NULL;
	NewNode->next = NULL;
	queue->front = queue->rear = NewNode;
	pthread_mutex_init(&queue->FrontLock, NULL);
	pthread_mutex_init(&queue->RearLock, NULL);
	return 1;

}

int InsertDataQueue(struct DataQueue *queue, char *sample, FILE *fp)
{

    struct DataNode *temp = (struct DataNode*)malloc(sizeof(struct DataNode));
  if (temp == NULL)
  {
  	fprintf(fp, "%s: Error allocating memory for node to store data sample\n", GetLogTime());
  	fflush(fp);
    return -1;
  }

  temp->DataSample = (char *)malloc(strlen(sample) + 1);
  strcpy(temp->DataSample, sample);
 	//printf("datasamples: %s\n", temp->DataSample);
 	temp->next = NULL;

  pthread_mutex_lock(&queue->RearLock);
  if (queue->rear == NULL)
  {
  	queue->front = queue->rear = temp;
    return 1;
  }

  queue->rear->next = temp;
  queue->rear = temp;
  pthread_mutex_unlock(&queue->RearLock);
  return 1;
}

int GetDataSample(struct  DataQueue *queue, char **DataSample, FILE *fp)
{
    struct DataNode *temp = NULL;
    struct DataNode *TempNode = NULL;

   pthread_mutex_lock(&queue->FrontLock);
   temp = queue->front;
   TempNode = queue->front->next;
   if(TempNode == NULL)
   {
   		pthread_mutex_unlock(&queue->FrontLock);
    	return -1;
   }
   *DataSample = (char *)malloc(strlen(TempNode->DataSample)+1);
   //*DataSample = malloc(strlen(TempNode->DataSample)+1);
   if (DataSample == NULL)
   {
   		fprintf(fp, "%s: Could not allocate memory to store data sample\n", GetLogTime());
   		fflush(fp);
   		return 0;
   }
   strcpy(*DataSample, TempNode->DataSample);

   //*DataSample = (char *)malloc(sizeof(TempNode->DataSample));
   //strcpy(*DataSample, TempNode->DataSample);
   //printf("%s\n", data_ptr);
   queue->front = TempNode;
   free(TempNode->DataSample);
   TempNode->DataSample = NULL;
   free(temp);
   pthread_mutex_unlock(&queue->FrontLock);

	 //return data_ptr;
	 return 1;
}

void DataQueueFree(struct DataQueue **queue)
{

	if(queue !=NULL)
	{
		free(*queue);
		*queue = NULL;
	}
}

int initialize_Queue_timestamp(struct Q_timestamp *queue, FILE *fp)
{
	struct Q_timestamp_node *new_node = (struct Q_timestamp_node*)malloc(sizeof(struct Q_timestamp_node));
	if(new_node == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory for a new node\n", GetLogTime());
		fflush(fp);
		return -1;
	}

	new_node->ms_record_timestamp = 0;
	new_node->next = NULL;
	queue->front = queue->rear = new_node;
	pthread_mutex_init(&queue->front_lock, NULL);
	pthread_mutex_init(&queue->rear_lock, NULL);
	return 1;
}

struct Q_timestamp *create_queue_timestamp(struct Q_timestamp *queue, FILE *fp)
{
	queue = (struct Q_timestamp*)malloc(sizeof(struct Q_timestamp));
	if(queue == NULL)
	{
		fprintf(fp, "%s: Cannot allocate memory\n", GetLogTime());
		fflush(fp);
		return NULL;
	}
	queue->front = queue->rear=NULL;
	return queue;
}

void queue_timestamp_free(struct Q_timestamp **queue)
{
	if(queue != NULL)
	{
		free(*queue);
		*queue = NULL;
	}
}

int insert_timestamp_queue(struct Q_timestamp *q, hptime_t timestamp, FILE *fp)
{

	struct Q_timestamp_node *temp = (struct Q_timestamp_node*)malloc(sizeof(struct Q_timestamp_node));
  if (temp == NULL)
  {
  	fprintf(fp, "%s: Error allocating memory for node to store timestamp\n", GetLogTime());
  	fflush(fp);
    return -1;
  }
	temp->ms_record_timestamp = timestamp;
 	temp->next = NULL;

  pthread_mutex_lock(&q->rear_lock);
  if (q->rear == NULL)
  {
  	q->front = q->rear=temp;
    return 1;
  }

  q->rear->next = temp;
  q->rear = temp;
  pthread_mutex_unlock(&q->rear_lock);
  return 1;
}
hptime_t getStartTime(struct  Q_timestamp *q_timestamp)
{

    struct Q_timestamp_node *temp = NULL;
    struct Q_timestamp_node *temp_node = NULL;

    pthread_mutex_lock(&q_timestamp->front_lock);
    temp = q_timestamp->front; // front of queue
    temp_node = q_timestamp->front->next;

    // empty queue
    if(temp_node == NULL)
    {
        pthread_mutex_unlock(&q_timestamp->front_lock);
        return 0;
    }

    hptime_t starttime = temp_node->ms_record_timestamp;
    temp_node->ms_record_timestamp = 0;

    q_timestamp->front = temp_node;
    free(temp);
    pthread_mutex_unlock(&q_timestamp->front_lock);

    return starttime;

}
