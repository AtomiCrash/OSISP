#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>

#define MAX_ARGS 20
#define BUFFER_SIZE 1024

typedef struct {
    char *name;
    char **args;
    int arg_count;
    char **file_patterns;
    int pattern_count;
} ProgramInfo;

int compare_files(const char *file1, const char *file2) {
    struct stat st1, st2;
    if (stat(file1, &st1) != 0 || stat(file2, &st2) != 0) {
        return -1;
    }

    if (st1.st_size != st2.st_size) return 1;

    int fd1 = open(file1, O_RDONLY);
    if (fd1 < 0) return -1;

    int fd2 = open(file2, O_RDONLY);
    if (fd2 < 0) {
        close(fd1);
        return -1;
    }

    char *map1 = mmap(NULL, st1.st_size, PROT_READ, MAP_PRIVATE, fd1, 0);
    char *map2 = mmap(NULL, st2.st_size, PROT_READ, MAP_PRIVATE, fd2, 0);

    if (map1 == MAP_FAILED || map2 == MAP_FAILED) {
        if (map1 != MAP_FAILED) munmap(map1, st1.st_size);
        if (map2 != MAP_FAILED) munmap(map2, st2.st_size);
        close(fd1);
        close(fd2);
        return -1;
    }

    int result = memcmp(map1, map2, st1.st_size);

    munmap(map1, st1.st_size);
    munmap(map2, st2.st_size);
    close(fd1);
    close(fd2);

    return result;
}

int copy_file(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb");
    if (!source) return -1;

    FILE *dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, source)) > 0) {
        if (fwrite(buffer, 1, bytes, dest) != bytes) {
            fclose(source);
            fclose(dest);
            remove(dst);
            return -1;
        }
    }

    fclose(source);
    fclose(dest);
    return 0;
}

void free_programs(ProgramInfo *programs, int count) {
    for (int i = 0; i < count; i++) {
        free(programs[i].args);
        for (int j = 0; j < programs[i].pattern_count; j++) {
            free(programs[i].file_patterns[j]);
        }
        free(programs[i].file_patterns);
    }
    free(programs);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <directory> <program1> [args...] [--files patterns...] -- ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *directory = argv[1];
    DIR *dir = opendir(directory);
    if (!dir) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
    closedir(dir);

    ProgramInfo *programs = NULL;
    int num_programs = 0;
    int i = 2;

    while (i < argc) {
        programs = realloc(programs, (num_programs + 1) * sizeof(ProgramInfo));
        if (!programs) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        ProgramInfo *current = &programs[num_programs];
        current->name = argv[i++];
        current->args = malloc(MAX_ARGS * sizeof(char*));
        current->arg_count = 0;
        current->file_patterns = NULL;
        current->pattern_count = 0;

        current->args[current->arg_count++] = current->name;

        while (i < argc && strcmp(argv[i], "--files") != 0 && strcmp(argv[i], "--") != 0) {
            if (current->arg_count >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many arguments for program %s\n", current->name);
                exit(EXIT_FAILURE);
            }
            current->args[current->arg_count++] = argv[i++];
        }
        current->args[current->arg_count] = NULL;

        if (i < argc && strcmp(argv[i], "--files") == 0) {
            i++;
            while (i < argc && strcmp(argv[i], "--") != 0) {
                current->file_patterns = realloc(current->file_patterns, 
                                               (current->pattern_count + 1) * sizeof(char*));
                if (!current->file_patterns) {
                    perror("Memory allocation failed");
                    exit(EXIT_FAILURE);
                }
                current->file_patterns[current->pattern_count++] = strdup(argv[i++]);
            }
        }

        if (i < argc && strcmp(argv[i], "--") == 0) {
            i++;
        }

        num_programs++;
    }

    struct dirent **file_list;
    int num_files = scandir(directory, &file_list, NULL, alphasort);
    if (num_files < 0) {
        perror("Error reading directory");
        exit(EXIT_FAILURE);
    }

    for (int f = 0; f < num_files; f++) {
        if (file_list[f]->d_type != DT_REG) {
            free(file_list[f]);
            continue;
        }

        char *filename = file_list[f]->d_name;
        char src_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", directory, filename);

        for (int p = 0; p < num_programs; p++) {
            ProgramInfo *prog = &programs[p];

            if (prog->pattern_count > 0) {
                int match = 0;
                for (int pat = 0; pat < prog->pattern_count; pat++) {
                    if (strstr(filename, prog->file_patterns[pat]) != NULL) {
                        match = 1;
                        break;
                    }
                }
                if (!match) continue;
            }

            char dst_path[PATH_MAX];
            snprintf(dst_path, sizeof(dst_path), "%s/%s_%s", directory, filename, prog->name);

            if (copy_file(src_path, dst_path) != 0) {
                fprintf(stderr, "Failed to copy %s to %s\n", src_path, dst_path);
                continue;
            }

            char *exec_args[MAX_ARGS + 2];
            int arg_count = 0;
            for (int a = 0; a < prog->arg_count; a++) {
                exec_args[arg_count++] = prog->args[a];
            }
            exec_args[arg_count++] = dst_path;
            exec_args[arg_count] = NULL;

            pid_t pid = fork();
            if (pid == 0) {
                execvp(prog->name, exec_args);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork failed");
            }
        }
        free(file_list[f]);
    }
    free(file_list);

    while (wait(NULL) > 0 || errno != ECHILD);

    dir = opendir(directory);
    if (!dir) {
        perror("Error opening directory for cleanup");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char *filename = entry->d_name;
        char *underscore = strrchr(filename, '_');
        if (!underscore) continue;

        *underscore = '\0';
        char *program_name = underscore + 1;

        char original_path[PATH_MAX];
        snprintf(original_path, sizeof(original_path), "%s/%s", directory, filename);

        char copy_path[PATH_MAX];
        snprintf(copy_path, sizeof(copy_path), "%s/%s_%s", directory, filename, program_name);

        if (access(copy_path, F_OK) == 0) {
            if (compare_files(original_path, copy_path) == 0) {
                remove(copy_path);
            }
        }
        *underscore = '_'; 
    }
    closedir(dir);

    free_programs(programs, num_programs);

    printf("All files processed successfully.\n");
    return 0;
}