#include "ring_buffer.h"

#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


message_queue* queue_init(int *shm_id) 
{
    *shm_id = shmget(SHM_KEY, sizeof(message_queue), IPC_CREAT | 0666);
    if (*shm_id == -1) 
    {
        perror("Shmget error");
        exit(EXIT_FAILURE);
    }

    message_queue *q = (message_queue*) shmat(*shm_id, NULL, 0);
    if (q == (void*) -1) 
    {
        perror("Shmat error");
        exit(EXIT_FAILURE);
    }

    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->added_count = 0;
    q->removed_count = 0;
    q->free_space = QUEUE_SIZE;
    memset(q->buffer, 0, sizeof(q->buffer));

    return q;
}

void queue_destroy(int shm_id, message_queue *q) 
{
    shmdt(q);
    shmctl(shm_id, IPC_RMID, NULL);
}

void enqueue(message_queue *q, message *msg)
{
    if (q->free_space == 0) 
    {
        printf("Queue is full!\n");
        return;
    }

    q->buffer[q->tail] = *msg;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    q->added_count++;
    q->free_space--;
}

message* dequeue(message_queue *q)
{
    if (q->count == 0) 
    {
        printf("Queue is empty!\n");
        return NULL;
    }

    message *msg = &q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    q->removed_count++;
    q->free_space++;

    return msg;
}

void print_queue_info(message_queue *q)
{
    printf("\nMessages count: %d, Free space: %d, Messages sent: %d, Messages receaved %d\n\n", q->count, q->free_space, q->added_count, q->removed_count);
}