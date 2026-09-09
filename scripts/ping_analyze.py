#!/usr/bin/env python3
"""Analyze `ping -D -n -i 0.2` logs. Stdlib only. Sent count = seq span, never 3600."""

from __future__ import annotations

import math
import re
import sys
from datetime import datetime
from pathlib import Path

LINE_RE = re.compile(
    r"\[(\d+(?:\.\d+)?)\]\s+.*\bicmp_seq=(\d+)\b.*\bttl=\d+.*\btime=([\d.]+)\s*ms"
)


def parse_replies(text: str) -> dict[int, tuple[float, float]]:
    replies: dict[int, tuple[float, float]] = {}
    for line in text.splitlines():
        m = LINE_RE.search(line)
        if not m:
            continue
        ts, seq, rtt = float(m.group(1)), int(m.group(2)), float(m.group(3))
        replies[seq] = (ts, rtt)
    return replies


def longest_run(flags: list[bool], want: bool) -> int:
    best = cur = 0
    for v in flags:
        if v is want:
            cur += 1
            if cur > best:
                best = cur
        else:
            cur = 0
    return best


def cond_prob(flags: list[bool], given: bool, want: bool, k: int) -> float:
    num = den = 0
    n = len(flags)
    for i, v in enumerate(flags):
        j = i + k
        if v is not given or j < 0 or j >= n:
            continue
        den += 1
        if flags[j] is want:
            num += 1
    return num / den if den else float("nan")


def pearson(xs: list[float], ys: list[float]) -> float:
    n = len(xs)
    if n < 2:
        return float("nan")
    mx = sum(xs) / n
    my = sum(ys) / n
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys, strict=True))
    dx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    dy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if dx == 0.0 or dy == 0.0:
        return float("nan")
    return num / (dx * dy)


def _esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def write_svg(path: Path, xs: list[float], ys: list[float], xlabel: str, ylabel: str,
              xlabels: tuple[str, str] | None = None) -> None:
    w, h, p = 640, 280, 52
    xmin, xmax = min(xs), max(xs)
    ymin, ymax = min(ys), max(ys)
    if xmin == xmax:
        xmax += 1.0
    if ymin == ymax:
        ymax += 1.0
    def sx(x: float) -> float:
        return p + (x - xmin) / (xmax - xmin) * (w - 2 * p)
    def sy(y: float) -> float:
        return h - p - (y - ymin) / (ymax - ymin) * (h - 2 * p)
    pts = " ".join(f"{sx(x):.1f},{sy(y):.1f}" for x, y in zip(xs, ys, strict=True))
    xl0, xl1 = xlabels if xlabels else (f"{xmin:g}", f"{xmax:g}")
    path.write_text(
        f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
  <rect width="100%" height="100%" fill="#fff"/>
  <line x1="{p}" y1="{h-p}" x2="{w-p}" y2="{h-p}" stroke="#000"/>
  <line x1="{p}" y1="{p}" x2="{p}" y2="{h-p}" stroke="#000"/>
  <polyline fill="none" stroke="#06c" stroke-width="1.5" points="{pts}"/>
  <text x="{w/2}" y="{h-10}" text-anchor="middle" font-size="12">{_esc(xlabel)}</text>
  <text x="14" y="{h/2}" font-size="12" transform="rotate(-90 14 {h/2})">{_esc(ylabel)}</text>
  <text x="{p}" y="{h-p+16}" font-size="10">{_esc(xl0)}</text>
  <text x="{w-p}" y="{h-p+16}" text-anchor="end" font-size="10">{_esc(xl1)}</text>
  <text x="{p-4}" y="{h-p}" text-anchor="end" font-size="10">{ymin:g}</text>
  <text x="{p-4}" y="{p+10}" text-anchor="end" font-size="10">{ymax:g}</text>
</svg>
'''
    )


def fmt_tod(ts: float) -> str:
    return datetime.fromtimestamp(ts).strftime("%H:%M:%S")


def analyze(path: Path) -> int:
    replies = parse_replies(path.read_text(errors="replace"))
    if not replies:
        print("no ping replies parsed", file=sys.stderr)
        return 1
    lo, hi = min(replies), max(replies)
    sent = hi - lo + 1  # seq span; not 5*3600 and not the lecture's 3600 slip
    received = len(replies)
    delivery = received / sent
    ok = [s in replies for s in range(lo, hi + 1)]
    longest_success = longest_run(ok, True)
    longest_loss = longest_run(ok, False)
    rtts = [replies[s][1] for s in sorted(replies)]
    rtt_min, rtt_max = min(rtts), max(rtts)
    adj_x, adj_y = [], []
    for s in range(lo, hi):
        if s in replies and s + 1 in replies:
            adj_x.append(replies[s][1])
            adj_y.append(replies[s + 1][1])
    rtt_corr = pearson(adj_x, adj_y)

    print(f"sent={sent} received={received} delivery_rate={received}/{sent}={delivery:.6g}")
    print(f"longest_success={longest_success} longest_loss={longest_loss}")
    print(f"rtt_min_ms={rtt_min:g} rtt_max_ms={rtt_max:g}")
    print(f"rtt_adjacent_corr={rtt_corr:.6g}")
    print("loss_autocorr k=-10..10  P(ok|ok)  P(loss|loss):")
    for k in range(-10, 11):
        p_ok = cond_prob(ok, True, True, k)
        p_loss = cond_prob(ok, False, False, k)
        print(f"  k={k:3d}  {p_ok:.6g}  {p_loss:.6g}")

    times = [replies[s][0] for s in sorted(replies)]
    stem = path.with_suffix("")
    write_svg(
        Path(str(stem) + "_rtt.svg"),
        times,
        rtts,
        "time of day",
        "RTT ms",
        (fmt_tod(times[0]), fmt_tod(times[-1])),
    )
    sr = sorted(rtts)
    n = len(sr)
    write_svg(
        Path(str(stem) + "_cdf.svg"),
        sr,
        [(i + 1) / n for i in range(n)],
        "RTT ms",
        "CDF",
    )

    if path.name == "ping_sample.txt":
        missing = [s for s in range(lo, hi + 1) if s not in replies]
        assert missing == [3], missing
        assert {0, 1, 2, 4} <= set(replies), set(replies)
        assert sent == 5 and received == 4 and delivery == 0.8
        assert longest_loss == 1
        print("PASS")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(f"usage: {argv[0]} PING_LOG", file=sys.stderr)
        return 2
    return analyze(Path(argv[1]))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
