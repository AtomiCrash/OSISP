#include "message.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint16_t calculate_hash(const message *msg) 
{
    uint16_t hash = 0;
    hash += msg->type;
    for (int i = 0; i < msg->size; i++) 
    {
        hash += msg->data[i];
    }
    return hash;
}

void generate_message(message *msg) 
{
    msg->type = rand() % 256;           
    msg->size = rand() % 256;  

    if (msg->size == 0) 
    {
        memset(msg->data, 0, MAX_DATA_SIZE);
    } 
    else 
    {
        for (int i = 0; i < msg->size; i++)
        {
            msg->data[i] = rand() % 256;
        }
    }

    msg->hash = 0;
    msg->hash = calculate_hash(msg);
}

