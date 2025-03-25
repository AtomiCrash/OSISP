#define _DEFAULT_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <getopt.h>

#define MAX_FILES 10000

int sort_output = 0;
int show_links = 0;
int show_dirs = 0;
int show_files = 0;

int compare(const void *a, const void *b) {
    return strcoll(*(const char **)a, *(const char **)b);
}

int match_type(struct stat *sb) {
    if((show_links && S_ISLNK(sb->st_mode)) ||
       (show_dirs && S_ISDIR(sb->st_mode)) ||
       (show_files && S_ISREG(sb->st_mode))) {
        return 1;
    }
    return !(show_links || show_dirs || show_files);
}

int dirwalk(char *path, char **files, int *count) {
    DIR *d;
    struct dirent *dir;
    struct stat sb;
    char fullpath[PATH_MAX];

    if ((d = opendir(path)) == NULL) {
        perror("opendir");
        return -1;
    }

    while ((dir = readdir(d)) != NULL && *count < MAX_FILES) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name) >= (int)sizeof(fullpath)) {
            fprintf(stderr, "Path too long: %s/%s\n", path, dir->d_name);
            continue;
        }

        if (lstat(fullpath, &sb) == -1) {
            perror("lstat");
            continue;
        }

        if (match_type(&sb)) {
            files[*count] = strdup(fullpath);
            if (files[*count] == NULL) {
                perror("strdup");
                closedir(d);
                return -1;
            }
            (*count)++;
        }

        if (S_ISDIR(sb.st_mode)) {
            dirwalk(fullpath, files, count);
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_COLLATE, "");

    char *files[MAX_FILES];
    int count = 0;
    char *dir_path = ".";  // Изменено с "./" на "."

    int opt;
    while ((opt = getopt(argc, argv, "sldf")) != -1) {
        switch (opt) {
            case 's':
                sort_output = 1;
                break;
            case 'l':
                show_links = 1;
                break;
            case 'd':
                show_dirs = 1;
                break;
            case 'f':
                show_files = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-s] [-l] [-d] [-f] [directory]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Обработка аргумента каталога (может быть как до, так и после опций)
    if (optind < argc) {
        dir_path = argv[optind];
    }

    if (dirwalk(dir_path, files, &count) == -1) {
        fprintf(stderr, "Error walking directory\n");
        exit(EXIT_FAILURE);
    }

    if (sort_output) {
        qsort(files, count, sizeof(char *), compare);
    }

    for (int i = 0; i < count; i++) {
        printf("%s\n", files[i]);
        free(files[i]);
    }

    return EXIT_SUCCESS;
}