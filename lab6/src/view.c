#define _GNU_SOURCE
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *filename = argv[1];
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    
    uint64_t count;
    if (fread(&count, sizeof(uint64_t), 1, file) != 1) {
        fprintf(stderr, "Error reading header\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    index_record rec;
    for (uint64_t i = 0; i < count; i++) {
        if (fread(&rec, sizeof(index_record), 1, file) != 1) {
            fprintf(stderr, "Error reading record %" PRIu64 "\n", i);
            break;
        }
        printf("recno = %" PRIu64 ", time_mark = %lf\n", rec.recno, rec.time_mark);
    }
    
    fclose(file);
    return EXIT_SUCCESS;
}
