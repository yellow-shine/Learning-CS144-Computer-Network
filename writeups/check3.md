Checkpoint 3 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

Program Structure and Design of the TCPSender:
Outstanding segments live in a deque; in-flight count is their sequence_length sum. push fills an effective window (0 treated as 1 for probing only). SYN/FIN occupy seqnos. Timer starts on first seq-using send, stops when nothing is outstanding, restarts on a new legal ack. Timeout retransmits the earliest whole segment; exponential backoff only if the real window is nonzero. Illegal ack (ackno > next_abs_) updates the window field but does not reset RTO.

Report from the hands-on component:
4.1 未验证 (2026-09-09, g++ 13.3.0, CS144_NET=1).
Apps listed: tcp_native, tcp_ipv4 present.
Lecture 4.1.1: tcp_native -l 0 9090; tcp_native 169.254.144.1 9090.
Client hung: DEBUG: Connecting to 169.254.144.1:9090... (timeout 5, rc 124).
./scripts/tun.sh start 144 -> line 64: exec: sudo: not found (rc 127).
No ip/iptables/sudo in image; container is root.
4.1.2: tcp_ipv4 169.254.144.1 9090 -> writev: Input/output error.
4.1.3 not run. Did not change util. Did not fake passing.
127.0.0.1 tcp_native self-talk worked; that is not lecture 4.1.

Implementation Challenges:
[zero-window probe vs full nonzero window; FIN deferred when it would exceed the window; RST on stream error]

Remaining Bugs:
[none known]
