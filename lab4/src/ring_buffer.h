#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "message.h"

#define QUEUE_SIZE 10  
#define SHM_KEY 0x1234

typedef struct
{
    message buffer[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    int added_count;
    int removed_count;
    int free_space;
} message_queue;

message_queue* queue_init(int *shm_id);

void queue_destroy(int shm_id, message_queue *q);

void enqueue(message_queue *q, message *msg);

message* dequeue(message_queue *q);

void print_queue_info(message_queue *q);

#endif
