#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>

typedef struct index_s {
    uint64_t recno;
    double time_mark;
} index_record;

typedef struct index_hdr_s {
    uint64_t records; 
} index_hdr;

#endif
