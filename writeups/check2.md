Checkpoint 2 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This lab took me about [n] hours to do. I did not attend the lab session.

Wrap32: wrap is `zero_point + uint32(n)`. unwrap takes the signed 32-bit offset from the checkpoint's wrap and adds `2^32` if that lands negative.

TCPReceiver keeps `optional<Wrap32> isn_`. No SYN → no ackno, drop non-RST. RST → `set_error()`. Stream index is 0 on the SYN payload, else `abs_seqno - 1`. ackno is `wrap(bytes_pushed + 1 [+1 if closed])`. window is `min(available_capacity, UINT16_MAX)`.

Implementation Challenges:
[SYN/FIN occupy seq space but not the ByteStream; FIN still closes only after the reassembler fills up to that index.]

Remaining Bugs:
[none known]
