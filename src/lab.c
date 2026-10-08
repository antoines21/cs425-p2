#include "lab.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t packet_checksum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    size_t index = 0;

    while (index + 1 < length) {
        sum += ((uint16_t)data[index] << 8) | data[index + 1];
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
        index += 2;
    }
    if (index < length) {
        sum += (uint16_t)data[index] << 8;
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    while (sum >> 16) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

int packet_encode(const struct packet *packet, uint8_t *buffer,
                  size_t capacity, size_t *encoded_length)
{
    size_t length;
    uint32_t sequence;
    uint16_t payload_length;

    if (packet == NULL || buffer == NULL || encoded_length == NULL ||
        packet->type > PACKET_FIN || packet->length > PACKET_MAX_PAYLOAD ||
        (packet->type != PACKET_DATA && packet->length != 0)) {
        return -1;
    }
    length = PACKET_HEADER_SIZE + packet->length;
    if (capacity < length) {
        return -1;
    }
    memset(buffer, 0, length);
    buffer[0] = packet->type;
    sequence = htonl(packet->seq);
    payload_length = htons(packet->length);
    memcpy(buffer + 4, &sequence, sizeof(sequence));
    memcpy(buffer + 8, &payload_length, sizeof(payload_length));
    memcpy(buffer + PACKET_HEADER_SIZE, packet->payload, packet->length);
    payload_length = htons(packet_checksum(buffer, length));
    memcpy(buffer + 2, &payload_length, sizeof(payload_length));
    *encoded_length = length;
    return 0;
}

int packet_decode(const uint8_t *buffer, size_t length, struct packet *packet)
{
    uint16_t wire_checksum;
    uint16_t payload_length;
    uint8_t copy[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];

    if (buffer == NULL || packet == NULL || length < PACKET_HEADER_SIZE ||
        length > sizeof(copy) || buffer[0] > PACKET_FIN || buffer[1] != 0) {
        return -1;
    }
    memcpy(&payload_length, buffer + 8, sizeof(payload_length));
    payload_length = ntohs(payload_length);
    if (payload_length > PACKET_MAX_PAYLOAD ||
        (buffer[0] != PACKET_DATA && payload_length != 0) ||
        (size_t)PACKET_HEADER_SIZE + payload_length != length) {
        return -1;
    }
    memcpy(copy, buffer, length);
    memcpy(&wire_checksum, copy + 2, sizeof(wire_checksum));
    memset(copy + 2, 0, sizeof(wire_checksum));
    if (packet_checksum(copy, length) != ntohs(wire_checksum)) {
        return -1;
    }
    packet->type = buffer[0];
    memcpy(&packet->seq, buffer + 4, sizeof(packet->seq));
    packet->seq = ntohl(packet->seq);
    packet->length = payload_length;
    memcpy(packet->payload, buffer + PACKET_HEADER_SIZE, payload_length);
    return 0;
}

char *get_greeting(const char *restrict name)
{
  if (name == NULL)
  {
    return NULL;
  }

  // Allocate memory for the greeting message
  int length = snprintf(NULL, 0, "Hello, %s!", name);
  if (length < 0) // GCOVR_EXCL_START
  {
    return NULL; // snprintf failed
  } // GCOVR_EXCL_STOP

  //Casting is safe here because we know length is non-negative
  size_t alloc_size = (size_t) length + 1; // +1 for the null terminator
  char *greeting = malloc( alloc_size);


  if (greeting == NULL) // GCOVR_EXCL_START
  {
    return NULL; // Memory allocation failed
  }  // GCOVR_EXCL_STOP


  // Create the greeting message
  snprintf(greeting, alloc_size, "Hello, %s!", name);

  return greeting;
}
