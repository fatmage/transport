#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <errno.h>
#include <string.h>
#include <math.h>


typedef char byte;



#define CHUNK_SIZE 700
#define CHUNK_LIMIT 3000
#define PACK_SIZE 500
#define REPEATS 3


#define PENDING 0
#define RECEIVED 1

#define INDEX(start,received,chunk_size) (((start)-(received))/(chunk_size))
#define START(start,i) ((start) + (i)*CHUNK_SIZE)


typedef struct {
    uint32_t start;
    uint16_t size;
    uint8_t received;
} chunk_t;

const char *request(uint32_t start, uint16_t size);
void parse_datagram(void *data, uint32_t *start, uint16_t *size);









#endif