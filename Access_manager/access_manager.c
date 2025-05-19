#include "access_manager.h"

void handle_error(const char *msg, int critical) {
    perror(msg);
    if (critical) exit(EXIT_FAILURE);
}

char *get_md5_hash(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    unsigned char data[BUFFER_SIZE];
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_CTX md5;
    MD5_Init(&md5);

    size_t bytes;
    while ((bytes = fread(data, 1, BUFFER_SIZE, file)) > 0) {
        MD5_Update(&md5, data, bytes);
    }
    fclose(file);

    MD5_Final(hash, &md5);
    char *hash_str = malloc(2 * MD5_DIGEST_LENGTH + 1);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    return hash_str;
}

int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out = fopen(dst, "wb");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        fwrite(buffer, 1, n, out);
    }

    fclose(in);
    fclose(out);
    return 0;
}

char *modify_filename(const char *filename, const char *progname) {
    char *base = strdup(filename);
    char *ext = strrchr(base, '.');
    if (ext) *ext = '\0';

    char *newname;
    asprintf(&newname, "%s_%s", base, progname);
    free(base);
    return newname;
}

int is_dir_empty(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 1;

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            count++;
        }
    }
    closedir(dir);
    return count == 0;
}

void execute_program(const char *filepath, ProgramInfo *prog, const char *output_path) {
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) exit(1);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        char **exec_args = malloc((prog->arg_count + 2) * sizeof(char *));
        int n = 0;
        for (int i = 0; i < prog->arg_count; i++) {
            exec_args[n++] = prog->args[i];
        }
        exec_args[n++] = (char *)filepath;
        exec_args[n] = NULL;

        execvp(prog->name, exec_args);
        free(exec_args);
        exit(1);
    }
}

void print_output_and_remove(const char *path, ProgramInfo *prog) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[BUFFER_SIZE];
    char *output = malloc(1);
    output[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        output = realloc(output, strlen(output) + strlen(line) + 1);
        strcat(output, line);
    }
    fclose(f);

    if (prog->output == NULL) {
        prog->output = output;
    } else {
        prog->output = realloc(prog->output, strlen(prog->output) + strlen(output) + 1);
        strcat(prog->output, output);
        free(output);
    }

    remove(path);
}