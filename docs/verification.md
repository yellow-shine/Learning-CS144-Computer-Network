# Verification

CP3 application path only (Task 14 expands). 2026-09-09, g++ 13.3.0,
`CS144_NET=1 ./scripts/dev.sh`. Did not fake passing. Did not change util.

| Item | Status | Evidence |
| --- | --- | --- |
| list `build/apps` | 通过 | `CS144_NET=1 ./scripts/dev.sh bash -lc 'cmake --build build && ls build/apps'` → `tcp_native`, `tcp_ipv4`, `webget`, `iptest`, `endtoend`, `ip_raw` |
| 4.1.1 Linux TCP @ 169.254.144.1 | 未验证 | `./build/apps/tcp_native -l 0 9090` then `./build/apps/tcp_native 169.254.144.1 9090`. Server: `DEBUG: Listening for incoming connection...`. Client hung at `DEBUG: Connecting to 169.254.144.1:9090...` (`timeout 5` rc 124). FIB has 127.0.0.1 and 192.168.215.2 only |
| `tun.sh start 144` | 未验证 | `./scripts/tun.sh start 144` → `./scripts/tun.sh: line 64: exec: sudo: not found` (rc 127). Image has no `sudo`/`ip`/`iptables`; container uid=0 |
| 4.1.2 minnow TCP vs Linux TCP | 未验证 | `./build/apps/tcp_native -l 0 9090` then `./build/apps/tcp_ipv4 169.254.144.1 9090` → `DEBUG: minnow connecting to 169.254.144.1:9090...` / `Exception: writev: Input/output error` (rc 1) |
| 4.1.3 one-megabyte | 未验证 | blocked on TUN |

Side note (not lecture 4.1): `tcp_native` 127.0.0.1:9091 exchanged `hello-from-client` and both streams finished. Lecture path still 未验证.

## Checkpoint 4

Tool only. Did not run a 1-hour public ping. Sent count is icmp_seq span, not 3600.

| Item | Status | Evidence |
| --- | --- | --- |
| `python3 scripts/ping_analyze.py testdata/ping_sample.txt` | 通过 | rc 0, `delivery_rate=4/5=0.8`, `longest_loss=1`, PASS |
| 3 Internet paths × ≥1 hour | 未验证 | capture deferred until user names targets/windows |
| high-rate <10s throughput sweep | 未验证 | not run against the public Internet |
