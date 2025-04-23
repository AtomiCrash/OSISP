#ifndef RECORD_H
#define RECORD_H

#include <stdint.h>

#define NAME_LEN   80
#define ADDR_LEN   80

typedef struct record_s {
    char     name[NAME_LEN];    
    char     address[ADDR_LEN]; 
    uint8_t  semester;         
} record_t;

#endif
