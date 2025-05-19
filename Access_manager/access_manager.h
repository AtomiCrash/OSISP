#ifndef ACCESS_MANAGER_H
#define ACCESS_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <openssl/md5.h>

#define BUFFER_SIZE 1024
#define MAX_PARALLEL_PROCESSES 10

typedef struct {
    char *name;
    char **args;
    int arg_count;
    char **file_patterns;
    int pattern_count;
    char **processed_files;
    int processed_count;
    char *output;
} ProgramInfo;

void handle_error(const char *msg, int critical);
char *get_md5_hash(const char *path);
int copy_file(const char *src, const char *dst);
char *modify_filename(const char *filename, const char *progname);
int is_dir_empty(const char *path);
void execute_program(const char *filepath, ProgramInfo *prog, const char *output_path);
void print_output_and_remove(const char *path, ProgramInfo *prog);

#endif