#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define ENV_FILE "env"
#define MAX_CHILD 100

void print_env_vars(void)
{
    extern char **environ;
    for (char **env = environ; *env; env++) 
    {
        printf("%s\n", *env);
    }
}

void read_env_file(char *envp[], int *env_count) 
{
    FILE *file = fopen(ENV_FILE, "r");
    if (!file) 
    {
        perror("Error of opening env");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file)) 
    {
        line[strcspn(line, "\n")] = 0; 
        char *value = getenv(line);
        if (value) 
        {
            size_t length = strlen(line) + strlen(value) + 2;
            envp[count] = malloc(length);
            if (!envp[count]) 
            {
                perror("Error malloc");
                exit(EXIT_FAILURE);
            }
            snprintf(envp[count], length, "%s=%s", line, value);
            count++;
        }
    }
    envp[count] = NULL;
    *env_count = count;
    fclose(file);
}


void spawn_child(int child_num, int mode) {
    char *child_path = getenv("CHILD_PATH");
    if (!child_path) {
        fprintf(stderr, "Variable CHILD_PATH doesn't set\n");
        return;
    }

    char child_exec[256];
    snprintf(child_exec, sizeof(child_exec), "%s/child", child_path);

    char child_name[16];
    snprintf(child_name, sizeof(child_name), "child_%02d", child_num);

    if (access(child_exec, X_OK) != 0) {
        perror("Child access error");
        return;
    }

    char *envp[100];  
    int env_count = 0;

    if (mode == 1) { 
        // Режим '+': берём переменные только из файла env
        read_env_file(envp, &env_count);
        char *argv[] = {child_name, ENV_FILE, NULL};
        if (fork() == 0) {
            execve(child_exec, argv, envp);
            perror("Error execve");
            exit(EXIT_FAILURE);
        }
    } 
    else if (mode == 0) { 
        // Режим '*': передаём ВСЁ окружение (берём из environ)
        extern char **environ;
        for (char **env = environ; *env != NULL; env++) {
            envp[env_count++] = *env;
        }
        envp[env_count] = NULL;
        
        char *argv[] = {child_name, NULL};
        if (fork() == 0) {
            execve(child_exec, argv, envp);
            perror("Error execve");
            exit(EXIT_FAILURE);
        }
    }

    // Освобождаем память только для mode == 1
    if (mode == 1) {
        for (int i = 0; i < env_count; i++) {
            free(envp[i]);
        }
    }
}



int main(void) 
{
    char *lc_collate = getenv("LC_COLLATE");
    setenv("LC_COLLATE", "C", 1);

    print_env_vars();

    if (lc_collate)
        setenv("LC_COLLATE", lc_collate, 1);

    int child_num = 0;
    char command;
    while (1) 
    {
        command = getchar();
        if (command == '+') 
        {
            if (child_num < MAX_CHILD) spawn_child(child_num++, 1);
        } 
        else if (command == '*') 
        {
            if (child_num < MAX_CHILD) spawn_child(child_num++, 0);
        } 
        else if (command == 'q') 
        {
            break;
        }
    }

    while (wait(NULL) > 0);
    return 0;
}
