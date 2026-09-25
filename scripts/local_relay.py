#!/usr/bin/env python3
"""Local two-port UDP bounce. Not the CS144 course relay (cs144.keithw.org)."""

from __future__ import annotations

import select
import socket
import sys


def main() -> None:
    if len(sys.argv) not in (2, 3):
        print(f"Usage: {sys.argv[0]} EVEN_PORT [ODD_PORT]", file=sys.stderr)
        raise SystemExit(2)
    even = int(sys.argv[1])
    odd = int(sys.argv[2]) if len(sys.argv) == 3 else even + 1
    socks: list[socket.socket] = []
    for port in (even, odd):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", port))
        socks.append(s)
    peer: list[tuple[str, int] | None] = [None, None]
    print(
        f"local_relay 127.0.0.1:{even} <-> 127.0.0.1:{odd} (not course relay)",
        file=sys.stderr,
        flush=True,
    )
    while True:
        ready, _, _ = select.select(socks, [], [])
        for s in ready:
            i = 0 if s is socks[0] else 1
            data, addr = s.recvfrom(65535)
            peer[i] = addr
            dst = peer[1 - i]
            # 对端还没发过包时丢掉这一包。endtoend 会先发三次空 UDP 注册。
            if dst is not None:
                socks[1 - i].sendto(data, dst)


if __name__ == "__main__":
    main()
