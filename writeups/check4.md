Checkpoint 4 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to thank/reward these classmates for their help: none

This checkpoint took me about [n] hours to do. I did not attend the lab session.

How to collect (do not exceed this rate on the public Internet):

    ping -D -n -i 0.2 HOST | tee pathN.txt

Need three interesting paths, each ≥1 hour. Mix at least one long-distance
"boring" path (RTT > 100 ms) and one with an interesting link (Wi-Fi /
cellular / satellite). Trace each path with mtr or traceroute first.

Do not flood: default-sized echo every 0.2 s is the cap for long runs.
Brief (<10 s) higher-rate trials (`-s` ≤ 1400, smaller `-i`, `-c` to stop)
are optional and only after a user names a target.

Analysis: `python3 scripts/ping_analyze.py pathN.txt`

Uses actual icmp_seq span for sent count (received/sent). Does not use the
lecture slip that writes 5×3600 as ~3600 (true 5 Hz × 3600 s = 18000).
Missing sequence numbers are losses. Writes `pathN_rtt.svg` and `pathN_cdf.svg`.

Status: tool complete / sample PASS (delivery 4/5=0.8, longest loss 1) /
long public capture 未验证. Real collection waits for user-chosen targets
and time windows. Did not run a 1-hour public ping.

Implementation Challenges:
[none for the analyzer]

Remaining Bugs:
[none known]
