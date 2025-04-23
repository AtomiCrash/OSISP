#define _GNU_SOURCE
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define MAX 60453.89653
#define MIN 15020.0

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <records_count (multiple of 256)> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint64_t records_count = atoll(argv[1]);
    if (records_count == 0 || records_count % 256 != 0) {
        fprintf(stderr, "Error: records_count must be a non-zero multiple of 256\n");
        return EXIT_FAILURE;
    }

    const char *filename = argv[2];
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    srand((unsigned int)time(NULL));

    if (fwrite(&records_count, sizeof(uint64_t), 1, file) != 1) {
        perror("fwrite header");
        fclose(file);
        return EXIT_FAILURE;
    }

    index_record *records = malloc(records_count * sizeof(index_record));
    if (!records) {
        perror("malloc");
        fclose(file);
        return EXIT_FAILURE;
    }

    for (uint64_t i = 0; i < records_count; i++) {
        records[i].recno = i + 1;
        records[i].time_mark = ((double)rand() / RAND_MAX) * (MAX - MIN) + MIN;
    }

    size_t written = fwrite(records, sizeof(index_record), records_count, file);
    if (written != records_count) {
        fprintf(stderr, "Error writing records\n");
        free(records);
        fclose(file);
        return EXIT_FAILURE;
    }

    free(records);
    fclose(file);
    return EXIT_SUCCESS;
}
