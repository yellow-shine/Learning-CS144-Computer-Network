Checkpoint 3 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

Program Structure and Design of the TCPSender:
Outstanding segments live in a deque; in-flight count is their sequence_length sum. push fills an effective window (0 treated as 1 for probing only). SYN/FIN occupy seqnos. Timer starts on first seq-using send, stops when nothing is outstanding, restarts on a new legal ack. Timeout retransmits the earliest whole segment; exponential backoff only if the real window is nonzero. Illegal ack (ackno > next_abs_) updates the window field but does not reset RTO.

Report from the hands-on component: []

Implementation Challenges:
[zero-window probe vs full nonzero window; FIN deferred when it would exceed the window; RST on stream error]

Remaining Bugs:
[none known]
