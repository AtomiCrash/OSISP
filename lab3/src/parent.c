#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>  

#define CYCLE_ITERATIONS_COUNT 5000
#define MAX_PROCESSES 100
#define TIMER_NANOSECONDS 100000

typedef struct 
{
    volatile sig_atomic_t a;
    volatile sig_atomic_t b;
} Data;

int stat[4] = { 0 };
Data data = { 0, 0 };
volatile sig_atomic_t contin = 0; 

struct sigaction allow_signal, disallow_signal;
timer_t timerID;
pid_t allProcesses[MAX_PROCESSES];
int process_count = 0;
int is_stdout_open = 1;

void sigusr1_handler(int signal)
{
    (void)signal;
    is_stdout_open = 1;
}

void sigusr2_handler(int signal) 
{
    (void)signal; 
    is_stdout_open = 0;
}

void signal_handlers()
{
    allow_signal.sa_handler = sigusr1_handler;
    disallow_signal.sa_handler = sigusr2_handler;
    allow_signal.sa_flags = SA_RESTART;
    disallow_signal.sa_flags = SA_RESTART;

    sigaction(SIGUSR1, &allow_signal, NULL);
    sigaction(SIGUSR2, &disallow_signal, NULL);
}

void alarm_handler(int sig, siginfo_t *si, void *uc) 
{
    (void)sig; (void)si; (void)uc;

    const int index = data.a * 2 + data.b * 1; 
    stat[index] += 1;
    contin = 1;
}

void print_statistic(pid_t ppid, pid_t pid) 
{
    if (is_stdout_open) 
    {
        printf("PPID: %d, PID: %d, 00: %d, 01: %d, 10: %d, 11: %d\n",
               ppid, pid, stat[0], stat[1], stat[2], stat[3]);
    }
}

void setup_timer() 
{
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerID;
    timer_create(CLOCK_REALTIME, &sev, &timerID);

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = TIMER_NANOSECONDS;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = TIMER_NANOSECONDS;
    timer_settime(timerID, 0, &its, NULL);
}

void child_process() 
{
    signal_handlers(); 
    pid_t ppid = getppid();
    pid_t pid = getpid();

    for (int i = 0; i < CYCLE_ITERATIONS_COUNT; i++) {
        contin = 0;
        setup_timer();
        //struct timespec ts;
        //ts.tv_sec = 0;   
        //ts.tv_nsec = 1 * 1000000L; 
        
        while (!contin) 
        {   
            data.a ^= 1;
            //nanosleep(&ts, NULL);
            data.b ^= 1;
            //nanosleep(&ts, NULL);
        }
        
        timer_delete(timerID);
    }
    print_statistic(ppid, pid);
    exit(0);
}

void create_child() 
{
    pid_t pid = fork();

    if (pid == -1) 
    {
        perror("Error when creating new process");
        exit(1);
    }
    if (pid == 0) 
    {
        child_process();
    }
    if (pid > 0) 
    {
        printf("Parent: Created new process with PID %d\n", pid);
    }

    allProcesses[process_count++] = pid;
}

void kill_last_process() 
{
    if (process_count > 0)
    {
        pid_t pid = allProcesses[--process_count];
        kill(pid, SIGKILL);
        printf("Parent: Killed process with PID %d, Remaining: %d\n", pid, process_count);
    } 
    else 
    {
        printf("Parent: No child processes to kill\n");
    }
}

void kill_all_processes() 
{
    while (process_count > 0)
    {
        kill_last_process();
    }
    printf("Parent: Killed all child processes\n");
    process_count = 0;
}

void show_all_processes() 
{
    printf("Parent PID: %d\n", getpid());
    for (int i = 0; i < process_count; i++) 
    {
        printf("|---Child PID: %d\n", allProcesses[i]);
    }
}

void allow_stdout_for_all(int isAllow) 
{
    is_stdout_open = isAllow;
    for (int i = 0; i < process_count; i++) 
    {
        int result = kill(allProcesses[i], isAllow ? SIGUSR1 : SIGUSR2);
        if (result) 
        {
            perror("Parent: Error sending signal");
        }
    }
    printf("Parent: %s stdout for all children\n", isAllow ? "Allowed" : "Disallowed");
}

int main() 
{
    printf("\nEnter symbol (+, -, l, k, s, g, q - exit): ");
    while (1)
    {
        char symbol[10];
        if (scanf("%9s", symbol) != 1) {
            continue;
        }

        if (strcmp(symbol, "+") == 0) {
            create_child();
        } else if (strcmp(symbol, "-") == 0) {
            kill_last_process();
        } else if (strcmp(symbol, "l") == 0) {
            show_all_processes();
        } else if (strcmp(symbol, "k") == 0) {
            kill_all_processes();
        } else if (strcmp(symbol, "s") == 0) {
            allow_stdout_for_all(0);
        } else if (strcmp(symbol, "g") == 0) {
            allow_stdout_for_all(1);
        } else if (strcmp(symbol, "q") == 0) {
            kill_all_processes();
            printf("Parent: Exiting\n");
            break;
        }
    }

    return 0;
}