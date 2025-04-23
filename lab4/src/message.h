#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#define MAX_DATA_SIZE 256  

typedef struct 
{
    uint8_t type;            
    uint16_t hash;           
    uint8_t size;            
    uint8_t data[MAX_DATA_SIZE];
} message;

void generate_message(message *msg);

uint16_t calculate_hash(const message *msg);

#endif
