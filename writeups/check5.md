Checkpoint 5 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

Program Structure and Design of the NetworkInterface:
ARP cache is a map IP→{eth, 30s ttl}. Unknown next-hop: queue the datagram, broadcast one ARP request, suppress repeats for 5s. Learn sender mapping from any ARP (request or reply) and flush that IP's queue. Reply only if the request targets this interface. Frames not to us or broadcast are ignored. IPv4 payloads parse into datagrams_received_. When the 5s wait expires, drop that IP's pending queue (no ARP retry).

Implementation Challenges:
[learn-from-any-ARP vs reply-only-if-ours; 5s wait vs 30s cache; drop pending on wait expiry]

Remaining Bugs:
[none known]
