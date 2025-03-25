#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_FILES 10
#define MAX_PROGRAMS 5
#define MAX_ARGS 10
#define MAX_PROCESSES 5  // Максимальное количество одновременно выполняемых процессов

/*void process_file(const char *file, const char *program, char *const args[]) {
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        execvp(program, args);
        // Если execvp вернул управление, значит произошла ошибка
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Ошибка при создании процесса
        perror("fork");
    }
}*/

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Использование: %s <программа1> [аргументы программы1] -- <программа2> [аргументы программы2] -- ... -- <файл1> <файл2> ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Массивы для хранения программ, их аргументов и файлов
    char *programs[MAX_PROGRAMS];
    char *args[MAX_PROGRAMS][MAX_ARGS];
    char *files[MAX_FILES];
    int num_programs = 0;
    int num_files = 0;

    // Разбор аргументов командной строки
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--") == 0) {
            // Разделитель между программами
            i++;
        } else if (strchr(argv[i], '.') != NULL) {
            // Аргумент считается файлом, если содержит точку
            if (num_files >= MAX_FILES) {
                fprintf(stderr, "Слишком много файлов. Максимум %d файлов.\n", MAX_FILES);
                exit(EXIT_FAILURE);
            }
            files[num_files++] = argv[i++];
        } else {
            // Аргумент считается программой и её параметрами
            if (num_programs >= MAX_PROGRAMS) {
                fprintf(stderr, "Слишком много программ. Максимум %d программ.\n", MAX_PROGRAMS);
                exit(EXIT_FAILURE);
            }
            programs[num_programs] = argv[i++];
            int arg_count = 0;
            args[num_programs][arg_count++] = programs[num_programs];
            while (i < argc && strcmp(argv[i], "--") != 0 && strchr(argv[i], '.') == NULL) {
                if (arg_count >= MAX_ARGS - 1) {
                    fprintf(stderr, "Слишком много аргументов для программы. Максимум %d аргументов.\n", MAX_ARGS - 1);
                    exit(EXIT_FAILURE);
                }
                args[num_programs][arg_count++] = argv[i++];
            }
            args[num_programs][arg_count] = NULL;  // Завершаем массив аргументов NULL
            num_programs++;
        }
    }

    if (num_programs == 0 || num_files == 0) {
        fprintf(stderr, "Не указаны программы или файлы.\n");
        exit(EXIT_FAILURE);
    }

    int active_processes = 0;  // Счетчик активных процессов

    // Обрабатываем каждый файл каждой программой
    for (int i = 0; i < num_files; i++) {
        for (int j = 0; j < num_programs; j++) {
            // Ожидаем, если количество активных процессов достигло максимума
            while (active_processes >= MAX_PROCESSES) {
                wait(NULL);  // Ожидаем завершения любого дочернего процесса
                active_processes--;
            }

            // Создаем массив аргументов для текущего файла
            char *file_args[MAX_ARGS + 1];
            int arg_count = 0;
            for (int k = 0; args[j][k] != NULL; k++) {
                file_args[arg_count++] = args[j][k];
            }
            file_args[arg_count++] = files[i];
            file_args[arg_count] = NULL;  // Завершаем массив аргументов NULL

            // Запускаем процесс
            pid_t pid = fork();
            if (pid == 0) {
                // Дочерний процесс
                execvp(programs[j], file_args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork");
            } else {
                active_processes++;  // Увеличиваем счетчик активных процессов
            }
        }
    }

    // Ожидаем завершения всех оставшихся процессов
    while (active_processes > 0) {
        wait(NULL);
        active_processes--;
    }

    printf("Все файлы обработаны всеми программами.\n");
    return 0;
}