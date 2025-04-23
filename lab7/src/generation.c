#define _GNU_SOURCE
#include "record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <output_file> <num_records> (>=10)\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc != 3) usage(argv[0]);

    const char *filename = argv[1];
    char *endptr;
    long n = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || n < 10) {
        fprintf(stderr, "Error: num_records must be a number >= 10\n");
        return EXIT_FAILURE;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Generating %ld records into '%s'...\n", n, filename);
    record_t rec;
    for (long i = 0; i < n; i++) {
        snprintf(rec.name, NAME_LEN, "Student%03ld", i);
        snprintf(rec.address, ADDR_LEN, "Address%03ld", i);
        rec.semester = (uint8_t)((i % 8) + 1);

        if (fwrite(&rec, sizeof(rec), 1, f) != 1) {
            fprintf(stderr, "Write error at record %ld: %s\n", i, strerror(errno));
            fclose(f);
            return EXIT_FAILURE;
        }
    }

    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fprintf(stderr, "Flush/fsync failed: %s\n", strerror(errno));
        fclose(f);
        return EXIT_FAILURE;
    }
    fclose(f);

    printf("Done. %ld records written.\n", n);
    return EXIT_SUCCESS;
}