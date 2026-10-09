#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void)
{
    printf("Setting up tests...\n");
}

void tearDown(void)
{
    printf("Tearing down tests...\n");
}

static void flip_random_bit(uint8_t *buffer, size_t length, unsigned int *seed)
{
    size_t index = (size_t)(rand_r(seed) % (int)length);
    size_t bit = (size_t)(rand_r(seed) % 8u);
    buffer[index] ^= (uint8_t)(1u << bit);
}

static void test_get_greeting(void)
{
    char *greeting = get_greeting("Alice");
    TEST_ASSERT_NOT_NULL(greeting);
    TEST_ASSERT_EQUAL_STRING("Hello, Alice!", greeting);
    free(greeting);

    greeting = get_greeting(NULL);
    TEST_ASSERT_NULL(greeting);

    greeting = get_greeting("");
    TEST_ASSERT_NOT_NULL(greeting);
    TEST_ASSERT_EQUAL_STRING("Hello, !", greeting);
    free(greeting);
}

static void test_packet_checksum_and_validation(void)
{
    static const uint8_t example[] = {0x00, 0x01, 0xf2, 0x03,
                                      0xf4, 0xf5, 0xf6, 0xf7};
    static const uint8_t odd[] = {0x00, 0x01, 0x02};
    uint8_t corrupted[sizeof(example)];
    uint8_t wire[PACKET_HEADER_SIZE + 3];
    struct packet packet = {.type = PACKET_DATA, .seq = 2, .length = 3};
    struct packet decoded;
    uint8_t short_buffer[PACKET_HEADER_SIZE - 1];
    size_t encoded_length = 0;

    TEST_ASSERT_EQUAL_UINT16(0x220d, packet_checksum(example, sizeof(example)));
    TEST_ASSERT_EQUAL_UINT16(0xfdfe, packet_checksum(odd, sizeof(odd)));

    memcpy(corrupted, example, sizeof(example));
    corrupted[0] ^= 0x01u;
    TEST_ASSERT_NOT_EQUAL_UINT16(packet_checksum(example, sizeof(example)),
                                  packet_checksum(corrupted, sizeof(corrupted)));

    memcpy(packet.payload, "Hi!", 3);
    TEST_ASSERT_EQUAL_INT(0, packet_encode(&packet, wire, sizeof(wire),
                                           &encoded_length));
    TEST_ASSERT_EQUAL_UINT32(13u, (uint32_t)encoded_length);
    TEST_ASSERT_EQUAL_INT(0, packet_decode(wire, encoded_length, &decoded));
    TEST_ASSERT_EQUAL_UINT32(2u, decoded.seq);
    TEST_ASSERT_EQUAL_UINT16(3u, decoded.length);
    TEST_ASSERT_EQUAL_UINT8('H', decoded.payload[0]);
    TEST_ASSERT_EQUAL_UINT8('i', decoded.payload[1]);
    TEST_ASSERT_EQUAL_UINT8('!', decoded.payload[2]);

    wire[0] = 3;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(wire, encoded_length, &decoded));

    wire[0] = PACKET_DATA;
    wire[9] = 0x00;
    wire[10] = 0x00;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(wire, encoded_length, &decoded));

    wire[0] = PACKET_DATA;
    wire[8] = 0x00;
    wire[9] = 0x00;
    wire[10] = 0x00;
    wire[11] = 0x00;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(wire, 10, &decoded));

    TEST_ASSERT_EQUAL_INT(-1, packet_decode(short_buffer, sizeof(short_buffer),
                                           &decoded));

    memset(wire, 0, sizeof(wire));
    wire[0] = PACKET_DATA;
    wire[4] = 0x01;
    wire[8] = 0x04;
    wire[9] = 0x00;
    wire[PACKET_HEADER_SIZE - 1] = 0x42;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(wire, sizeof(wire), &decoded));
}

static void test_sender_window_and_ack_progression(void)
{
    struct sender_state sender;
    struct protocol_actions actions;
    const uint8_t first[] = {0x01, 0x02, 0x03};
    const uint8_t second[] = {0x04, 0x05, 0x06, 0x07};

    sender_init(&sender, 4, 100);
    TEST_ASSERT_EQUAL_UINT32(4u, sender_capacity(&sender));

    TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, first, sizeof(first), 0,
                                              &actions));
    TEST_ASSERT_EQUAL_UINT32(1u, sender.next);
    TEST_ASSERT_EQUAL_UINT32(1u, actions.outgoing_count);

    TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, second, sizeof(second), 10,
                                              &actions));
    TEST_ASSERT_EQUAL_UINT32(2u, sender.next);
    TEST_ASSERT_EQUAL_UINT32(1u, actions.outgoing_count);
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sender_capacity(&sender));

    sender_on_ack(&sender, 2, 20, &actions);
    TEST_ASSERT_EQUAL_UINT32(2u, sender.base);

    sender_on_ack(&sender, 1, 25, &actions);
    TEST_ASSERT_EQUAL_UINT32(2u, sender.base);

    sender_init(&sender, 1, 50);
    TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, first, sizeof(first), 0,
                                              &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, second, sizeof(second), 1,
                                             &actions));
}

static void test_sender_timeout_and_give_up(void)
{
    struct sender_state sender;
    struct protocol_actions actions;
    const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
    int i;
    int result;

    sender_init(&sender, 4, 100);
    for (i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, payload, sizeof(payload),
                                                  i * 10, &actions));
    }
    TEST_ASSERT_EQUAL_UINT32(4u, sender.next - sender.base);

    result = sender_on_timeout(&sender, 400, &actions);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT32(4u, (uint32_t)actions.outgoing_count);

    for (i = 0; i < 8; ++i) {
        result = sender_on_timeout(&sender, 500 + i * 100, &actions);
        TEST_ASSERT_EQUAL_INT(0, result);
    }
    result = sender_on_timeout(&sender, 1500, &actions);
    TEST_ASSERT_EQUAL_INT(1, result);
}

static void test_receiver_duplicate_gap_and_fin(void)
{
    struct receiver_state receiver;
    struct protocol_actions actions;
    struct packet data = {.type = PACKET_DATA, .seq = 0, .length = 3};
    struct packet fin = {.type = PACKET_FIN, .seq = 1, .length = 0};

    receiver_init(&receiver);
    memcpy(data.payload, "abc", 3);

    receiver_on_packet(&receiver, &data, 10, &actions);
    TEST_ASSERT_TRUE(actions.delivered_packet);
    TEST_ASSERT_EQUAL_UINT16(3u, actions.delivered_length);
    TEST_ASSERT_EQUAL_UINT32(1u, receiver.expected);

    receiver_on_packet(&receiver, &data, 11, &actions);
    TEST_ASSERT_EQUAL_UINT32(1u, receiver.expected);
    TEST_ASSERT_EQUAL_UINT32(1u, actions.outgoing[0].seq);

    data.seq = 2;
    receiver_on_packet(&receiver, &data, 12, &actions);
    TEST_ASSERT_EQUAL_UINT32(1u, receiver.expected);
    TEST_ASSERT_EQUAL_UINT32(1u, actions.outgoing[0].seq);

    receiver_on_packet(&receiver, &fin, 20, &actions);
    TEST_ASSERT_TRUE(receiver_is_finished(&receiver));
    TEST_ASSERT_EQUAL_UINT32(2u, receiver.expected);
    TEST_ASSERT_TRUE(receiver_linger_expired(&receiver, 2021));

    receiver_on_packet(&receiver, &fin, 25, &actions);
    TEST_ASSERT_EQUAL_UINT32(2u, receiver.expected);
}

static void test_edge_cases_and_invalid_inputs(void)
{
    struct sender_state sender;
    struct receiver_state receiver;
    struct protocol_actions actions;
    struct packet packet = {.type = PACKET_DATA, .seq = 7, .length = 1};
    struct packet decoded;
    uint8_t buffer[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    uint8_t data = 0x55;
    size_t encoded_length = 0;

    protocol_actions_reset(NULL);
    protocol_actions_reset(&actions);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)actions.outgoing_count);
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(NULL, &data, 1, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, NULL, 1, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, &data, 0, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, &data,
                                               PACKET_MAX_PAYLOAD + 1u, 0,
                                               &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, &data, 1, 0, NULL));
    sender_init(&sender, 1, 10);
    TEST_ASSERT_EQUAL_INT(0, sender_finish(&sender, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_push_data(&sender, &data, 1, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_finish(&sender, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_finish(NULL, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_finish(&sender, 0, NULL));
    TEST_ASSERT_EQUAL_INT(0, sender_on_timeout(&sender, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_on_timeout(NULL, 0, &actions));
    TEST_ASSERT_EQUAL_INT(-1, sender_on_timeout(&sender, 0, NULL));
    sender_init(&sender, 1, 10);
    TEST_ASSERT_EQUAL_INT(0, sender_on_timeout(&sender, 0, &actions));
    sender_init(&sender, PROTOCOL_MAX_WINDOW, 10);
    sender.next = PROTOCOL_MAX_WINDOW + 1u;
    sender_on_ack(&sender, 0, 0, &actions);
    sender_on_ack(&sender, PROTOCOL_MAX_WINDOW + 2u, 0, &actions);
    sender_on_ack(NULL, 1, 0, &actions);
    sender_on_ack(&sender, 1, 0, NULL);
    TEST_ASSERT_FALSE(sender_is_complete(NULL));

    receiver_init(&receiver);
    receiver_on_packet(NULL, &packet, 0, &actions);
    receiver_on_packet(&receiver, NULL, 0, &actions);
    receiver_on_packet(&receiver, &packet, 0, NULL);
    packet.type = PACKET_ACK;
    receiver_on_packet(&receiver, &packet, 0, &actions);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)actions.outgoing[0].seq);
    packet.type = PACKET_FIN;
    packet.seq = 9;
    receiver_on_packet(&receiver, &packet, 0, &actions);
    packet.type = PACKET_DATA;
    packet.seq = 0;
    receiver_on_packet(&receiver, &packet, 0, &actions);
    TEST_ASSERT_FALSE(receiver_linger_expired(&receiver, 0));
    receiver.finished = 1;
    receiver.expected = 10;
    receiver_on_packet(&receiver, &packet, 0, &actions);
    receiver.linger_until = 100;
    TEST_ASSERT_FALSE(receiver_is_finished(NULL));
    TEST_ASSERT_FALSE(receiver_linger_expired(NULL, 0));
    TEST_ASSERT_FALSE(receiver_linger_expired(&receiver, 0));

    TEST_ASSERT_EQUAL_INT(-1, packet_encode(NULL, buffer, sizeof(buffer),
                                            &encoded_length));
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, NULL, sizeof(buffer),
                                            &encoded_length));
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, buffer, sizeof(buffer),
                                            NULL));
    packet.type = 3;
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, buffer, sizeof(buffer),
                                            &encoded_length));
    packet.type = PACKET_DATA;
    packet.length = PACKET_MAX_PAYLOAD + 1u;
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, buffer, sizeof(buffer),
                                            &encoded_length));
    packet.type = PACKET_ACK;
    packet.length = 1;
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, buffer, sizeof(buffer),
                                            &encoded_length));
    packet.length = 0;
    TEST_ASSERT_EQUAL_INT(-1, packet_encode(&packet, buffer, PACKET_HEADER_SIZE - 1,
                                            &encoded_length));
    TEST_ASSERT_EQUAL_INT(0, packet_encode(&packet, buffer, sizeof(buffer),
                                           &encoded_length));

    TEST_ASSERT_EQUAL_INT(-1, packet_decode(NULL, encoded_length, &decoded));
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, encoded_length, NULL));
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, PACKET_HEADER_SIZE - 1,
                                            &decoded));
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, sizeof(buffer) + 1,
                                            &decoded));
    buffer[0] = 3;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, encoded_length, &decoded));
    buffer[0] = PACKET_ACK;
    buffer[1] = 1;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, encoded_length, &decoded));

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = PACKET_ACK;
    buffer[8] = 0x04;
    buffer[9] = 0x01;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, PACKET_HEADER_SIZE + 1,
                                            &decoded));
    buffer[8] = 0x04;
    buffer[9] = 0x01;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, sizeof(buffer), &decoded));
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = PACKET_ACK;
    buffer[9] = 1;
    TEST_ASSERT_EQUAL_INT(-1, packet_decode(buffer, PACKET_HEADER_SIZE + 1,
                                            &decoded));
}

static int deliver_with_loss_and_damage(const struct packet *packet,
                                       struct packet *queue,
                                       size_t *queue_count,
                                       unsigned int *seed,
                                       int drop_percent,
                                       int corrupt_percent,
                                       int duplicate_percent)
{
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    size_t encoded_length = 0;
    struct packet decoded;

    if (packet == NULL || queue == NULL || queue_count == NULL || seed == NULL) {
        return -1;
    }
    if (packet_encode(packet, wire, sizeof(wire), &encoded_length) != 0) {
        return -1;
    }
    if ((rand_r(seed) % 100) < drop_percent) {
        return 0;
    }
    if ((rand_r(seed) % 100) < corrupt_percent) {
        flip_random_bit(wire, encoded_length, seed);
    }
    if (packet_decode(wire, encoded_length, &decoded) != 0) {
        return 0;
    }
    queue[(*queue_count)++] = decoded;
    if ((rand_r(seed) % 100) < duplicate_percent) {
        queue[(*queue_count)++] = decoded;
    }
    return 1;
}

static void test_end_to_end_seeded_lossy_channel(void)
{
    struct sender_state sender;
    struct receiver_state receiver;
    struct protocol_actions sender_actions;
    struct protocol_actions receiver_actions;
    struct packet sender_queue[128];
    struct packet receiver_queue[128];
    uint8_t payload[4096];
    uint8_t received[sizeof(payload)];
    size_t offset = 0;
    size_t sender_queue_count = 0;
    size_t receiver_queue_count = 0;
    size_t received_count = 0;
    unsigned int seed = 0xC0FFEEu;
    int64_t now = 0;
    int step;

    for (step = 0; step < (int)sizeof(payload); ++step) {
        payload[step] = (uint8_t)((step * 17u) & 0xFFu);
    }

    sender_init(&sender, 4, 50);
    receiver_init(&receiver);

    for (step = 0; step < 2000 && (!sender_is_complete(&sender) ||
                                    !receiver_is_finished(&receiver)); ++step) {
        while (offset < sizeof(payload) && sender_capacity(&sender) > 0) {
            size_t chunk = sizeof(payload) - offset;
            if (chunk > PACKET_MAX_PAYLOAD) {
                chunk = PACKET_MAX_PAYLOAD;
            }
            TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, payload + offset,
                                                      (uint16_t)chunk, now,
                                                      &sender_actions));
            for (int i = 0; i < (int)sender_actions.outgoing_count; ++i) {
                sender_queue[sender_queue_count++] = sender_actions.outgoing[i];
            }
            offset += chunk;
        }
        if (offset == sizeof(payload) && !sender.input_finished) {
            TEST_ASSERT_EQUAL_INT(0, sender_finish(&sender, now, &sender_actions));
            for (int i = 0; i < (int)sender_actions.outgoing_count; ++i) {
                sender_queue[sender_queue_count++] = sender_actions.outgoing[i];
            }
        }

        for (size_t i = 0; i < sender_queue_count; ++i) {
            if (deliver_with_loss_and_damage(&sender_queue[i], receiver_queue,
                                            &receiver_queue_count, &seed,
                                            20, 20, 20) == 0) {
                continue;
            }
        }
        sender_queue_count = 0;

        for (size_t i = 0; i < receiver_queue_count; ++i) {
            receiver_on_packet(&receiver, &receiver_queue[i], now,
                              &receiver_actions);
            if (receiver_actions.delivered_packet) {
                memcpy(received + received_count, receiver_actions.delivered,
                       receiver_actions.delivered_length);
                received_count += receiver_actions.delivered_length;
            }
            for (int j = 0; j < (int)receiver_actions.outgoing_count; ++j) {
                sender_queue[sender_queue_count++] = receiver_actions.outgoing[j];
            }
        }
        receiver_queue_count = 0;

        for (size_t i = 0; i < sender_queue_count; ++i) {
            if (deliver_with_loss_and_damage(&sender_queue[i], receiver_queue,
                                            &receiver_queue_count, &seed,
                                            20, 20, 20) == 0) {
                continue;
            }
        }
        sender_queue_count = 0;

        for (size_t i = 0; i < receiver_queue_count; ++i) {
            if (receiver_queue[i].type == PACKET_ACK ||
                receiver_queue[i].type == PACKET_FIN) {
                sender_on_ack(&sender, receiver_queue[i].seq, now,
                              &sender_actions);
                for (int j = 0; j < (int)sender_actions.outgoing_count; ++j) {
                    sender_queue[sender_queue_count++] = sender_actions.outgoing[j];
                }
            }
        }
        receiver_queue_count = 0;

        if (sender.base < sender.next && sender.timer_due < 0) {
            sender.timer_due = now + sender.timeout_ms;
        }
        if (sender.base < sender.next && now >= sender.timer_due) {
            int timeout_result = sender_on_timeout(&sender, now, &sender_actions);
            TEST_ASSERT_TRUE(timeout_result == 0 || timeout_result == 1);
            if (timeout_result == 1) {
                TEST_ASSERT_MESSAGE(0, "sender gave up unexpectedly");
            }
            for (int i = 0; i < (int)sender_actions.outgoing_count; ++i) {
                sender_queue[sender_queue_count++] = sender_actions.outgoing[i];
            }
            sender.timer_due = now + sender.timeout_ms;
        }

        if (receiver_is_finished(&receiver) && receiver_linger_expired(&receiver, now)) {
            break;
        }
        now += 10;
    }

    TEST_ASSERT_TRUE(sender_is_complete(&sender));
    TEST_ASSERT_TRUE(receiver_is_finished(&receiver));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(payload), (uint32_t)received_count);
    TEST_ASSERT_EQUAL_MEMORY(payload, received, sizeof(payload));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_greeting);
    RUN_TEST(test_packet_checksum_and_validation);
    RUN_TEST(test_sender_window_and_ack_progression);
    RUN_TEST(test_sender_timeout_and_give_up);
    RUN_TEST(test_receiver_duplicate_gap_and_fin);
    RUN_TEST(test_edge_cases_and_invalid_inputs);
    RUN_TEST(test_end_to_end_seeded_lossy_channel);
    return UNITY_END();
}
