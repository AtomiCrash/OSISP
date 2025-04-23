#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>  
#include <sys/sem.h>
#include <sys/ipc.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "ring_buffer.h"
#include "message.h"

#define SEM_KEY 0x5678
#define MAX_COMMAND_LEN 16
#define MAX_PROCESSES_COUNT 100

void termination_handler(int signum);  
void delay(void);  
void read_command(char *buffer);  
void init_semaphores(void);  
void wait_semaphore(int sem_num);  
void signal_semaphore(int sem_num);  
void producer_process(void);  
void consumer_process(void);  
void create_process(const char process_type);  
void print_processes(void);  
void kill_process(int index);  
void cleanup_and_exit(void); 

typedef enum
{
    EL_COUNT_SEM,
    FREE_SPACE_SEM,
    QUEUE_ACCESS_SEM
} semaphore_type;

typedef enum
{
    CONSUMER_PROCESS,
    PRODUCER_PROCESS
} process_type;
char *process_type_arr[2] = { "consumer", "producer" };

int sem_id = 0;
int shm_id = 0;
message_queue* queue = NULL;

pid_t processes[MAX_PROCESSES_COUNT] = { 0 }; 
int processes_types[MAX_PROCESSES_COUNT] = { 0 };
int processes_count = 0;

volatile sig_atomic_t terminate_flag = 0;


void termination_handler(int signum) 
{
    (void)signum;
    terminate_flag = 1;
}

void delay(void) 
{
    struct timespec ts = { 5, 0 };
    nanosleep(&ts, NULL);
}

void read_command(char *buffer) 
{
    if (fgets(buffer, MAX_COMMAND_LEN, stdin) == NULL)
    {
        fprintf(stderr, "Error reading command\n");
        exit(EXIT_FAILURE);
    }

    buffer[strcspn(buffer, "\n")] = '\0';
}

void init_semaphores(void) 
{
    sem_id = semget(SEM_KEY, 3, IPC_CREAT | 0666);
    if (sem_id == -1) 
    {
        perror("Semget error");
        exit(EXIT_FAILURE);
    }

    semctl(sem_id, EL_COUNT_SEM, SETVAL, 0);
    semctl(sem_id, FREE_SPACE_SEM, SETVAL, QUEUE_SIZE);
    semctl(sem_id, QUEUE_ACCESS_SEM, SETVAL, 1);
}

void wait_semaphore(int sem_num) 
{
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

void signal_semaphore(int sem_num) 
{
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

void producer_process(void)
{
    signal(SIGUSR1, termination_handler);
    message msg;

    while (!terminate_flag)
    {
        wait_semaphore(FREE_SPACE_SEM);
        wait_semaphore(QUEUE_ACCESS_SEM);

        generate_message(&msg);
        enqueue(queue, &msg);

        signal_semaphore(QUEUE_ACCESS_SEM);
        signal_semaphore(EL_COUNT_SEM);

        printf("Producer: Message added, count = %d\n", queue->added_count);
        delay();
    }

    printf("Producer: Terminating\n");
    exit(EXIT_SUCCESS);
}

void consumer_process(void) 
{
    signal(SIGUSR1, termination_handler);
    message* msg;

    while (!terminate_flag) 
    {
        wait_semaphore(EL_COUNT_SEM);  
        wait_semaphore(QUEUE_ACCESS_SEM);  

        msg = dequeue(queue);

        signal_semaphore(QUEUE_ACCESS_SEM);
        signal_semaphore(FREE_SPACE_SEM);

        if (msg) 
        {
            uint16_t hash = calculate_hash(msg);
            if (hash == msg->hash) 
            {
                printf("Consumer: Message consumed, count = %d\n", queue->removed_count);
            } 
            else 
            {
                printf("Consumer: Invalid hash!\n");
            }
        }

        delay();
    }

    printf("Consumer: Terminating\n");
    exit(EXIT_SUCCESS);
}

void create_process(const char process_type)
{
    if (processes_count == MAX_PROCESSES_COUNT)
    {
        printf("Maximum number of processes reached\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) 
    {
        if (process_type == '+')
            consumer_process();
        else if (process_type == '*')
            producer_process();
    }
    else if (pid > 0) 
    {
        processes[processes_count] = pid;
        processes_types[processes_count] = (process_type == '+') ? CONSUMER_PROCESS : PRODUCER_PROCESS;
        processes_count++;
    }
    else 
    {
        perror("Fork error");
        exit(EXIT_FAILURE);
    }
}

void print_processes(void)
{
    printf("\nParent process: %d\n", getpid());
    for (int i = 0; i < processes_count; ++i)
    {
        process_type process_type = processes_types[i];
        printf("|---%d: %s, pid: %d\n", i, process_type_arr[process_type], processes[i]);
    }
    printf("\n");
}

void kill_process(int index) 
{
    if (index < 0 || index >= processes_count || processes[index] == 0) 
    {
        printf("Invalid process index: %d\n", index);
        return;
    }

    pid_t pid = processes[index];
    printf("Requesting termination of process %d (PID: %d)\n", index, pid);

    kill(pid, SIGUSR1);

    if (waitpid(pid, NULL, 0) == -1)
    {   
        perror("Waitpid error");
    }

    for (int i = index; i < processes_count - 1; i++) 
    {
        processes[i] = processes[i + 1];
        processes_types[i] = processes_types[i + 1];
    }
    processes_count--;
}

void cleanup_and_exit(void) 
{
    printf("Shutting down...\n");

    for (int i = 0; i < processes_count; ++i) 
    {
        pid_t pid = processes[i];
        if (pid != 0) {
            printf("Termination of process %d (PID: %d)\n", i, pid);
            kill(pid, SIGTERM);
        }
    }

    while (wait(NULL) > 0);

    if (!queue) 
        queue_destroy(shm_id, queue);
    semctl(sem_id, 0, IPC_RMID);

    printf("Cleanup complete. Exiting...\n");
    exit(EXIT_SUCCESS);
}

int main() 
{
    char command[MAX_COMMAND_LEN] = { 0 };
    srand(time(NULL));

    queue = queue_init(&shm_id);
    init_semaphores();

    printf("+: Create consumer\n*: Create producer\nl: Print all processes\ni: Print queue info\nk<n>: kill n process\nq: exit programm\n");
    while (1)
    {
        int process_index = 0;
        read_command(command);

        if (!strcmp(command, "+"))
        {
            create_process(command[0]);
        }
        else if (!strcmp(command, "*"))
        {
            create_process(command[0]);
        }
        else if (!strcmp(command, "l"))
        {
            print_processes();
        }
        else if (!strcmp(command, "i"))
        {
            print_queue_info(queue);
        }
        else if (command[0] == 'k' && isdigit(command[1]))
        {
            process_index = strtol(command + 1, NULL, 10);
            kill_process(process_index);
        }
        else if (!strcmp(command, "q")) 
        {
            cleanup_and_exit();
        }
    }

    return 0;
}
