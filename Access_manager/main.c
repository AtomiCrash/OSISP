#include "access_manager.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <directory> <programs and files>\n", argv[0]);
        return 1;
    }

    char *directory = realpath(argv[1], NULL);
    if (!directory) handle_error("Invalid directory", 1);

    char copy_dir[PATH_MAX];
    snprintf(copy_dir, sizeof(copy_dir), "%s/copy", directory);
    mkdir(copy_dir, 0755);

    ProgramInfo *programs = NULL;
    int prog_count = 0;

    int i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            continue;
        }

        programs = realloc(programs, sizeof(ProgramInfo) * (prog_count + 1));
        ProgramInfo *p = &programs[prog_count++];
        p->name = argv[i++];
        p->args = malloc(sizeof(char*) * 2); 
        p->arg_count = 1;
        p->args[0] = p->name;
        p->file_patterns = NULL;
        p->pattern_count = 0;
        p->processed_files = NULL;
        p->processed_count = 0;
        p->output = NULL;

        while (i < argc && strcmp(argv[i], "--") && strcmp(argv[i], "--files")) {
            p->args = realloc(p->args, sizeof(char*) * (p->arg_count + 1)); 
            p->args[p->arg_count++] = argv[i++];
        }

        if (i < argc && strcmp(argv[i], "--files") == 0) {
            i++;
            while (i < argc && strcmp(argv[i], "--")) {
                p->file_patterns = realloc(p->file_patterns, sizeof(char*) * (p->pattern_count + 1));
                p->file_patterns[p->pattern_count++] = strdup(argv[i++]);
            }
        }
    }

    struct dirent **namelist;
    int n = scandir(directory, &namelist, NULL, alphasort);
    if (n < 0) handle_error("scandir failed", 1);

    int active_processes = 0;

    for (int f = 0; f < n; f++) {
        if (namelist[f]->d_type != DT_REG) {
            free(namelist[f]);
            continue;
        }

        char *filename = namelist[f]->d_name;
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, filename);

        for (int p = 0; p < prog_count; p++) {
            ProgramInfo *prog = &programs[p];

            int match = 1;
            if (prog->pattern_count > 0) {
                match = 0;
                for (int j = 0; j < prog->pattern_count; j++) {
                    if (strstr(filename, prog->file_patterns[j])) {
                        match = 1;
                        break;
                    }
                }
            }
            if (!match) continue;

            char *modname = modify_filename(filename, prog->name);
            char copypath[PATH_MAX];
            snprintf(copypath, sizeof(copypath), "%s/%s", copy_dir, modname);

            if (copy_file(fullpath, copypath) != 0) {
                free(modname);
                continue;
            }

            char *before_hash = get_md5_hash(copypath);

            char temp_out[PATH_MAX];
            snprintf(temp_out, sizeof(temp_out), "%s.out", copypath);

            while (active_processes >= MAX_PARALLEL_PROCESSES) {
                wait(NULL);
                active_processes--;
            }

            execute_program(copypath, prog, temp_out);
            active_processes++;

            int status;
            wait(&status);
            active_processes--;

            char *after_hash = get_md5_hash(copypath);
            if (before_hash && after_hash && strcmp(before_hash, after_hash) == 0) {
                remove(copypath);
            }

            print_output_and_remove(temp_out, prog);

            prog->processed_files = realloc(prog->processed_files, sizeof(char*) * (prog->processed_count + 1));
            prog->processed_files[prog->processed_count++] = strdup(modname);

            free(before_hash);
            free(after_hash);
            free(modname);
        }

        free(namelist[f]);
    }
    free(namelist);

    if (is_dir_empty(copy_dir)) {
        rmdir(copy_dir);
    }

    for (int p = 0; p < prog_count; p++) {
        ProgramInfo *prog = &programs[p];
        if (prog->output) {
            printf("Results for program: %s\n", prog->name);
            printf("%s\n", prog->output);
            free(prog->output);
        }
    }

    for (int p = 0; p < prog_count; p++) {
        free(programs[p].args);
        for (int j = 0; j < programs[p].pattern_count; j++) {
            free(programs[p].file_patterns[j]);
        }
        free(programs[p].file_patterns);
        for (int j = 0; j < programs[p].processed_count; j++) {
            free(programs[p].processed_files[j]);
        }
        free(programs[p].processed_files);
    }
    free(programs);
    free(directory);

    printf("All files processed successfully.\n");
    return 0;
}