#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void print_env_vars_from_file(const char *filename) 
{
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        perror("Error opening env");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) 
    {
        line[strcspn(line, "\n")] = 0; 
        char *value = getenv(line);
        if (value) 
        {
            printf("%s=%s\n", line, value);
        }
    }
    fclose(file);
}

void print_env_from_envp(char **envp) 
{
    for (char** env = envp; *env; env++) 
    {
        printf("%s\n", *env);
    }
}

int main(int argc, char *argv[], char *envp[])
{
    printf("Process: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    if (argc > 1) 
    {
        print_env_vars_from_file(argv[1]); 
    }
    else 
    {
        print_env_from_envp(envp);
    }

    return 0;
}
