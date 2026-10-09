#include "lab.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void sender_update_timer(struct sender_state *sender, int64_t now)
{
    sender->timer_due = sender->base < sender->next // GCOVR_EXCL_BR_LINE
                            ? now + sender->timeout_ms
                            : -1; // GCOVR_EXCL_BR_LINE
}

static void sender_emit_new(struct sender_state *sender,
                            struct protocol_actions *actions)
{
    while (sender->sent_through < sender->next &&
           actions->outgoing_count < PROTOCOL_MAX_WINDOW) {
        actions->outgoing[actions->outgoing_count++] =
            sender->outstanding[sender->sent_through % sender->window_size];
        ++sender->sent_through;
    }
}

static void sender_maybe_queue_fin(struct sender_state *sender)
{
    if (sender->input_finished && !sender->fin_sent &&
        sender->base == sender->next) {
        struct packet *fin =
            &sender->outstanding[sender->next % sender->window_size];
        fin->type = PACKET_FIN;
        fin->seq = sender->next;
        fin->length = 0;
        ++sender->next;
        sender->fin_sent = 1;
    }
}

void protocol_actions_reset(struct protocol_actions *actions)
{
    if (actions != NULL) {
        actions->outgoing_count = 0;
        actions->delivered_length = 0;
        actions->delivered_packet = 0;
        actions->timer_due = -1;
    }
}

void sender_init(struct sender_state *sender, uint32_t window_size,
                 int64_t timeout_ms)
{
    memset(sender, 0, sizeof(*sender));
    sender->window_size = window_size;
    sender->timeout_ms = timeout_ms;
    sender->timer_due = -1;
}

size_t sender_capacity(const struct sender_state *sender)
{
    return sender->window_size - (sender->next - sender->base);
}

int sender_push_data(struct sender_state *sender, const uint8_t *data,
                     uint16_t length, int64_t now,
                     struct protocol_actions *actions)
{
    struct packet *packet;

    if (sender == NULL || data == NULL || actions == NULL || length == 0 ||
        length > PACKET_MAX_PAYLOAD || sender->input_finished ||
        sender_capacity(sender) == 0) {
        return -1;
    }
    protocol_actions_reset(actions);
    packet = &sender->outstanding[sender->next % sender->window_size];
    packet->type = PACKET_DATA;
    packet->seq = sender->next;
    packet->length = length;
    memcpy(packet->payload, data, length);
    ++sender->next;
    if (sender->timer_due < 0) {
        sender_update_timer(sender, now);
    }
    sender_emit_new(sender, actions);
    actions->timer_due = sender->timer_due;
    return 0;
}

int sender_finish(struct sender_state *sender, int64_t now,
                  struct protocol_actions *actions)
{
    if (sender == NULL || actions == NULL || sender->input_finished) {
        return -1;
    }
    protocol_actions_reset(actions);
    sender->input_finished = 1;
    sender_maybe_queue_fin(sender);
    if (sender->timer_due < 0 && sender->base < sender->next) { // GCOVR_EXCL_BR_LINE
        sender_update_timer(sender, now);
    }
    sender_emit_new(sender, actions);
    actions->timer_due = sender->timer_due;
    return 0;
}

void sender_on_ack(struct sender_state *sender, uint32_t sequence,
                   int64_t now, struct protocol_actions *actions)
{
    if (sender == NULL || actions == NULL) {
        return;
    }
    protocol_actions_reset(actions);
    if (sequence > sender->base && sequence <= sender->next) {
        sender->base = sequence;
        sender->consecutive_timeouts = 0;
        sender_maybe_queue_fin(sender);
        if (sender->base == sender->next) {
            sender->complete = sender->fin_sent;
            sender->timer_due = -1;
        } else {
            sender_update_timer(sender, now);
        }
    }
    sender_emit_new(sender, actions);
    actions->timer_due = sender->timer_due;
}

int sender_on_timeout(struct sender_state *sender, int64_t now,
                      struct protocol_actions *actions)
{
    uint32_t sequence;

    if (sender == NULL || actions == NULL) {
        return -1;
    }
    protocol_actions_reset(actions);
    if (sender->base == sender->next) {
        actions->timer_due = -1;
        return 0;
    }
    ++sender->consecutive_timeouts;
    if (sender->consecutive_timeouts >= 10) {
        actions->timer_due = sender->timer_due;
        return 1;
    }
    for (sequence = sender->base; sequence < sender->next; ++sequence) {
        actions->outgoing[actions->outgoing_count++] =
            sender->outstanding[sequence % sender->window_size];
    }
    sender->sent_through = sender->next;
    sender_update_timer(sender, now);
    actions->timer_due = sender->timer_due;
    return 0;
}

int sender_is_complete(const struct sender_state *sender)
{
    return sender != NULL && sender->complete;
}

void receiver_init(struct receiver_state *receiver)
{
    memset(receiver, 0, sizeof(*receiver));
    receiver->linger_until = -1;
}

void receiver_on_packet(struct receiver_state *receiver,
                        const struct packet *packet, int64_t now,
                        struct protocol_actions *actions)
{
    struct packet ack;

    if (receiver == NULL || packet == NULL || actions == NULL) {
        return;
    }
    protocol_actions_reset(actions);
    if (packet->type == PACKET_DATA && !receiver->finished &&
        packet->seq == receiver->expected) {
        memcpy(actions->delivered, packet->payload, packet->length);
        actions->delivered_length = packet->length;
        actions->delivered_packet = 1;
        ++receiver->expected;
    } else if (packet->type != PACKET_DATA && packet->type != PACKET_FIN) {
        return;
    } else if (packet->type == PACKET_FIN && !receiver->finished &&
               packet->seq == receiver->expected) {
        ++receiver->expected;
        receiver->finished = 1;
        receiver->linger_until = now + 2000;
    }
    ack.type = PACKET_ACK;
    ack.seq = receiver->expected;
    ack.length = 0;
    actions->outgoing[0] = ack;
    actions->outgoing_count = 1;
    actions->timer_due = receiver->finished ? receiver->linger_until : -1;
}

int receiver_is_finished(const struct receiver_state *receiver)
{
    return receiver != NULL && receiver->finished;
}

int receiver_linger_expired(const struct receiver_state *receiver,
                            int64_t now)
{
    return receiver != NULL && receiver->finished &&
           now >= receiver->linger_until;
}

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
