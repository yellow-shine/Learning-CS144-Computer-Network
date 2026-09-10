Checkpoint 7 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

Environment (not the course original):
Official relay cs144.keithw.org did not bounce UDP. DNS
104.196.238.229, TCP :80/:443 ok, UDP even/odd pair timed out on host
and in cs144-minnow. `endtoend` vs EVEN=38142 hung at listen/connect
(timeout rc 124). Added `scripts/local_relay.py`: two UDP ports, payload
forwarded as-is. This is a local adapter, not cs144.keithw.org.

Solo portion:

- Did your implementation successfully start and end a conversation
  with another copy of itself? y (via local_relay 127.0.0.1:45128/9,
  not the course relay)

- Did it successfully transfer a one-megabyte file, with contents
  identical upon receipt? y (sha256
  3725efe5efe48663a22ffc34e592283ed48b16d8d76cae9b0417115611f0f899,
  1048576 bytes both sides)

- Please describe what code changes, if any, were necessary to pass
  these steps:
  None in src/. Only the local UDP bounce above.

Group portion:

- Who is your lab partner (and what is their SUNet ID, e.g. "winstein"?
  none; no partner

- Did your implementations successfully start and end a conversation
  with each other (with each implementation acting as ``client'' or as
  ``server'')?
  未验证. Self dual-end success ≠ independent peer.

- Did you successfully transfer a one-megabyte file between your two
  implementations, with contents identical upon receipt?
  未验证 (no partner)

- Please describe what code changes, if any, were necessary to pass
  these steps, either by you or your lab partner.
  n/a

Creative portion (if you did anything for our creative challenge,
                  please boast about it!)
none

- Optional: I had unexpected difficulty with: course relay UDP bounce
  dead while TCP to the same host still works.

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
