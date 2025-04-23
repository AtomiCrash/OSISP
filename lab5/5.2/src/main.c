#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "./headers/queue.h"

#define MAX_CONSUMER_THREADS 10
#define MAX_PRODUCER_THREADS 10

int consumers_count = 0;
int producers_count = 0;

pthread_t consumers[MAX_CONSUMER_THREADS];
pthread_t producers[MAX_PRODUCER_THREADS];

int consumers_working[MAX_CONSUMER_THREADS];
int producers_working[MAX_PRODUCER_THREADS];

pthread_cond_t free_space_cond;
pthread_cond_t items_cond;

pthread_mutex_t queue_mutex;
pthread_mutex_t consumers_working_mutex;
pthread_mutex_t producers_working_mutex;

message_queue_t* message_queue;


// SEMS\MUTEX INIT AND END

void sync_init() {
    if (
        pthread_cond_init(&free_space_cond, NULL) != 0 
        || pthread_cond_init(&items_cond, NULL) != 0
        || pthread_mutex_init(&queue_mutex, NULL) != 0
        || pthread_mutex_init(&consumers_working_mutex, NULL) != 0
        || pthread_mutex_init(&producers_working_mutex, NULL) != 0
    ) {
        perror("Conds/mutexes creation failed");
        exit(1);
    }
}

void sync_destroy() {
    pthread_cond_destroy(&free_space_cond);
    pthread_cond_destroy(&items_cond);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&consumers_working_mutex);
    pthread_mutex_destroy(&producers_working_mutex);
}



// THREAD PROCESSING

int checkTermProducer(int ind, int is_mutex_should_be_unlocked) {
    pthread_mutex_lock(&producers_working_mutex);

    if (producers_working[ind] == 0) {
        pthread_mutex_unlock(&producers_working_mutex);
        
        if (is_mutex_should_be_unlocked) pthread_mutex_unlock(&queue_mutex);
        
        printf("\nProducer (ind %d): Closing\n", ind);
        fflush(stdout);
        return 1;
    }

    pthread_mutex_unlock(&producers_working_mutex);

    return 0;
}

void* producer_thread_processing(void* arg) {
    int ind = *(int*)arg;
    free(arg);

    while (1) {
        if (checkTermProducer(ind, 1)) return NULL;

        sleep(3);
        
        pthread_mutex_lock(&queue_mutex);
        while (queue_is_full(message_queue)) {
            if (checkTermProducer(ind, 1)) return NULL;

            pthread_cond_wait(&free_space_cond, &queue_mutex);
        }

        queue_push(queue_generate_message(), message_queue);
        printf("\nProducer (ind %d): pushed item\n", ind);

        pthread_cond_signal(&items_cond);
        pthread_mutex_unlock(&queue_mutex);
    }
}


int checkTermConsumer(int ind, int is_mutex_should_be_unlocked) {
    pthread_mutex_lock(&consumers_working_mutex);

    if (consumers_working[ind] == 0) {
        pthread_mutex_unlock(&consumers_working_mutex);
        
        if (is_mutex_should_be_unlocked) pthread_mutex_unlock(&queue_mutex);
        
        printf("\nConsumer (ind %d): Closing\n", ind);
        fflush(stdout);
        return 1;
    }

    pthread_mutex_unlock(&consumers_working_mutex);

    return 0;
}

void* consumer_thread_processing(void* arg) {
    int ind = *(int*)arg;
    free(arg);

    while (1) {
        if (checkTermConsumer(ind, 1)) return NULL;

        sleep(4);

        pthread_mutex_lock(&queue_mutex);
        while (queue_is_empty(message_queue)) {
            if (checkTermConsumer(ind, 1)) return NULL;

            pthread_cond_wait(&items_cond, &queue_mutex);
        }

        message_queue_element_t* data = queue_pop(message_queue);

        if(data) {
            printf("\nConsumer (ind %d): popped from queue: ", ind);
            for (int i = 0; i < data->size; i++) {
                printf("%d", data->data[i]);
            }
            printf("\n");

            free(data);
            data = NULL;
        }

        pthread_cond_signal(&free_space_cond);
        pthread_mutex_unlock(&queue_mutex);
    }
}



// THREAD CREATION

void create_thread(int opt) {
    // opt = +1 - producer 
    // opt = -1 - consumer

    int* thread_index;

    if (opt == 1) {
        if (producers_count < MAX_PRODUCER_THREADS) {
            thread_index = malloc(sizeof(int));
            *thread_index = 0;

            while (producers_working[*thread_index] != 0) (*thread_index)++;

            producers_working[*thread_index] = 1;

            if (pthread_create(&producers[*thread_index], NULL, producer_thread_processing, thread_index) != 0) {
                producers_working[*thread_index] = 0;
                free(thread_index);

                printf("Parent: Failure creation of producer thread\n");
            } else {
                producers_count++;
                printf("Parent: Created new producer thread\n");
            }
        } else {
            printf("Parent: producers limit is reached");
        }
    } else if (opt == -1) {
        if (consumers_count < MAX_CONSUMER_THREADS) {
            thread_index = malloc(sizeof(int));
            *thread_index = 0;

            while (consumers_working[*thread_index] != 0) (*thread_index)++;

            consumers_working[*thread_index] = 1;

            if (pthread_create(&consumers[*thread_index], NULL, consumer_thread_processing, thread_index) != 0) {
                consumers_working[*thread_index] = 0;
                free(thread_index);

                printf("Parent: Failure creation of consumer pthread\n");
            } else {
                consumers_count++;
                printf("Parent: Created new consumer pthread\n");
            }
        } else {
            printf("Parent: consumers limit is reached");
        }
    } else {
        printf("Parent: invalid pthread create option.");
    }
}


// CLOSING TREADS

void close_thread_by_ind(int ind, int type) {
    int is_should_wait = 1;

    if (type == 1) {
        pthread_mutex_lock(&producers_working_mutex);
        if (producers_count > 0 && ind >= 0 && ind < MAX_PRODUCER_THREADS && producers_working[ind] == 1) {
            if (consumers_count == 0 && message_queue->len == message_queue->max_len) {
                pthread_cancel(producers[ind]);
                is_should_wait = 0;
            }

            producers_working[ind] = 0;
            pthread_mutex_unlock(&producers_working_mutex);

            if (is_should_wait) {
                pthread_cond_broadcast(&free_space_cond);
                pthread_join(producers[ind], NULL);
            } else {
                is_should_wait = 1;
            }

            pthread_mutex_lock(&producers_working_mutex);
            producers_count--;
            pthread_mutex_unlock(&producers_working_mutex);
    
            printf("Parent: closed %dth producer thread. Remaining: %d\n", ind, producers_count);
        } else {
            pthread_mutex_unlock(&producers_working_mutex);
            printf("Parent: No producers thread to close\n");
        }
    } else if (type == -1) {
        pthread_mutex_lock(&consumers_working_mutex);
        if (consumers_count > 0 && ind >= 0 && ind < MAX_CONSUMER_THREADS && consumers_working[ind] == 1) {
            if (producers_count == 0 && message_queue->len == 0) {
                pthread_cancel(consumers[ind]);
                is_should_wait = 0;
            }

            consumers_working[ind] = 0;
            pthread_mutex_unlock(&consumers_working_mutex);

            if (is_should_wait) {
                pthread_cond_broadcast(&items_cond);
                pthread_join(consumers[ind], NULL);
            } else {
                is_should_wait = 1;
            }
    
            pthread_mutex_lock(&consumers_working_mutex);
            consumers_count--;
            pthread_mutex_unlock(&consumers_working_mutex);
    
            printf("Parent: closed %dth consumer thread. Remaining: %d\n", ind, consumers_count);
        } else {
            pthread_mutex_unlock(&consumers_working_mutex);
            printf("Parent: No consumers thread to close\n");
        }
    } else {
        printf("Parent: invalid pthread type\n");
    }
}

void close_all_threads(int type) {
    if (type == 1) {
        while (producers_count > 0) {
            close_thread_by_ind(producers_count - 1, type);
        }
    
        printf("Parent: Closed all producers\n");
    } else if (type == -1) {
        while (consumers_count > 0) {
            close_thread_by_ind(consumers_count - 1, type);
        }
    
        printf("Parent: Closed all consumers\n");
    } else {
        printf("Parent: invalid process type\n");
    }
}


// EXIT PROGRAM

void cleanup_and_exit() {
    printf("\nShutting down...\n");

    close_all_threads(1);
    close_all_threads(-1);

    sync_destroy();

    if (message_queue) {
        queue_free(message_queue);
        free(message_queue);
        message_queue = NULL;
    }

    printf("\nCleanup complete. Exiting...\n");
}

void termination_handler(int signum) {
    (void)signum;
    cleanup_and_exit();
    exit(1);
}


// QUEUE

void message_queue_init() {
    message_queue = malloc(sizeof(message_queue_t));
    if (!message_queue) {
        perror("Failed to allocate memory for message_queue");
        exit(EXIT_FAILURE);
    }
    queue_init(message_queue);
}


int main() {
    signal(SIGINT, termination_handler);
    srand(time(NULL));

    message_queue_init();
    sync_init();

    printf("\nEnter option:");
    printf("\n+ add producer");
    printf("\n- remove last added producer");
    printf("\n* add consumer");
    printf("\n_ remove last added consumer");
    printf("\nl print queue");
    printf("\ns print childs");
    printf("\ne expand queue (len + 1)");
    printf("\nr reduce queue (len - 1)");
    printf("\nq quit");

    while (1) {
        char option[10];

        printf("\nOPtion: ");
        if (scanf("%9s", option) != 1) {
            continue;
        }

        if (strcmp(option, "+") == 0) {
            create_thread(1);
        } else if (strcmp(option, "*") == 0) {
            create_thread(-1);
        } else if (strcmp(option, "-") == 0) {
            close_thread_by_ind(producers_count - 1, 1);
        } else if (strcmp(option, "_") == 0) {
            close_thread_by_ind(consumers_count - 1, -1);
        } else if (strcmp(option, "l") == 0) {
            queue_print(message_queue);
        } else if (strcmp(option, "s") == 0) {
            printf("\nParent: Now %d producer threads, %d consumer threads", producers_count, consumers_count);
        } else if (strcmp(option, "r") == 0) {
            if (consumers_count == 0) continue;

            pthread_mutex_lock(&queue_mutex);
            while (queue_is_full(message_queue)) {
                pthread_cond_wait(&free_space_cond, &queue_mutex);
            }
            
            queue_reduce(message_queue);
            pthread_mutex_unlock(&queue_mutex);
        } else if (strcmp(option, "e") == 0) {
            pthread_mutex_lock(&queue_mutex);
            queue_expand(message_queue);
            pthread_mutex_unlock(&queue_mutex);   

            pthread_cond_signal(&free_space_cond);
        } else if (strcmp(option, "q") == 0) {
            cleanup_and_exit();
            break;
        } 
    }
    
    return 0;
}

    

