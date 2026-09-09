Checkpoint 1 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This lab took me about [n] hours to do. I did not attend the lab session.

I was surprised by or edified to learn that: `is_last_substring` is an end index, not a close-now flag. A last segment clipped by the window must not close the stream until `next_index` reaches that index.

Reassembler: `std::map<uint64_t, std::string> pending_` of non-overlapping intervals, plus `next_index_` and optional `eof_index_`.

- Clip each insert to `[next_index_, next_index_ + available_capacity)`.
- Merge into the predecessor if it reaches `first_index`; then swallow later overlapping/adjacent nodes by appending only the uncovered suffix (no duplicate bytes).
- Flush the map prefix while `begin()->first == next_index_`.
- Close only when `next_index_ >= eof_index_`.

Alternatives: `set` of intervals, or a `capacity`-sized sparse buffer. Map is less code; overlap merge is O(k log n) per insert. Speed: 32.57 Gbit/s no-overlap / 4.22 Gbit/s 10x-overlap (need ≥ 0.1).

Implementation Challenges:
[Window vs ByteStream capacity: pending bytes are future writes, so the window is writer `available_capacity`, not pending+buffered.]

Remaining Bugs:
[none known]

- Optional: I had unexpected difficulty with: none
