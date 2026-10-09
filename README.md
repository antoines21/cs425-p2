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
