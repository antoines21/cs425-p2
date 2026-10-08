#include <stdlib.h>
#include <stdio.h>
#include "harness/unity.h"
#include "../src/lab.h"


void setUp(void) {
  printf("Setting up tests...\n");
}

void tearDown(void) {
  printf("Tearing down tests...\n");
}

void test_get_greeting(void) {
  char *greeting = get_greeting("Alice");
  TEST_ASSERT_NOT_NULL(greeting);
  TEST_ASSERT_EQUAL_STRING("Hello, Alice!", greeting);
  free(greeting); // Free the allocated memory for the greeting

  greeting = get_greeting(NULL);
  TEST_ASSERT_NULL(greeting);

  greeting = get_greeting("");
  TEST_ASSERT_NOT_NULL(greeting);
  TEST_ASSERT_EQUAL_STRING("Hello, !", greeting);
  free(greeting);
}

void test_packet_round_trip(void) {
  struct packet packet = {.type = PACKET_DATA, .seq = 7, .length = 3,
                          .payload = {'H', 'i', '!'}};
  struct packet decoded;
  uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
  size_t wire_length = 0;

  TEST_ASSERT_EQUAL_INT(0, packet_encode(&packet, wire, sizeof(wire),
                                         &wire_length));
  TEST_ASSERT_EQUAL_INT(13, (int)wire_length);
  TEST_ASSERT_EQUAL_INT(0, packet_decode(wire, wire_length, &decoded));
  TEST_ASSERT_EQUAL_INT(PACKET_DATA, decoded.type);
  TEST_ASSERT_EQUAL_UINT32(7, decoded.seq);
  TEST_ASSERT_EQUAL_UINT16(3, decoded.length);
  TEST_ASSERT_EQUAL_MEMORY(packet.payload, decoded.payload, 3);
}

void test_sender_and_receiver_state_machines(void) {
  struct sender_state sender;
  struct receiver_state receiver;
  struct protocol_actions actions;
  const uint8_t data[] = {'a', 'b', 'c'};
  struct packet data_packet;
  struct packet ack;

  sender_init(&sender, 2, 100);
  receiver_init(&receiver);
  TEST_ASSERT_EQUAL_UINT32(2, sender_capacity(&sender));
  TEST_ASSERT_EQUAL_INT(0, sender_push_data(&sender, data, sizeof(data), 0,
                                            &actions));
  TEST_ASSERT_EQUAL_UINT32(1, sender.next);
  TEST_ASSERT_EQUAL_UINT32(1, actions.outgoing_count);
  data_packet = actions.outgoing[0];
  receiver_on_packet(&receiver, &data_packet, 10, &actions);
  TEST_ASSERT_TRUE(actions.delivered_packet);
  TEST_ASSERT_EQUAL_UINT16(sizeof(data), actions.delivered_length);
  ack = actions.outgoing[0];
  sender_on_ack(&sender, ack.seq, 20, &actions);
  TEST_ASSERT_EQUAL_UINT32(1, sender.base);
  TEST_ASSERT_FALSE(sender_is_complete(&sender));
  TEST_ASSERT_EQUAL_INT(0, sender_finish(&sender, 20, &actions));
  TEST_ASSERT_EQUAL_INT(PACKET_FIN, actions.outgoing[0].type);
  receiver_on_packet(&receiver, &actions.outgoing[0], 30, &actions);
  sender_on_ack(&sender, actions.outgoing[0].seq, 40, &actions);
  TEST_ASSERT_TRUE(sender_is_complete(&sender));
  TEST_ASSERT_TRUE(receiver_is_finished(&receiver));
  TEST_ASSERT_FALSE(receiver_linger_expired(&receiver, 100));
  TEST_ASSERT_TRUE(receiver_linger_expired(&receiver, 2030));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_get_greeting);
  RUN_TEST(test_packet_round_trip);
  RUN_TEST(test_sender_and_receiver_state_machines);
  return UNITY_END();
}
