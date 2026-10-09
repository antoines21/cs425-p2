# Project X

- Name: John Doe
- Email: johndoe@u.boisestate.edu
- Class: CS123-001

## Known Bugs or Issues

The sender transfers the file correctly in the Task 6 measurements. The
receiver prints `receiver timed out waiting for a valid packet` after a
successful transfer instead of exiting cleanly. One receiver process also
segfaulted during a 5% loss run; the resulting file still passed `cmp`, but
this remains an issue to fix in the later leak and crash-check task.

## Experience

TODO: Describe your experience with the project (struggles, breakthroughs, etc.).

## Analysis

The measurements and interpretation for Task 6 are included in the Results
section below.

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
