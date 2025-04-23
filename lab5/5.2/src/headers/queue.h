#ifndef QUEUE_H
#define QUEUE_H

#include <inttypes.h>

#define QUEUE_BASE_SIZE 10
#define MESSAGE_SIZE 20

typedef struct {
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    uint32_t data[MESSAGE_SIZE];
} message_queue_element_t;

typedef struct {
    int head;
    int tail;
    int len;
    int max_len;
    message_queue_element_t** messages;
} message_queue_t;

void queue_init(message_queue_t* queue);
void queue_push(message_queue_element_t* new_message, message_queue_t* queue);
message_queue_element_t* queue_pop(message_queue_t* queue);
void queue_print(message_queue_t* queue);
message_queue_element_t* queue_generate_message();
void queue_reduce(message_queue_t* queue);
void queue_expand(message_queue_t* queue);
void queue_free(message_queue_t* queue);
int queue_is_full(message_queue_t* queue);
int queue_is_empty(message_queue_t* queue);

#endif