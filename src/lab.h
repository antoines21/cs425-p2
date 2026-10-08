#ifndef LAB_H
#define LAB_H

#include <stddef.h>
#include <stdint.h>

enum packet_type {
    PACKET_DATA = 0,
    PACKET_ACK = 1,
    PACKET_FIN = 2
};

enum { PACKET_HEADER_SIZE = 10, PACKET_MAX_PAYLOAD = 1024 };

struct packet {
    uint8_t type;
    uint32_t seq;
    uint16_t length;
    uint8_t payload[PACKET_MAX_PAYLOAD];
};

uint16_t packet_checksum(const uint8_t *data, size_t length);
int packet_encode(const struct packet *packet, uint8_t *buffer,
                  size_t capacity, size_t *encoded_length);
int packet_decode(const uint8_t *buffer, size_t length, struct packet *packet);

char *get_greeting(const char *restrict name);

#endif
