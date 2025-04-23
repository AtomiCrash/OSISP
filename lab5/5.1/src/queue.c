#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "./headers/queue.h"

void queue_init(message_queue_t* queue) {
    if (!queue) return;

    queue->head = 0;
    queue->tail = 0;
    queue->len = 0;
    queue->max_len = QUEUE_BASE_SIZE;

    queue->messages = malloc(sizeof(message_queue_element_t*) * queue->max_len);
    for (int i = 0; i < queue->max_len; i++) {
        queue->messages[i] = NULL;
    }
}

void queue_expand(message_queue_t* queue) {
    if (!queue) {
        printf("Queue: queue is null");
        return;
    }

    queue->messages = realloc(queue->messages, sizeof(message_queue_element_t*) * ++(queue->max_len));
    
    if (queue->tail < queue->head) {
        for (int i = queue->max_len - 1; i > queue->head; i--) {
            queue->messages[i] = queue->messages[i - 1];
        }

        queue->messages[queue->head] = NULL;
        queue->head++;
    } else {
        queue->messages[queue->max_len - 1] = NULL;
    }
}

void queue_reduce(message_queue_t* queue) {
    if (!queue) {
        printf("\nQueue: queue is null");
        return;
    }

    if (queue->len == queue->max_len) {
        return;
    }

    if (queue->max_len == 1) {
        printf("\nQueue: cannot reduce queue, max len is actualy 1");
        return;
    }

    if (queue->head > queue->tail) {
        for (int i = queue->head; i < queue->max_len; i++) {
            queue->messages[i - 1] = queue->messages[i];
        }
        queue->messages[queue->max_len - 1] = NULL;
        queue->head--;
    }

    queue->messages = realloc(queue->messages, sizeof(message_queue_element_t*) * --(queue->max_len));

    printf("\nQueue: reduced, actual max-len = %d", queue->max_len);
    return;
}

void queue_free(message_queue_t* queue) {
    if (!queue || !queue->messages) return;

    for (int i = 0; i < queue->max_len; i++) {
        if (queue->messages[i] != NULL) {
            free(queue->messages[i]);
            queue->messages[i] = NULL;
        }
    }

    free(queue->messages);
    queue->messages = NULL;
}


void queue_push(message_queue_element_t* new_message, message_queue_t* queue) {
    if (!queue || !new_message) {
        printf("Queue: queue or new message is null");
        return;
    }    

    if (queue->len >= queue->max_len) {
        printf("\nQueue: cannot push message, queue is full");
        return;
    } 

    if (queue->len != 0) {
        queue->tail = (queue->tail + 1) % queue->max_len;
    }

    queue->messages[queue->tail] = new_message;
    queue->len++;

    printf("\nQueue: message was pushed");
    fflush(stdout);
}

message_queue_element_t* queue_pop(message_queue_t* queue) {
    if (!queue) {
        printf("\nQueue: queue ptr is null");
        exit(1);
    };

    if (queue->len == 0) {
        printf("\nQueue: cannot pop message, queue is empty");
        return NULL;
    }

    message_queue_element_t* data = queue->messages[queue->head];

    queue->messages[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->max_len;
    queue->len--;

    return data;
}

message_queue_element_t* queue_generate_message() {
    message_queue_element_t* message = malloc(sizeof(message_queue_element_t));
    message->size = rand() % 20 + 1;
    message->type = 1;
    message->hash = 0;

    for (int i = 0; i < message->size; i++) {
        message->data[i] = rand() % 9 + 1;
    }

    return message;
}

void queue_print(message_queue_t* queue) {
    if (!queue) return;

    printf("\nQueue: current state of queue: ");
    printf("\n  (max len %d ; current len %d ; head position %d ; tail position %d)", queue->max_len, queue->len, queue->head, queue->tail);

    for (int i = 0; i < queue->max_len; i++) {
        if (queue->messages[i] != NULL) {
            printf("\n\t");
            for (int j = 0; j < queue->messages[i]->size; j++) {
                printf("%d", queue->messages[i]->data[j]);
            }
        }
    }

    printf("\n\t");
}
