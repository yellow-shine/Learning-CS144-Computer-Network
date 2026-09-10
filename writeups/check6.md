Checkpoint 6 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

Program Structure and Design of the Router:
Vector of {prefix, prefix_length, optional next_hop, interface}. Match longest prefix_length. prefix_length 0 matches all (no 32-bit shift). No match or TTL<=1: drop. Else decrement TTL, recompute checksum, send on that interface. Direct route next hop = datagram dst.

Implementation Challenges:
[uint32 shift-by-32 UB for /0 default route]

Remaining Bugs:
[none known]
