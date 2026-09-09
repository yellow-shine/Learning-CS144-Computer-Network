# Verification

三列：实现完成 / 本地验证通过 / 外部待验证。没有跑过的命令不写「通过」。未改 `util/`、`tests/`。未伪造通过。

容器 `cs144-minnow`，g++ 13.3.0。2026-09-10 重跑：`check1`、`check5`、`check6`、`python3 scripts/ping_analyze.py testdata/ping_sample.txt`。未重跑 `check0` / `check_webget` / `check2` / `check3` / `speed` 目标（wrapping 15s 超时；webget 需外部 HTTP）。

## Checkpoint 0

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| ByteStream | 是 `src/byte_stream.{hh,cc}` tag `checkpoint-0-bytestream` | 通过。2026-09-09 `./scripts/dev.sh bash -lc 'cmake --build build --target check_byte_stream'` g++ 13.3.0，10/10 PASS。2026-09-10 含在 `check1` 的 `byte_stream_*` 均 PASS | 无 |
| webget | 是 `apps/webget.cc` tag `checkpoint-0` | 未通过。2026-09-09 compile-with-bug-checkers PASS；`t_webget` / `check_webget` 空 body。未于 2026-09-10 重跑 | `cs144.keithw.org:80` hasher。DNS `104.196.238.229`；host :80 超时；容器 EOF 0 bytes；代理 502。斯坦福身份/邮件作业未做 |

`check0` 目标含 webget，故整体不能记通过。

## Checkpoint 1

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| Reassembler | 是 `src/reassembler.{hh,cc}` tag `checkpoint-1` | 通过。2026-09-10 `./scripts/dev.sh bash -lc 'cmake --build build --target check1'` g++ 13.3.0，18/18 PASS（含 `byte_stream_*`、`reassembler_*`、`no_skip`、两条 `*_speed_test`） | 无 |
| speed 吞吐数字 | 同上 | 2026-09-09 `speed`：`reassembler_speed_test` 32.57 / 4.22 Gbit/s（≥ 0.1）。2026-09-10 未单独跑 `speed` 目标，无新 Gbit/s | 无 |

## Checkpoint 2

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| Wrap32 | 是 `src/wrapping_integers.cc` | wrap / unwrap / cmp / extra PASS（Task 6，2026-09-09，g++ 13.3.0）。`wrapping_integers_roundtrip` 需 ~121s，官方 15s 超时 | 无 |
| TCPReceiver | 是 `src/tcp_receiver.{hh,cc}` tag `checkpoint-2` | 全部 `recv_*` PASS（Task 7，2026-09-09；`recv_transmit` 25.5s，`recv_reorder_more` 26.9s）。未于 2026-09-10 重跑 | 无 |
| `check2` 目标 | — | 未通过。`--timeout 15` 卡在 `wrapping_integers_roundtrip`。未于 2026-09-10 重跑 | 无 |

## Checkpoint 3

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| TCPSender | 是 `src/tcp_sender.{hh,cc}` tag `checkpoint-3` | 全部 `send_*` PASS（Task 8，2026-09-09，sanitized，含 `send_extra`）。未于 2026-09-10 重跑 | 无 |
| `check3` 目标 | — | 未通过。同 CP2，15s 卡在 `wrapping_integers_roundtrip`。未于 2026-09-10 重跑 | 无 |
| list `build/apps` | 应用已编出 | 通过。2026-09-09 `CS144_NET=1 ./scripts/dev.sh bash -lc 'cmake --build build && ls build/apps'` g++ 13.3.0 → `tcp_native` `tcp_ipv4` `webget` `iptest` `endtoend` `ip_raw` | 无 |
| 4.1.1 Linux TCP @ 169.254.144.1 | 未改 util | 未验证。2026-09-09 `./build/apps/tcp_native -l 0 9090` 后客户端连 `169.254.144.1:9090`，`timeout 5` rc 124。FIB 仅 127.0.0.1 与 192.168.215.2 | 需宿主机 TAP/Linux TCP |
| `tun.sh start 144` | — | 未验证。2026-09-09 `./scripts/tun.sh start 144` → `exec: sudo: not found` rc 127。镜像无 `sudo`/`ip`/`iptables`；容器 uid=0 | 需能配 TUN 的环境 |
| 4.1.2 minnow TCP vs Linux TCP | — | 未验证。2026-09-09 `tcp_ipv4 169.254.144.1:9090` → `writev: Input/output error` rc 1 | 同上 |
| 4.1.3 one-megabyte | — | 未验证（blocked on TUN） | 同上 |

旁注（非讲义 4.1）：2026-09-09 `tcp_native` 127.0.0.1:9091 交换 `hello-from-client` 且双边结束。讲义路径仍未验证。

## Checkpoint 4

工具 only。未跑 1 小时公网 ping。sent 计的是 icmp_seq 跨度，不是 3600。

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| `ping_analyze.py` 样例 | 是 `scripts/ping_analyze.py` tag `checkpoint-4` | 通过。2026-09-10 `./scripts/dev.sh bash -lc 'python3 scripts/ping_analyze.py testdata/ping_sample.txt'` g++ 13.3.0（同容器），rc 0，`delivery_rate=4/5=0.8`，`longest_loss=1`，PASS | 无 |
| 3 Internet paths × ≥1 hour | 未做 | 未验证 | 待用户指定目标与窗口后再抓包 |
| high-rate <10s throughput sweep | 未做 | 未验证 | 不对公网冒充 |

## Checkpoint 5

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| NetworkInterface + ARP | 是 `src/network_interface.{hh,cc}` tag `checkpoint-5` | 通过。2026-09-10 `./scripts/dev.sh bash -lc 'cmake --build build --target check5'` g++ 13.3.0，3/3 PASS（`net_interface`、`no_skip`） | 无 |

## Checkpoint 6

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| Router LPM / TTL | 是 `src/router.{hh,cc}` tag `checkpoint-6` | 通过。2026-09-10 `./scripts/dev.sh bash -lc 'cmake --build build --target check6'` g++ 13.3.0，4/4 PASS（`net_interface`、`router`、`no_skip`） | 无 |

## Checkpoint 7

Sanitized apps 已编。未改 util/tests。自身双端 ≠ 独立同伴。无伙伴 → Group 未验证。`scripts/local_relay.py` 不是课程环境。

| 项 | 实现完成 | 本地验证通过 | 外部待验证 |
| --- | --- | --- | --- |
| official relay `cs144.keithw.org` | `endtoend` 已编 tag `checkpoint-7` | 未验证。2026-09-09 DNS `104.196.238.229`；TCP :80/:443 ok；UDP even/odd bounce 超时；`timeout 12 ./build/apps/endtoend server … 38142` + client 38143：server `listening`，client `connecting to 172.16.0.100:1234`，rc 124 | 官方 UDP relay |
| self dual-end conversation | 未改 src | 通过（local relay）。2026-09-09 `python3 scripts/local_relay.py 45128`；`endtoend server/client 127.0.0.1 45128/9`；交换 `hello-from-client` / `hello-from-server`；双边 `TCP connection finished cleanly` | 官方 relay 仍待 |
| self dual-end 1MiB | 未改 src | 通过（local relay）。2026-09-09 sha256 `3725efe5efe48663a22ffc34e592283ed48b16d8d76cae9b0417115611f0f899` 双边，1048576 bytes | 官方 relay 仍待 |
| Group portion vs other impl | — | 未验证 | 需同伴实现 |

未于 2026-09-10 重跑 CP7。
