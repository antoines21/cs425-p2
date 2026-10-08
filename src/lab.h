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
enum { PROTOCOL_MAX_WINDOW = 64 };

struct packet {
    uint8_t type;
    uint32_t seq;
    uint16_t length;
    uint8_t payload[PACKET_MAX_PAYLOAD];
};

struct protocol_actions {
    struct packet outgoing[PROTOCOL_MAX_WINDOW];
    size_t outgoing_count;
    uint8_t delivered[PACKET_MAX_PAYLOAD];
    uint16_t delivered_length;
    int delivered_packet;
    int64_t timer_due;
};

struct sender_state {
    struct packet outstanding[PROTOCOL_MAX_WINDOW];
    uint32_t base;
    uint32_t next;
    uint32_t sent_through;
    uint32_t window_size;
    int64_t timeout_ms;
    int input_finished;
    int fin_sent;
    int complete;
    unsigned consecutive_timeouts;
    int64_t timer_due;
};

struct receiver_state {
    uint32_t expected;
    int finished;
    int64_t linger_until;
};

uint16_t packet_checksum(const uint8_t *data, size_t length);
int packet_encode(const struct packet *packet, uint8_t *buffer,
                  size_t capacity, size_t *encoded_length);
int packet_decode(const uint8_t *buffer, size_t length, struct packet *packet);

void protocol_actions_reset(struct protocol_actions *actions);
void sender_init(struct sender_state *sender, uint32_t window_size,
                 int64_t timeout_ms);
size_t sender_capacity(const struct sender_state *sender);
int sender_push_data(struct sender_state *sender, const uint8_t *data,
                     uint16_t length, int64_t now,
                     struct protocol_actions *actions);
int sender_finish(struct sender_state *sender, int64_t now,
                  struct protocol_actions *actions);
void sender_on_ack(struct sender_state *sender, uint32_t sequence,
                   int64_t now, struct protocol_actions *actions);
int sender_on_timeout(struct sender_state *sender, int64_t now,
                      struct protocol_actions *actions);
int sender_is_complete(const struct sender_state *sender);
void receiver_init(struct receiver_state *receiver);
void receiver_on_packet(struct receiver_state *receiver,
                        const struct packet *packet, int64_t now,
                        struct protocol_actions *actions);
int receiver_is_finished(const struct receiver_state *receiver);
int receiver_linger_expired(const struct receiver_state *receiver,
                            int64_t now);

char *get_greeting(const char *restrict name);

#endif
