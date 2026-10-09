# Submission Report

- Submission generated at 10/09/2026 at 21:49:20

- Machine info: Linux runnervmmprz5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

# Project 2 - Reliable Data Transfer

- Name: Antoine Sabatier
- Email: antoinesabatier@u.boisestate.edu
- Class: CS425-001

## Known Bugs or Issues

No known bugs remain after the coverage, leak, crash, and lossy-transfer
checks. The receiver treats the expected post-transfer linger timeout as a
successful completion.

## Experience

The most challenging part of the project was making Go-Back-N reliable while
also keeping the sender responsive to both acknowledgements and timer
expiration. Cumulative acknowledgements simplified normal progress, but
duplicate acknowledgements, gaps, retransmissions, and the FIN packet required
careful state transitions. The fixed-seed in-memory lossy-channel test was
particularly useful because it reproduced loss, corruption, and duplication
without depending on network timing.

The window measurements made the protocol behavior concrete. With a 100 ms
round trip, a window of one spends nearly all of its time waiting for the next
acknowledgement. A larger window keeps packets in flight and approaches the
ideal bandwidth-delay-product improvement. The leak and crash checks also
helped identify that a receiver completing its linger period should return
success rather than report a timeout failure.

## Design

The implementation is divided into three layers so that protocol behavior can
be tested independently from operating-system and network behavior.

1. **Packets.** The packet layer contains the checksum, encoding, and decoding
   functions. These functions accept byte buffers or packet structures and
   validate packet type, payload length, size, and checksum. They do not use
   sockets, clocks, or files, so malformed and corrupted datagrams can be
   tested deterministically.
2. **Go-Back-N state machines.** The sender and receiver state are represented
   by structs and manipulated through event-oriented functions. The sender
   tracks `base`, `next`, the outstanding retransmission window, cumulative
   acknowledgements, FIN state, and one retransmission timer. The receiver
   tracks the next expected sequence number, delivers only the next
   in-order DATA packet, acknowledges duplicates or packets beyond a gap with
   the current cumulative acknowledgement, and enters a short FIN linger
   period. The current time is passed into these functions rather than read
   internally, and each call reports packets to send, payload to deliver, and
   the next timer deadline.
3. **I/O.** The application layer owns the UDP socket, relay registration,
   `poll`, the monotonic clock, and file I/O. It reads a datagram or timer
   event, passes it to the appropriate state machine, then sends the returned
   packets or writes the returned payload. This layer is intentionally thin
   and is the only layer that depends on the operating system.

This separation makes the important protocol logic deterministic and
testable. Unit tests use supplied timestamps and an in-memory channel, so a
lost packet is simply omitted, a corrupted packet is rejected by the packet
layer, and a timeout is represented by advancing the test timestamp. The
production program uses the same state machines with real sockets and a real
monotonic clock.

## Results

I created a 1 MiB input file and ran each configuration three times through
`cs425_relay.py --delay 50`, using the default 250 ms timeout. Every received
file was checked with `cmp`. The throughput values use 1024 KiB divided by
the mean sender wall-clock time.

| Window | Loss | Corrupt | Dup | Mean time (s) | Throughput (KiB/s) |
|--------|------|---------|-----|---------------|--------------------|
| 1      | 0    | 0       | 0   | 103.342       | 9.91               |
| 16     | 0    | 0       | 0   | 6.575         | 155.74             |
| 1      | 0.05 | 0      | 0   | 130.421       | 7.85               |
| 16     | 0.05 | 0      | 0   | 22.482        | 45.55              |

With a window of 1 and no loss, the sender took an average of 103.342 seconds
for 1025 packets, so the round-trip time it observed was approximately
`103.342 / 1025 = 0.1008 seconds`, or 100.8 ms. The relay contributes 100 ms
of that delay. The remaining roughly 0.8 ms comes from local scheduling,
socket processing, packet encoding/decoding, and the sender's timing and
polling overhead.

Increasing the window to 16 lets the sender keep multiple packets in flight
instead of waiting for one round trip after every packet. The no-loss transfer
speedup was `103.342 / 6.575 = 15.72x`, which is close to the ideal 16x. It is
not exactly 16x because the transfer has startup and shutdown/final-window
overhead, and the relay and endpoint still need to process each packet.

Loss affects the window-16 transfer much more than the window-1 transfer
relative to their respective no-loss times. With a window of 1, a timeout
causes only the one unacknowledged packet to be sent again. With Go-Back-N and
a window of 16, a timeout causes the sender to resend the outstanding packet
and the later packets in that window. Packets after a missing packet may also
be discarded by the receiver, so a single loss can force a larger burst of
retransmissions and additional round trips. This increased the measured time
from 6.575 s to 22.482 s for window 16, while window 1 increased from
103.342 s to 130.421 s.

## Testing

The project was validated with:

```bash
make check
make report
make leak
make leak-test
```

The test suite passes all seven tests, and `make report` reports 100% line
coverage for `src/lab.c` (191 of 191 lines). The Task 6 transfers also
produced byte-identical copies for all 12 runs.

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/lab.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
Setting up tests...
Tearing down tests...
tests/lab-test.c:462:test_get_greeting:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:463:test_packet_checksum_and_validation:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:464:test_sender_window_and_ack_progression:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:465:test_sender_timeout_and_give_up:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:466:test_receiver_duplicate_gap_and_fin:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:467:test_edge_cases_and_invalid_inputs:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:468:test_end_to_end_seeded_lossy_channel:PASS

-----------------------
7 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
Setting up tests...
Tearing down tests...
tests/lab-test.c:462:test_get_greeting:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:463:test_packet_checksum_and_validation:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:464:test_sender_window_and_ack_progression:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:465:test_sender_timeout_and_give_up:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:466:test_receiver_duplicate_gap_and_fin:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:467:test_edge_cases_and_invalid_inputs:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:468:test_end_to_end_seeded_lossy_channel:PASS

-----------------------
7 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    191      191   100%
------------------------------------------------------------------------------
TOTAL                                        191      191   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
Setting up tests...
Tearing down tests...
tests/lab-test.c:462:test_get_greeting:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:463:test_packet_checksum_and_validation:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:464:test_sender_window_and_ack_progression:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:465:test_sender_timeout_and_give_up:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:466:test_receiver_duplicate_gap_and_fin:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:467:test_edge_cases_and_invalid_inputs:PASS
Setting up tests...
Tearing down tests...
tests/lab-test.c:468:test_end_to_end_seeded_lossy_channel:PASS

-----------------------
7 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

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

```

### lab.h

```c

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

```

### main.c

```c

#include "lab.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

static void print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
            "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
            "       myapp recv -s <session> [-p port] <relay> <file>\n"
            "\n"
            "  -s <session>     session name shared by the sender and the receiver\n"
            "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
            "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
            "  -l <loss>        probability the relay drops a packet (default: 0)\n"
            "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
            "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
            "  -p <port>        relay port (default: 4250)\n"
            "  <relay>          host name or address of the relay\n"
            "  <file>           file to send, or file to write what is received\n");
}

static int parse_long_value(const char *text, long min, long max, long *value)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min ||
        parsed > max) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static int parse_probability(const char *text, double *value)
{
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || parsed < 0.0 ||
        parsed > 1.0) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static int64_t monotonic_milliseconds(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * INT64_C(1000) +
           now.tv_nsec / INT64_C(1000000);
}

static int wait_for_socket(int socket_fd, int timeout_ms)
{
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN};
    int result;

    do {
        result = poll(&descriptor, 1, timeout_ms);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int register_receiver(int socket_fd, const char *session)
{
    char hello[64];
    char response[128];
    int attempt;

    if (snprintf(hello, sizeof(hello), "HELLO %s recv", session) < 0) {
        return -1;
    }
    for (attempt = 0; attempt < 5; ++attempt) {
        size_t hello_length = strlen(hello);
        ssize_t sent = send(socket_fd, hello, hello_length, 0);
        if (sent < 0 || (size_t)sent != hello_length) {
            return -1;
        }
        if (wait_for_socket(socket_fd, 1000) > 0) {
            ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
            if (received < 0) {
                return -1;
            }
            response[received] = '\0';
            if (strcmp(response, "OK") == 0) {
                return 0;
            }
            fprintf(stderr, "relay refused receiver: %s\n", response);
            return -1;
        }
    }
    fprintf(stderr, "relay did not answer receiver registration\n");
    return -1;
}

static int register_sender(int socket_fd, const char *session, double loss,
                           double corrupt, double duplicate)
{
    char hello[128];
    char response[128];
    int attempt;

    if (snprintf(hello, sizeof(hello), "HELLO %s send %g %g %g", session,
                 loss, corrupt, duplicate) < 0) {
        return -1;
    }
    for (attempt = 0; attempt < 5; ++attempt) {
        size_t hello_length = strlen(hello);
        ssize_t sent = send(socket_fd, hello, hello_length, 0);
        if (sent < 0 || (size_t)sent != hello_length) {
            return -1;
        }
        if (wait_for_socket(socket_fd, 1000) > 0) {
            ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
            if (received < 0) {
                return -1;
            }
            response[received] = '\0';
            if (strcmp(response, "OK") == 0) {
                return 0;
            }
            fprintf(stderr, "relay refused sender: %s\n", response);
            return -1;
        }
    }
    fprintf(stderr, "relay did not answer sender registration\n");
    return -1;
}

static int send_packet(int socket_fd, const struct packet *packet)
{
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    size_t wire_length;

    if (packet_encode(packet, wire, sizeof(wire), &wire_length) != 0 ||
        send(socket_fd, wire, wire_length, 0) != (ssize_t)wire_length) {
        return -1;
    }
    return 0;
}

static int send_file(int socket_fd, const char *file_name, long window_size,
                     long timeout_ms)
{
    FILE *input = fopen(file_name, "rb");
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    struct packet incoming;
    struct sender_state sender;
    struct protocol_actions actions;
    uint8_t chunk[PACKET_MAX_PAYLOAD];
    int result = 2;

    if (input == NULL) {
        fprintf(stderr, "could not open input file %s: %s\n", file_name,
                strerror(errno));
        return 2;
    }
    sender_init(&sender, (uint32_t)window_size, timeout_ms);
    for (;;) {
        int64_t now = monotonic_milliseconds();
        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            goto done;
        }
        while (!sender.input_finished && sender_capacity(&sender) > 0) {
            size_t bytes_read = fread(chunk, 1, sizeof(chunk), input);
            if (bytes_read == 0) {
                if (ferror(input) != 0) {
                    fprintf(stderr, "could not read input file %s: %s\n",
                            file_name, strerror(errno));
                    goto done;
                }
                if (sender_finish(&sender, now, &actions) != 0) {
                    goto done;
                }
                break;
            }
            if (sender_push_data(&sender, chunk, (uint16_t)bytes_read, now,
                                 &actions) != 0) {
                goto done;
            }
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to send packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
        }
        if (sender.input_finished && sender.base == sender.next &&
            !sender.fin_sent) {
            if (sender_finish(&sender, now, &actions) != 0) {
                goto done;
            }
        }
        if (sender_is_complete(&sender)) {
            result = 0;
            break;
        }
        if (sender.timer_due < 0) {
            continue;
        }
        now = monotonic_milliseconds();
        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            goto done;
        }
        if (sender.timer_due <= now) {
            int timeout_result = sender_on_timeout(&sender, now, &actions);
            if (timeout_result != 0) {
                fprintf(stderr, "sender gave up after 10 timeouts\n");
                goto done;
            }
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to retransmit packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
            continue;
        }
        {
            int64_t remaining = sender.timer_due - now;
            int ready = wait_for_socket(
                socket_fd, remaining > INT_MAX ? INT_MAX : (int)remaining);
            if (ready < 0) {
                fprintf(stderr, "sender failed while waiting: %s\n",
                        strerror(errno));
                goto done;
            }
            if (ready == 0) {
                continue;
            }
        }
        {
            ssize_t received = recv(socket_fd, wire, sizeof(wire), 0);
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                fprintf(stderr, "sender failed to receive ACK: %s\n",
                        strerror(errno));
                goto done;
            }
            if (packet_decode(wire, (size_t)received, &incoming) != 0 ||
                incoming.type != PACKET_ACK || incoming.length != 0) {
                continue;
            }
            now = monotonic_milliseconds();
            if (now < 0) {
                fprintf(stderr, "could not read monotonic clock\n");
                goto done;
            }
            sender_on_ack(&sender, incoming.seq, now, &actions);
            for (size_t i = 0; i < actions.outgoing_count; ++i) {
                if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                    fprintf(stderr, "sender failed to send packet: %s\n",
                            strerror(errno));
                    goto done;
                }
            }
        }
    }

done:
    if (fclose(input) != 0 && result == 0) {
        fprintf(stderr, "could not close input file %s: %s\n", file_name,
                strerror(errno));
        result = 2;
    }
    return result;
}

static int receive_file(int socket_fd, const char *file_name)
{
    FILE *output = fopen(file_name, "wb");
    uint8_t wire[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
    struct packet packet;
    struct receiver_state receiver;
    struct protocol_actions actions;
    int64_t last_valid;
    int result = 2;

    if (output == NULL) {
        fprintf(stderr, "could not open output file %s: %s\n", file_name,
                strerror(errno));
        return 2;
    }
    receiver_init(&receiver);
    last_valid = monotonic_milliseconds();
    if (last_valid < 0) {
        fprintf(stderr, "could not read monotonic clock\n");
        (void)fclose(output);
        return 2;
    }
    for (;;) {
        int64_t now = monotonic_milliseconds();
        int timeout_ms;
        ssize_t received;
        int ready;

        if (now < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            break;
        }
        if (receiver_linger_expired(&receiver, now)) {
            result = 0;
            break;
        }
        if (receiver_is_finished(&receiver)) {
            timeout_ms = (int)(receiver.linger_until - now);
        } else {
            timeout_ms = (int)(30000 - (now - last_valid));
        }
        if (timeout_ms <= 0) {
            if (receiver_is_finished(&receiver)) {
                result = 0;
            } else {
                fprintf(stderr, "receiver timed out waiting for a valid packet\n");
            }
            break;
        }
        ready = wait_for_socket(socket_fd, timeout_ms);
        if (ready < 0) {
            fprintf(stderr, "receiver failed while waiting: %s\n",
                    strerror(errno));
            break;
        }
        if (ready == 0) {
            if (receiver_is_finished(&receiver)) {
                result = 0;
            } else {
                fprintf(stderr, "receiver timed out waiting for a valid packet\n");
            }
            break;
        }
        received = recv(socket_fd, wire, sizeof(wire), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "receiver failed to read packet: %s\n",
                    strerror(errno));
            break;
        }
        if (packet_decode(wire, (size_t)received, &packet) != 0) {
            continue;
        }
        last_valid = monotonic_milliseconds();
        if (last_valid < 0) {
            fprintf(stderr, "could not read monotonic clock\n");
            break;
        }
        receiver_on_packet(&receiver, &packet, last_valid, &actions);
        if (actions.delivered_packet &&
            fwrite(actions.delivered, 1, actions.delivered_length, output) !=
                actions.delivered_length) {
            fprintf(stderr, "could not write output file %s: %s\n",
                    file_name, strerror(errno));
            break;
        }
        for (size_t i = 0; i < actions.outgoing_count; ++i) {
            if (send_packet(socket_fd, &actions.outgoing[i]) != 0) {
                fprintf(stderr, "receiver failed to send ACK: %s\n",
                        strerror(errno));
                break;
            }
        }
        if (receiver_is_finished(&receiver)) {
            if (fclose(output) != 0) {
                fprintf(stderr, "could not close output file %s: %s\n",
                        file_name, strerror(errno));
                return 2;
            }
            output = NULL;
        }
    }
    if (output != NULL) {
        (void)fclose(output);
    }
    return result;
}

int main(int argc, char **argv)
{
    const char *mode;
    const char *session = NULL;
    const char *relay;
    const char *file;
    long window = 8;
    long timeout_ms = 250;
    long port = 4250;
    double loss = 0.0;
    double corrupt = 0.0;
    double duplicate = 0.0;
    int option;
    int sender_option_seen = 0;
    struct addrinfo hints = {0};
    struct addrinfo *relay_address = NULL;
    struct addrinfo *address;
    char port_text[6];
    int socket_fd = -1;
    int result;

    if (argc == 1) {
        print_usage(stdout);
        return 0;
    }
    if (argc < 2 || (strcmp(argv[1], "send") != 0 &&
                     strcmp(argv[1], "recv") != 0)) {
        print_usage(stderr);
        return 1;
    }
    mode = argv[1];

    optind = 2;
    opterr = 0;
    while ((option = getopt(argc, argv, "s:w:T:l:c:d:p:")) != -1) {
        switch (option) {
        case 's':
            session = optarg;
            break;
        case 'w':
            if (parse_long_value(optarg, 1, 64, &window) != 0) {
                fprintf(stderr, "invalid window: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'T':
            if (parse_long_value(optarg, 1, INT32_MAX, &timeout_ms) != 0) {
                fprintf(stderr, "invalid timeout: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'l':
            if (parse_probability(optarg, &loss) != 0) {
                fprintf(stderr, "invalid loss probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'c':
            if (parse_probability(optarg, &corrupt) != 0) {
                fprintf(stderr, "invalid corruption probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'd':
            if (parse_probability(optarg, &duplicate) != 0) {
                fprintf(stderr, "invalid duplicate probability: %s\n", optarg);
                return 1;
            }
            sender_option_seen = 1;
            break;
        case 'p':
            if (parse_long_value(optarg, 1, 65535, &port) != 0) {
                fprintf(stderr, "invalid port: %s\n", optarg);
                return 1;
            }
            break;
        case '?':
        default:
            print_usage(stderr);
            return 1;
        }
    }

    if (session == NULL || session[0] == '\0' ||
        (strcmp(mode, "recv") == 0 && sender_option_seen) ||
        argc - optind != 2) {
        print_usage(stderr);
        return 1;
    }

    relay = argv[optind];
    file = argv[optind + 1];
    (void)file;
    (void)window;
    (void)timeout_ms;
    (void)loss;
    (void)corrupt;
    (void)duplicate;

    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;
    (void)snprintf(port_text, sizeof(port_text), "%ld", port);
    if (getaddrinfo(relay, port_text, &hints, &relay_address) != 0) {
        fprintf(stderr, "could not resolve relay: %s\n", relay);
        return 2;
    }
    for (address = relay_address; address != NULL; address = address->ai_next) {
        socket_fd = socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }
        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        close(socket_fd);
        socket_fd = -1;
    }
    if (socket_fd < 0) {
        fprintf(stderr, "could not connect to relay: %s\n", strerror(errno));
        freeaddrinfo(relay_address);
        return 2;
    }
    if (strcmp(mode, "send") == 0) {
        result = register_sender(socket_fd, session, loss, corrupt, duplicate);
        if (result == 0) {
            result = send_file(socket_fd, file, window, timeout_ms);
        }
    } else {
        result = register_receiver(socket_fd, session);
        if (result == 0) {
            result = receive_file(socket_fd, file);
        }
    }
    close(socket_fd);
    freeaddrinfo(relay_address);
    return result == 0 ? 0 : 2;
}

```

## Tests Files
### lab-test.c

```c

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

```

## Scripts Files
Report generated on 10/09/2026 at 21:49:22


---

## End of Report

SHA-256 Hash of the report: 5e3c89b47fa7e59eb3ad4502353503bdccb33bb14af87f35555731b4903f4c83

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: antoines21/cs425-p2
- The repository visibility is public.
- The workflow was triggered by antoines21
