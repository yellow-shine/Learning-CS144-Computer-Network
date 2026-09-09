# CS144 Winter 2025 Reference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在本仓库落地 Stanford CS144 Winter 2025 / minnow 的整套参考实现、容器化验证记录和中文导读，使后续可以按模块带读。

**Architecture:** 保留 minnow 公开接口与测试；只改 `src/`、`apps/webget.cc`、writeups 和本地文档/脚本。用 Ubuntu 24.04 容器跑 CMake/CTest。实现按 ByteStream → Reassembler → Wrap32/TCPReceiver → TCPSender → NetworkInterface → Router → 联调/测量工具的依赖顺序提交并打标签。

**Tech Stack:** C++20 minnow、CMake/CTest、ASan/UBSan、Docker Ubuntu 24.04、g++ 13、Python 3 标准库（Checkpoint 4 分析）。

---

## File structure

导入起始代码后，仓库根目录就是 minnow。新增文件只服务构建、来源追溯、验证记录和带读，不另造协议框架。

| Path | Responsibility |
| --- | --- |
| `src/byte_stream.{hh,cc}` | 有容量限制的单线程可靠字节流 |
| `src/reassembler.{hh,cc}` | 乱序/重叠子串重组并写入 ByteStream |
| `src/wrapping_integers.{hh,cc}` | 32-bit seqno 与绝对序号换算 |
| `src/tcp_receiver.{hh,cc}` | 收段、插入 Reassembler、产生 ackno/window/RST |
| `src/tcp_sender.{hh,cc}` | 填窗口、记录未确认段、超时重传 |
| `src/network_interface.{hh,cc}` | IP↔Ethernet、ARP 缓存与排队 |
| `src/router.{hh,cc}` | 最长前缀匹配转发、TTL |
| `apps/webget.cc` | 用 OS TCPSocket 发 HTTP/1.1 GET |
| `writeups/checkN.md` | 每个 checkpoint 的设计、命令、限制 |
| `Dockerfile` / `scripts/dev.sh` | Ubuntu 24.04 构建与测试入口 |
| `scripts/ping_analyze.py` | Checkpoint 4 ping 日志分析 |
| `scripts/local_relay.py` | 仅当官方 relay 不可用时的本地 UDP 中继 |
| `docs/SOURCES.md` | 归档 URL、revision、讲义哈希 |
| `docs/RUN.md` | 容器构建与测试命令 |
| `docs/overview.md` | 应用→TCP→IP→链路全景 |
| `docs/verification.md` | 实现完成 / 本地通过 / 外部待验证 |
| `docs/assignments/checkN.pdf` | 同期讲义副本 |
| `testdata/ping_sample.txt` | 分析脚本的可运行样例 |

不要修改 `tests/`、`util/`（除课程允许范围外）、不要公开发布解答、不要 push。

---

### Task 1: 导入 Winter 2025 起始代码并固定来源

**Files:**

- Create: `docs/SOURCES.md`, `docs/assignments/check{0-7}.pdf`
- Import: minnow 树（保留已有 `docs/superpowers/`）

- [ ] **Step 1: 克隆固定 revision 并核对提交**

```bash
git clone https://github.com/HT4w5/minnow-winter-2025.git /tmp/minnow-w2025
git -C /tmp/minnow-w2025 fetch --all
git -C /tmp/minnow-w2025 switch --detach 591b466f888970eedd6c3b37c4cae8cf58ee7ee3
git -C /tmp/minnow-w2025 log -1 --format='%H %s'
```

Expected: `591b466f888970eedd6c3b37c4cae8cf58ee7ee3 Relax new NetworkInterface test case`

若远端不可用：使用本机已核实过的 `/tmp/cs144-course-inspect/minnow-winter-2025`（同一 SHA）。

- [ ] **Step 2: 把起始代码拷进仓库根目录，不覆盖方案文档**

```bash
rsync -a --exclude .git /tmp/minnow-w2025/ ./
mkdir -p docs/assignments
for n in 0 1 2 3 4 5 6 7; do
  curl -fsSL "https://raw.githubusercontent.com/HT4w5/cs144.github.io/374000c4096c51946360aea0694e504d8b48f32a/assignments/check${n}.pdf" \
    -o "docs/assignments/check${n}.pdf"
done
shasum -a 256 docs/assignments/check*.pdf
```

Expected hashes:

```
465a0aad27c0915bf2ef4afb8430b4ca70462526e791063e6c6d0d12111d7cb9  docs/assignments/check0.pdf
8564faea756ec53eade29fc51587c25dc9ae30103b74608789959b78c73ade24  docs/assignments/check1.pdf
a9caa16929525167e6dc978cd8da3fb63605ba8ea2d96313673dd02806b7658f  docs/assignments/check2.pdf
ef75c7caa6e3ad7acac1af57391dd1e0af7e582d4f4f80ba1d16c154943ed374  docs/assignments/check3.pdf
bb2ecd9831b2ed7bc5189306ccac44cf26a663ec368ff3e388d8b40399afaf62  docs/assignments/check4.pdf
a2e1c0234f339d903a923d913c58ded762a66f1e044b937a2e3d973afeaf18f5  docs/assignments/check5.pdf
39b59082bc19dc2df102d027713cfa328295bf403be0307978ab7538d8d3ef80  docs/assignments/check6.pdf
32b7391af4f10b6ae4fed838af29ea55737eeffb12d0c7d72afd567ed661a6dc  docs/assignments/check7.pdf
```

- [ ] **Step 3: 写来源说明**

Create `docs/SOURCES.md`:

```markdown
# 来源

本仓库学生实现部分是本地学习用参考实现，不公开发布。

| 材料 | 归档 | revision |
| --- | --- | --- |
| 起始代码 | https://github.com/HT4w5/minnow-winter-2025 | 591b466f888970eedd6c3b37c4cae8cf58ee7ee3 |
| 讲义网站 | https://github.com/HT4w5/cs144.github.io | 374000c4096c51946360aea0694e504d8b48f32a |

2026-09-09：官方 `cs144.github.io` 与 `github.com/CS144/minnow` 不可访问。
讲义 PDF 哈希见 `docs/assignments/` 与本计划 Task 1。
```

- [ ] **Step 4: 提交起始代码**

```bash
git add -A
git status
git commit -m "chore: import CS144 Winter 2025 minnow starter at 591b466"
git tag starter-2025
```

---

### Task 2: Ubuntu 24.04 容器与验证入口

**Files:**

- Create: `Dockerfile`, `scripts/dev.sh`, `docs/RUN.md`

- [ ] **Step 1: 写 Dockerfile**

```dockerfile
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake ninja-build build-essential g++ clang clang-tidy clang-format \
    pkg-config python3 iputils-ping traceroute iproute2 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
```

- [ ] **Step 2: 写 `scripts/dev.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
IMAGE=cs144-minnow
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  docker build -t "$IMAGE" "$ROOT"
fi
CAPS=()
if [[ "${CS144_NET:-}" == "1" ]]; then
  CAPS+=(--device /dev/net/tun --cap-add NET_ADMIN)
fi
docker run --rm "${CAPS[@]}" -v "$ROOT":/src -w /src "$IMAGE" "$@"
```

`chmod +x scripts/dev.sh`

不要加 `--privileged`。

- [ ] **Step 3: 写 `docs/RUN.md`**

```markdown
# 运行

构建镜像并进入：

    ./scripts/dev.sh bash

配置与编译：

    cmake -S . -B build -G Ninja
    cmake --build build

默认 `test` 目标排除 webget 与 speed。需要分别跑：

    cmake --build build --target check0
    cmake --build build --target check_webget
    cmake --build build --target check1
    cmake --build build --target check2
    cmake --build build --target check3
    cmake --build build --target check5
    cmake --build build --target check6
    cmake --build build --target speed

TUN/TAP（Checkpoint 3/7 应用路径）：

    CS144_NET=1 ./scripts/dev.sh bash
```

- [ ] **Step 4: 在容器里确认工具链并做空构建**

```bash
./scripts/dev.sh bash -lc 'g++ --version; cmake --version; cmake -S . -B build -G Ninja && cmake --build build'
```

Expected: g++ 13.x、CMake ≥ 3.24、构建成功（测试会失败，因为实现仍是空的）。

- [ ] **Step 5: 提交**

```bash
git add Dockerfile scripts/dev.sh docs/RUN.md
git commit -m "chore: add Ubuntu 24.04 container for minnow tests"
```

---

### Task 3: ByteStream

**Files:**

- Modify: `src/byte_stream.hh`, `src/byte_stream.cc`
- Test: 官方 `check_byte_stream` / `check0` 中的 `byte_stream_*`

约束：`Reader`/`Writer` 的 `sizeof` 必须等于 `ByteStream`，状态只能加在基类。`peek()` 必须返回当前缓冲的连续 `string_view`（测试会整段比较）。

- [ ] **Step 1: 跑测试确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake -S . -B build -G Ninja && cmake --build build --target check_byte_stream'
```

Expected: FAIL（`available_capacity` / `peek` 等为 0 或空）。

- [ ] **Step 2: 在 `ByteStream` 基类加状态**

`src/byte_stream.hh` 的 `protected:` 改为：

```cpp
protected:
  uint64_t capacity_;
  bool error_ {};
  bool closed_ {};
  std::string buffer_ {};
  uint64_t start_ {};
  uint64_t bytes_pushed_ {};
  uint64_t bytes_popped_ {};
```

- [ ] **Step 3: 实现 `src/byte_stream.cc`**

```cpp
#include "byte_stream.hh"

#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( closed_ || data.empty() ) {
    return;
  }
  const uint64_t n = min( available_capacity(), static_cast<uint64_t>( data.size() ) );
  buffer_.append( data.data(), static_cast<size_t>( n ) );
  bytes_pushed_ += n;
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( buffer_.size() - start_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return string_view { buffer_ }.substr( start_ );
}

void Reader::pop( uint64_t len )
{
  const uint64_t n = min( len, bytes_buffered() );
  start_ += n;
  bytes_popped_ += n;
  if ( start_ > 4096 && start_ * 2 >= buffer_.size() ) {
    buffer_.erase( 0, start_ );
    start_ = 0;
  }
}

bool Reader::is_finished() const
{
  return closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size() - start_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
```

- [ ] **Step 4: 再跑测试**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check_byte_stream'
```

Expected: PASS。再跑 `cmake --build build --target speed` 中的 `byte_stream_speed_test`；低于 0.1 Gbit/s 则减少 `pop` 时的频繁 `erase`（保留 `start_` 压缩策略即可）。

- [ ] **Step 5: 提交并打标签**

```bash
git add src/byte_stream.hh src/byte_stream.cc
git commit -m "feat: implement capacity-limited ByteStream"
git tag checkpoint-0-bytestream
```

---

### Task 4: webget

**Files:**

- Modify: `apps/webget.cc`
- Test: `tests/webget_t.sh` via `check_webget`

- [ ] **Step 1: 跑 `check_webget` 确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check_webget'
```

Expected: `webget returned output that did not match the test's expectations`

- [ ] **Step 2: 实现 `get_URL`**

替换 `apps/webget.cc` 中的 `get_URL`：

```cpp
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <sys/socket.h>

using namespace std;

void get_URL( const string& host, const string& path )
{
  TCPSocket sock;
  sock.connect( Address { host, "http" } );
  const string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  sock.write( req );
  sock.shutdown( SHUT_WR );
  while ( not sock.eof() ) {
    string buf;
    sock.read( buf );
    cout << buf;
  }
}
```

每行必须 `\r\n`；必须循环读到 EOF。

- [ ] **Step 3: 验证**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check_webget && cmake --build build --target check0'
```

Expected: PASS。若 DNS/外网失败：在 `docs/verification.md` 记为「外部 HTTP 待验证」，不要改测试去假装通过。

- [ ] **Step 4: 填 `writeups/check0.md` 并提交**

写明：deque/string 取舍、Stanford 身份/邮件作业不执行、webget 依赖外网。然后：

```bash
git add apps/webget.cc writeups/check0.md
git commit -m "feat: implement HTTP webget client"
git tag checkpoint-0
```

---

### Task 5: Reassembler

**Files:**

- Modify: `src/reassembler.hh`, `src/reassembler.cc`
- Test: `check1`

规则：流下标从 0；立即写入下一个字节；窗口外丢弃；内部不得存重叠副本；`is_last_substring` 记录结束下标，只有 `next_index == eof_index` 才 `close`；被容量裁掉的最后一段不能提前结束流。

- [ ] **Step 1: 跑 `check1` 确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check1'
```

Expected: FAIL on `reassembler_*`。

- [ ] **Step 2: 在 `reassembler.hh` 增加私有状态**

```cpp
#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <optional>
#include <string>

class Reassembler
{
public:
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  void insert( uint64_t first_index, std::string data, bool is_last_substring );
  uint64_t count_bytes_pending() const;

  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_;
  uint64_t next_index_ {};
  std::map<uint64_t, std::string> pending_ {};
  std::optional<uint64_t> eof_index_ {};
};
```

- [ ] **Step 3: 实现 `src/reassembler.cc`**

```cpp
#include "reassembler.hh"

#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    eof_index_ = first_index + data.size();
  }

  Writer& w = output_.writer();
  const uint64_t lo = next_index_;
  const uint64_t hi = next_index_ + w.available_capacity();

  if ( first_index < lo ) {
    const uint64_t skip = min<uint64_t>( data.size(), lo - first_index );
    data.erase( 0, skip );
    first_index += skip;
  }
  if ( first_index >= hi ) {
    data.clear();
  } else if ( data.size() > hi - first_index ) {
    data.resize( hi - first_index );
  }

  if ( not data.empty() ) {
    auto it = pending_.upper_bound( first_index );
    if ( it != pending_.begin() ) {
      auto prev = std::prev( it );
      if ( prev->first + prev->second.size() >= first_index ) {
        const uint64_t prev_end = prev->first + prev->second.size();
        if ( first_index + data.size() > prev_end ) {
          prev->second.append( data.substr( prev_end - first_index ) );
        }
        it = prev;
      } else {
        it = pending_.emplace_hint( it, first_index, move( data ) );
      }
    } else {
      it = pending_.emplace_hint( it, first_index, move( data ) );
    }

    auto nxt = std::next( it );
    while ( nxt != pending_.end() && it->first + it->second.size() >= nxt->first ) {
      const uint64_t cur_end = it->first + it->second.size();
      if ( nxt->first + nxt->second.size() > cur_end ) {
        it->second.append( nxt->second.substr( cur_end - nxt->first ) );
      }
      nxt = pending_.erase( nxt );
    }
  }

  while ( not pending_.empty() && pending_.begin()->first == next_index_ ) {
    auto node = pending_.extract( pending_.begin() );
    const uint64_t n = node.mapped().size();
    w.push( move( node.mapped() ) );
    next_index_ += n;
  }

  if ( eof_index_.has_value() && next_index_ >= *eof_index_ ) {
    w.close();
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t n = 0;
  for ( const auto& [_, s] : pending_ ) {
    n += s.size();
  }
  return n;
}
```

- [ ] **Step 4: 验证**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check1 && cmake --build build --target speed'
```

Expected: `check1` PASS；`reassembler_speed_test` ≥ 0.1 Gbit/s。

- [ ] **Step 5: 写 writeup 并提交**

```bash
git add src/reassembler.hh src/reassembler.cc writeups/check1.md
git commit -m "feat: implement stream reassembler"
git tag checkpoint-1
```

---

### Task 6: Wrap32

**Files:**

- Modify: `src/wrapping_integers.cc`
- Test: `wrapping_integers_*`（含在 `check2`）

- [ ] **Step 1: 先单独构建并跑 wrapping 测试确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build && ctest --test-dir build -R wrapping --output-on-failure'
```

Expected: FAIL。

- [ ] **Step 2: 实现 wrap/unwrap**

`src/wrapping_integers.cc`：

```cpp
#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint32_t checkpoint_wrapped = wrap( checkpoint, zero_point ).raw_value_;
  const int32_t offset = static_cast<int32_t>( raw_value_ - checkpoint_wrapped );
  const int64_t abs = static_cast<int64_t>( checkpoint ) + offset;
  if ( abs >= 0 ) {
    return static_cast<uint64_t>( abs );
  }
  return static_cast<uint64_t>( abs + ( int64_t { 1 } << 32 ) );
}
```

`wrap` 用 `zero_point + uint32_t(n)`。`unwrap` 用相对 checkpoint 的有符号 32-bit 最短距离；结果为负时加 `2^32`（例如 checkpoint=0 且答案是 `2^32-1`）。

- [ ] **Step 3: 再跑 wrapping 测试**

```bash
./scripts/dev.sh bash -lc 'ctest --test-dir build -R wrapping --output-on-failure'
```

Expected: PASS。

- [ ] **Step 4: 提交**

```bash
git add src/wrapping_integers.cc
git commit -m "feat: implement Wrap32 wrap and unwrap"
```

---

### Task 7: TCPReceiver

**Files:**

- Modify: `src/tcp_receiver.hh`, `src/tcp_receiver.cc`
- Test: `recv_*` via `check2`

规则：

- 未收到 SYN 之前忽略非 RST 段，ackno 为空。
- RST → `set_error()`；流错误 → 发送 RST。
- 流下标 = SYN 段的 payload 从 0 起；非 SYN 段 `stream_index = abs_seqno - 1`。
- FIN 表示该 payload 末字节是流末尾（即使前面有洞）。
- ackno = wrap(bytes_pushed + 1[+1 if writer closed])。
- window = min(available_capacity, UINT16_MAX)。

- [ ] **Step 1: 跑 `check2` 确认 receiver 失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check2'
```

Expected: wrapping 已过，`recv_*` FAIL。

- [ ] **Step 2: 加 ISN 状态并实现**

`tcp_receiver.hh` private:

```cpp
#include <optional>

Reassembler reassembler_;
std::optional<Wrap32> isn_ {};
```

`tcp_receiver.cc`:

```cpp
#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( not isn_.has_value() ) {
    if ( not message.SYN ) {
      return;
    }
    isn_ = message.seqno;
  }

  const uint64_t checkpoint = writer().bytes_pushed() + 1;
  const uint64_t abs_seqno = message.seqno.unwrap( *isn_, checkpoint );
  const uint64_t first_index = message.SYN ? abs_seqno : abs_seqno - 1;
  reassembler_.insert( first_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  const uint64_t cap = writer().available_capacity();
  msg.window_size = static_cast<uint16_t>( min( cap, uint64_t { UINT16_MAX } ) );
  msg.RST = reader().has_error();
  if ( isn_.has_value() ) {
    uint64_t abs_ack = writer().bytes_pushed() + 1;
    if ( writer().is_closed() ) {
      ++abs_ack;
    }
    msg.ackno = Wrap32::wrap( abs_ack, *isn_ );
  }
  return msg;
}
```

- [ ] **Step 3: 验证 `check2`**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check2'
```

Expected: PASS。

- [ ] **Step 4: 提交**

```bash
git add src/tcp_receiver.hh src/tcp_receiver.cc writeups/check2.md
git commit -m "feat: implement TCPReceiver and ack/window reporting"
git tag checkpoint-2
```

---

### Task 8: TCPSender

**Files:**

- Modify: `src/tcp_sender.hh`, `src/tcp_sender.cc`
- Test: `send_*` via `check3`

规则（讲义 + `send_extra.cc`）：

- 初始窗口 1。
- `push`：在窗口内尽量发，单段 payload ≤ `TCPConfig::MAX_PAYLOAD_SIZE`；SYN/FIN 占序号。
- 窗口为 0 时，`push` **假装**窗口为 1（零窗口探测）；不要把记住的窗口改成 1。非零但已满的窗口必须尊重。
- 只把 `sequence_length()>0` 的段放入 outstanding；按最早序号重传整段，不合并、不裁剪。
- 首次发送或重传占用序号的段时，若定时器没在跑则启动。
- 全部确认后停表。
- 超时：重传最早段；若**真实**窗口非 0，则连续重传计数 +1 且 RTO 加倍；然后按当前 RTO 重启定时器。
- 新的合法 ackno（大于旧 ackno 且 ≤ `next_abs_`）才重置 RTO 与连续重传计数。超出 `next_abs_` 的 ack 非法，不重置 RTO，但窗口字段仍更新。
- 无 ackno 的 receiver 消息仍更新窗口。
- 流错误或收到 RST：`set_error()`；发出的消息带 RST。
- 不要调用真实时钟。

- [ ] **Step 1: 跑 `check3` 确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check3'
```

Expected: FAIL on `send_*`。

- [ ] **Step 2: 扩展 `tcp_sender.hh`**

```cpp
#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

class TCPSender
{
public:
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), rto_ms_( initial_RTO_ms )
  {}

  TCPSenderMessage make_empty_message() const;
  void receive( const TCPReceiverMessage& msg );
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;
  void push( const TransmitFunction& transmit );
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  uint64_t sequence_numbers_in_flight() const;
  uint64_t consecutive_retransmissions() const;
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }
  TCPSenderMessage make_message( uint64_t abs_seqno, bool syn, std::string payload, bool fin ) const;
  void send_segment( const TCPSenderMessage& msg, const TransmitFunction& transmit );

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t rto_ms_;
  uint64_t next_abs_ {};
  uint64_t ack_abs_ {};
  uint64_t window_size_ { 1 };
  uint64_t consecutive_retransmissions_ {};
  bool timer_running_ {};
  uint64_t timer_remaining_ms_ {};
  bool syn_sent_ {};
  bool fin_sent_ {};
  std::deque<TCPSenderMessage> outstanding_ {};
};
```

- [ ] **Step 3: 实现 `src/tcp_sender.cc`**

```cpp
#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t n = 0;
  for ( const auto& seg : outstanding_ ) {
    n += seg.sequence_length();
  }
  return n;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

TCPSenderMessage TCPSender::make_message( uint64_t abs_seqno, bool syn, string payload, bool fin ) const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( abs_seqno, isn_ );
  msg.SYN = syn;
  msg.payload = move( payload );
  msg.FIN = fin;
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::send_segment( const TCPSenderMessage& msg, const TransmitFunction& transmit )
{
  transmit( msg );
  if ( msg.sequence_length() == 0 ) {
    return;
  }
  outstanding_.push_back( msg );
  if ( not timer_running_ ) {
    timer_running_ = true;
    timer_remaining_ms_ = rto_ms_;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return make_message( next_abs_, false, {}, false );
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( input_.has_error() ) {
    transmit( make_empty_message() );
    return;
  }

  const uint64_t effective_window = window_size_ == 0 ? 1 : window_size_;

  while ( true ) {
    const uint64_t inflight = sequence_numbers_in_flight();
    if ( inflight >= effective_window ) {
      return;
    }
    const uint64_t remaining = effective_window - inflight;

    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_abs_, isn_ );
    msg.RST = input_.has_error();

    uint64_t used = 0;
    if ( not syn_sent_ ) {
      msg.SYN = true;
      syn_sent_ = true;
      used += 1;
    }

    const uint64_t cap = min<uint64_t>( remaining - used, TCPConfig::MAX_PAYLOAD_SIZE );
    string payload;
    read( reader(), cap, payload );
    used += payload.size();
    msg.payload = move( payload );

    if ( reader().is_finished() && not fin_sent_ && used < remaining ) {
      msg.FIN = true;
      fin_sent_ = true;
      used += 1;
    }

    if ( used == 0 ) {
      return;
    }

    send_segment( msg, transmit );
    next_abs_ += msg.sequence_length();
  }
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    writer().set_error();
    return;
  }

  window_size_ = msg.window_size;

  if ( not msg.ackno.has_value() ) {
    return;
  }

  const uint64_t abs_ack = msg.ackno->unwrap( isn_, next_abs_ );
  if ( abs_ack > next_abs_ ) {
    return;
  }

  const bool new_ack = abs_ack > ack_abs_;
  if ( new_ack ) {
    ack_abs_ = abs_ack;
    while ( not outstanding_.empty() ) {
      const auto& seg = outstanding_.front();
      const uint64_t seg_abs = seg.seqno.unwrap( isn_, next_abs_ );
      if ( seg_abs + seg.sequence_length() <= ack_abs_ ) {
        outstanding_.pop_front();
      } else {
        break;
      }
    }
    consecutive_retransmissions_ = 0;
    rto_ms_ = initial_RTO_ms_;
    if ( outstanding_.empty() ) {
      timer_running_ = false;
    } else {
      timer_running_ = true;
      timer_remaining_ms_ = rto_ms_;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( not timer_running_ ) {
    return;
  }
  if ( timer_remaining_ms_ > ms_since_last_tick ) {
    timer_remaining_ms_ -= ms_since_last_tick;
    return;
  }

  timer_remaining_ms_ = 0;
  if ( outstanding_.empty() ) {
    timer_running_ = false;
    return;
  }

  transmit( outstanding_.front() );
  if ( window_size_ != 0 ) {
    ++consecutive_retransmissions_;
    rto_ms_ *= 2;
  }
  timer_running_ = true;
  timer_remaining_ms_ = rto_ms_;
}
```

注意：`read()` 来自 `byte_stream.hh` 提供的 helper。零窗口探测走 `effective_window = 1`，但超时指数退避只看真实 `window_size_ != 0`。

- [ ] **Step 4: 验证 `check3`**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check3'
```

Expected: PASS。若个别 `send_extra` 失败，先核对这些点再改：非法 ack 是否误重置 RTO；`push` 在错误流上是否仍发出 RST；FIN 是否在窗口满时被推迟；SYN+payload 是否占用窗口。

- [ ] **Step 5: 提交**

```bash
git add src/tcp_sender.hh src/tcp_sender.cc writeups/check3.md
git commit -m "feat: implement TCPSender window, ARQ, and retransmission"
git tag checkpoint-3
```

---

### Task 9: Checkpoint 3 应用路径（能跑则跑，失败单列）

**Files:**

- Modify: `docs/verification.md`, `writeups/check3.md`

- [ ] **Step 1: 在容器中列出应用**

```bash
CS144_NET=1 ./scripts/dev.sh bash -lc 'cmake --build build && ls build/apps'
```

Expected: `tcp_native`, `tcp_ipv4` 等存在。

- [ ] **Step 2: 尝试讲义中的本机回环路径**

按 `docs/assignments/check3.pdf` 4.1 节，在同一容器里让 Linux TCP 自对话、再让 minnow TCP 与 Linux TCP 对话。TUN 需要 `CS144_NET=1`。

若缺少 TAP/权限/外网：把命令、错误和「未验证」写入 `docs/verification.md`，不要改 util 去绕过 Linux 设备。

- [ ] **Step 3: 提交验证记录**

```bash
git add docs/verification.md writeups/check3.md
git commit -m "docs: record checkpoint 3 application-path verification status"
```

---

### Task 10: Checkpoint 4 测量工具（不冒充长时观测）

**Files:**

- Create: `scripts/ping_analyze.py`, `testdata/ping_sample.txt`, `writeups/check4.md`
- Modify: `docs/verification.md`

原讲义把 `5 × 3600` 写成约 3600；实现必须用实际发送/接收计数，不能用 3600。

- [ ] **Step 1: 写带自检的分析脚本**

`scripts/ping_analyze.py` 解析 `ping -D -n -i 0.2` 风格输出（`[unix] ... icmp_seq=N ttl=.. time=.. ms`），计算：投递率、最长成功/丢失串、k∈[-10,10] 的丢失自相关、RTT min/max、相邻 RTT 相关系数。用标准库；图写成简单 SVG（CDF、RTT 时间序列）。`python3 scripts/ping_analyze.py testdata/ping_sample.txt` 必须退出码 0。

样例 `testdata/ping_sample.txt` 至少包含：seq 0,1,2 成功，缺 3，4 成功；时间戳递增。脚本断言缺号被计为丢失，且投递率 = 4/5。

- [ ] **Step 2: 跑自检**

```bash
./scripts/dev.sh bash -lc 'python3 scripts/ping_analyze.py testdata/ping_sample.txt'
```

Expected: PASS（打印投递率 0.8，最长丢失 1）。

- [ ] **Step 3: 写操作说明，不跑隐含 1 小时任务**

`writeups/check4.md` 写明：三条路径、每条 ≥1 小时、`ping -D -n -i 0.2`、不要超过该频率对公网做压力测试。真实采集在用户指定目标与时段后再做。状态：工具完成 / 样例通过 / 长时公网未验证。

- [ ] **Step 4: 提交**

```bash
git add scripts/ping_analyze.py testdata/ping_sample.txt writeups/check4.md docs/verification.md
git commit -m "feat: add checkpoint 4 ping analysis tool and sample check"
git tag checkpoint-4
```

---

### Task 11: NetworkInterface

**Files:**

- Modify: `src/network_interface.hh`, `src/network_interface.cc`
- Test: `check5` / `net_interface`

规则：

- 已知下一跳以太网地址则立刻发 IPv4 帧。
- 未知则广播 ARP 请求并排队；同一 IP 5 秒内不重复请求。
- 忽略非本机且非广播的帧。
- IPv4：`parse` 成功则入 `datagrams_received_`。
- ARP：学习 sender 映射 30 秒；若是问本机 IP 的请求则回复。
- 不实现 ARP 重试/ICMP/排队超时。

- [ ] **Step 1: 跑 `check5` 确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check5'
```

Expected: FAIL。

- [ ] **Step 2: 加缓存状态**

`network_interface.hh` private 增加：

```cpp
#include "arp_message.hh"

#include <map>
#include <vector>

struct ARPEntry
{
  EthernetAddress eth {};
  size_t ttl_ms { 30000 };
};

std::map<uint32_t, ARPEntry> arp_table_ {};
std::map<uint32_t, size_t> arp_wait_ms_ {};
std::map<uint32_t, std::vector<InternetDatagram>> pending_datagrams_ {};
```

- [ ] **Step 3: 实现三个方法**

在 `network_interface.cc` 用已有 `serialize`/`parse`/`ETHERNET_BROADCAST`/`Address::ipv4_numeric()`：

```cpp
static constexpr size_t ARP_RETX_MS = 5000;
static constexpr size_t ARP_TTL_MS = 30000;

void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_ip = next_hop.ipv4_numeric();
  if ( auto it = arp_table_.find( next_ip ); it != arp_table_.end() ) {
    EthernetFrame frame;
    frame.header.src = ethernet_address_;
    frame.header.dst = it->second.eth;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize( dgram );
    transmit( frame );
    return;
  }

  pending_datagrams_[next_ip].push_back( dgram );
  if ( arp_wait_ms_.contains( next_ip ) ) {
    return;
  }
  arp_wait_ms_[next_ip] = ARP_RETX_MS;
  ARPMessage arp;
  arp.opcode = ARPMessage::OPCODE_REQUEST;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = {};
  arp.target_ip_address = next_ip;
  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = ETHERNET_BROADCAST;
  frame.header.type = EthernetHeader::TYPE_ARP;
  frame.payload = serialize( arp );
  transmit( frame );
}

void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( move( dgram ) );
    }
    return;
  }

  if ( frame.header.type != EthernetHeader::TYPE_ARP ) {
    return;
  }

  ARPMessage arp;
  if ( not parse( arp, frame.payload ) ) {
    return;
  }

  arp_table_[arp.sender_ip_address] = ARPEntry { arp.sender_ethernet_address, ARP_TTL_MS };
  arp_wait_ms_.erase( arp.sender_ip_address );

  if ( auto it = pending_datagrams_.find( arp.sender_ip_address ); it != pending_datagrams_.end() ) {
    for ( const auto& dgram : it->second ) {
      send_datagram( dgram, Address::from_ipv4_numeric( arp.sender_ip_address ) );
    }
    pending_datagrams_.erase( it );
  }

  if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
    ARPMessage reply;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ethernet_address = ethernet_address_;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.target_ethernet_address = arp.sender_ethernet_address;
    reply.target_ip_address = arp.sender_ip_address;
    EthernetFrame out;
    out.header.src = ethernet_address_;
    out.header.dst = arp.sender_ethernet_address;
    out.header.type = EthernetHeader::TYPE_ARP;
    out.payload = serialize( reply );
    transmit( out );
  }
}

void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if ( it->second.ttl_ms <= ms_since_last_tick ) {
      it = arp_table_.erase( it );
    } else {
      it->second.ttl_ms -= ms_since_last_tick;
      ++it;
    }
  }
  for ( auto it = arp_wait_ms_.begin(); it != arp_wait_ms_.end(); ) {
    if ( it->second <= ms_since_last_tick ) {
      it = arp_wait_ms_.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      ++it;
    }
  }
}
```

学习映射后调用 `send_datagram` 会走缓存命中分支。`#include "helpers.hh"`。

- [ ] **Step 4: 验证**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check5'
```

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add src/network_interface.hh src/network_interface.cc writeups/check5.md
git commit -m "feat: implement Ethernet/IP network interface and ARP cache"
git tag checkpoint-5
```

---

### Task 12: Router

**Files:**

- Modify: `src/router.hh`, `src/router.cc`
- Test: `check6`（内部会跑 `net_interface` + `router`）

规则：保存全部路由；匹配最高 `prefix_length`；无匹配或 TTL 已 0/减到 0 则丢弃；直接路由的 next hop 是目的地址；TTL 变化后 `compute_checksum()`。`prefix_length==0` 匹配一切；**禁止**对 32-bit 整数左移 32 位。

- [ ] **Step 1: 跑 `check6` 确认失败**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check6'
```

Expected: `net_interface` 已过，`router` FAIL。

- [ ] **Step 2: 加路由表并实现**

`router.hh`:

```cpp
struct Route
{
  uint32_t route_prefix {};
  uint8_t prefix_length {};
  std::optional<Address> next_hop {};
  size_t interface_num {};
};

std::vector<Route> routes_ {};
```

`router.cc`：

```cpp
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  routes_.push_back( { route_prefix, prefix_length, next_hop, interface_num } );
}

static bool prefix_match( uint32_t addr, uint32_t prefix, uint8_t len )
{
  if ( len == 0 ) {
    return true;
  }
  const uint32_t mask = ~uint32_t { 0 } << ( 32 - len );
  return ( addr & mask ) == ( prefix & mask );
}

void Router::route()
{
  for ( auto& iface : interfaces_ ) {
    auto& q = iface->datagrams_received();
    while ( not q.empty() ) {
      InternetDatagram dgram = move( q.front() );
      q.pop();

      optional<Route> best;
      for ( const auto& r : routes_ ) {
        if ( prefix_match( dgram.header.dst, r.route_prefix, r.prefix_length ) ) {
          if ( not best.has_value() || r.prefix_length > best->prefix_length ) {
            best = r;
          }
        }
      }
      if ( not best.has_value() ) {
        continue;
      }
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      --dgram.header.ttl;
      dgram.header.compute_checksum();
      const Address hop = best->next_hop.has_value() ? *best->next_hop
                                                     : Address::from_ipv4_numeric( dgram.header.dst );
      interface( best->interface_num )->send_datagram( dgram, hop );
    }
  }
}
```

- [ ] **Step 3: 验证**

```bash
./scripts/dev.sh bash -lc 'cmake --build build --target check6'
```

Expected: PASS。

- [ ] **Step 4: 提交**

```bash
git add src/router.hh src/router.cc writeups/check6.md
git commit -m "feat: implement longest-prefix-match IP router"
git tag checkpoint-6
```

---

### Task 13: Checkpoint 7 整栈联调

**Files:**

- Create: `scripts/local_relay.py`（仅在官方 relay 失败时）
- Modify: `writeups/check7.md`, `docs/verification.md`

- [ ] **Step 1: 构建 sanitized apps**

```bash
CS144_NET=1 ./scripts/dev.sh bash -lc 'cmake -S . -B build -G Ninja -DSANITIZED_APPS=True && cmake --build build'
```

- [ ] **Step 2: 先按讲义试官方 relay 自身双端**

```text
./build/apps/endtoend server cs144.keithw.org EVEN
./build/apps/endtoend client cs144.keithw.org ODD
```

EVEN 为 1024–64000 的随机偶数。成功后再传 1MiB 并 `sha256sum` 对比。

- [ ] **Step 3: 若 relay 不可达，加最小本地 UDP 中继**

`scripts/local_relay.py`：监听两个端口，把收到的 payload 原样转到对端。在 `writeups/check7.md` 标明这不是课程原始环境。自身双端成功 ≠ 与另一独立实现互通；无同伴则 Group portion 保持未验证。

- [ ] **Step 4: 填 writeup 并提交**

```bash
git add scripts/local_relay.py writeups/check7.md docs/verification.md
git commit -m "docs: record checkpoint 7 end-to-end verification status"
git tag checkpoint-7
```

不要为联调修改 `util/` 或测试来「制造通过」。

---

### Task 14: 中文导读与验证总表

**Files:**

- Create: `docs/overview.md`
- Modify: `docs/verification.md`, `README.md`（在课程 README 之上增加本地学习说明，保留不公开解答的原文）

- [ ] **Step 1: 写 `docs/overview.md`**

用一次 HTTP 请求串起：应用字节 → ByteStream → TCP 段（seqno/ack/window）→ IP 数据报 → Ethernet/ARP 帧 → Router → 对端反向路径。明确四层各装什么、实验模块落在哪一层。C++ 只点到 `string_view`/`optional`/继承切 Reader-Writer；Go 对照仅限「切片 vs string_view」「map 合并区间」。

- [ ] **Step 2: 写 `docs/verification.md` 三列状态表**

对 Checkpoint 0–7 逐项填写：实现完成 / 本地验证通过（命令、日期、g++ 版本、结果）/ 外部待验证。没有跑过的命令不得写「通过」。

- [ ] **Step 3: 全量再跑可本地检查**

```bash
./scripts/dev.sh bash -lc 'g++ --version && cmake --build build --target check0 && cmake --build build --target check_webget && cmake --build build --target check1 && cmake --build build --target check2 && cmake --build build --target check3 && cmake --build build --target check5 && cmake --build build --target check6 && cmake --build build --target speed && python3 scripts/ping_analyze.py testdata/ping_sample.txt'
```

把实际输出摘要记入 `docs/verification.md`。

- [ ] **Step 4: 提交**

```bash
git add docs/overview.md docs/verification.md README.md
git commit -m "docs: add panorama guide and verification ledger"
```

---

## Self-review

**Spec coverage:**

- 版本/归档/哈希 → Task 1
- 容器与 C++20/g++13 记录 → Task 2、14
- CP0 ByteStream+webget，斯坦福身份/邮件不执行 → Task 3–4
- CP1 Reassembler+速度 → Task 5
- CP2 Wrap32+Receiver → Task 6–7
- CP3 Sender+应用路径单列 → Task 8–9
- CP4 工具/样例/不冒充 1 小时观测/不沿用 3600 笔误 → Task 10
- CP5 ARP 网卡 → Task 11
- CP6 路由/TTL/移位 UB → Task 12
- CP7 自身双端 vs 同伴 vs 官方 relay → Task 13
- 中文导读、状态三分、不公开解答、主线+标签 → Task 1/14 与各提交

**Placeholders:** 无 TBD；每个实现步骤含完整代码与命令。

**Type consistency:** `next_index_`/`next_abs_`/`ack_abs_`/`window_size_`/`arp_table_`/`routes_` 在后续任务中与首次定义一致。
