这个是 **CS144 Winter 2025 的 Checkpoint 3：实现 TCP Sender**。它和前几个 checkpoint 的关系很清楚：Checkpoint 0 做 `ByteStream`，Checkpoint 1/2 做接收方向的 `Reassembler` 和 `TCPReceiver`，而这一关开始做发送方向的 `TCPSender`。 check3

你可以把整个作业理解成一句话：

> **把一个 outgoing ByteStream 切成 TCP segments，在接收方窗口允许的范围内发送；记录未确认的数据；超时后重传。**

## 1. 你到底要实现什么

主要实现 `src/tcp_sender.hh` 和 `src/tcp_sender.cc` 里的 `TCPSender`。它需要完成 4 类事情。 check3

### ① `push()`: 尽量把发送窗口填满

```
void push(const TransmitFunction& transmit);
```

它需要：

- 从 outbound `ByteStream` 读取数据
- 切成 `TCPSenderMessage`
- 给 segment 填正确的 sequence number
- 必要时设置：
    - `SYN`
    - payload
    - `FIN`
- 调 `transmit(message)` 发出去
- 一直发到：
    - receiver window 满了，或者
    - ByteStream 没数据了

每个 segment 应该尽可能大，但 payload 不能超过：

```
TCPConfig::MAX_PAYLOAD_SIZE
```

而且这里最容易犯的错是：

> **TCP window 计算的是 sequence space，不只是 payload 字节。**

因为：

- SYN 占 1 个 sequence number
- 每个 payload byte 占 1 个
- FIN 也占 1 个

文档专门提醒要用：

```
TCPSenderMessage::sequence_length()
```

来计算一个 segment 到底占多少 sequence numbers。 check3

---

### ② `receive()`: 处理 ACK 和接收窗口

```
void receive(const TCPReceiverMessage& msg);
```

receiver 会告诉 sender：

```
ackno
window_size
```

因此你要更新发送窗口，并检查 outstanding segments：

```
哪些 segment 已经被完全 ACK？
```

已经完全 ACK 的，就从 outstanding 队列中删除。 check3

这里有一个很重要的简化规则：

> **部分 ACK 一个 segment 时，不需要把 segment 裁剪。**

例如你发了：

```
segment:
seq = 100
payload = "abcdef"
```

如果 ACK 到：

```
ack = 103
```

虽然 `"abc"` 实际已经收到了，但这个作业要求你仍然把整个 segment 当 outstanding。

只有当：

```
ackno > 这个 segment 占据的所有 sequence numbers
```

才能整个删掉。 check3

这会让实现简单很多。

---

### ③ `tick()`: 实现 retransmission timer

```
void tick(uint64_t ms_since_last_tick,
          const TransmitFunction& transmit);
```

这是本次作业最核心、最容易出 bug 的部分。

你不能调用系统时钟：

```
std::chrono
gettimeofday()
clock()
```

之类。

**唯一允许的时间来源就是 `tick(ms_since_last_tick)`。**

你自己维护类似：

```
uint64_t current_time_ms;
```

每次：

```
current_time_ms += ms_since_last_tick;
```

文档明确要求这样做，因为测试必须 deterministic。 check3

---

### ④ `make_empty_message()`

```
TCPSenderMessage make_empty_message() const;
```

生成一个：

```
payload = empty
SYN = false
FIN = false
```

但：

```
seqno 必须正确
```

这种 message 不占 sequence space，所以：

- 不进入 outstanding 队列
- 不需要 retransmit

check3

---

# 2. 这个作业真正难的地方：sender state

我建议你不要一上来写代码，先把 `TCPSender` 想成一个状态机。

你大概率至少需要维护这些状态：

```
ISN
next_seqno
acknowledged_seqno

receiver_window_size

outstanding_segments

initial_RTO
current_RTO

timer_running
timer_elapsed

consecutive_retransmissions
```

其中最重要的是区分：

```
next_seqno
```

和

```
ackno
```

假设：

```
receiver ACK 到 100
sender 已经发送到 150
```

那么：

```
[0, 100)   已 ACK
[100, 150) outstanding
[150, ...) 还没发送
```

sender 当前占用了：

```
bytes_in_flight = next_seqno - acked_seqno
```

所以还能发送：

```
effective_window - bytes_in_flight
```

这其实就是整个 `push()` 的核心数学关系。

---

# 3. 发送窗口不是“window_size 个新字节”

这是一个很值得特别注意的坑。

receiver 告诉你：

```
ACK = A
window size = W
```

表示它接受的 sequence-number range 大致是：

```
[A, A + W)
```

假设：

```
ACK = 10
window = 5
```

receiver 接受：

```
seq 10
11
12
13
14
```

但如果你已经发了：

```
10 11 12
```

还没 ACK，那么并不是还能发 5 个，而是只能继续发：

```
13 14
```

所以：

```
window_size != 当前还能发送多少
```

文档也特别提醒：

> window size 非零，不代表 window 没满。 check3

---

# 4. zero-window 是本题特别重要的特殊情况

receiver 可能告诉你：

```
window_size = 0
```

正常想法会是：

```
那什么都不能发。
```

但 TCP 会因此死锁：

```
receiver window = 0
        ↓
sender 不再发
        ↓
receiver 后来有空间了
        ↓
但 sender 不知道
```

因此作业要求：

### 在 `push()` 内临时把 window=0 当成 window=1

也就是允许发一个 sequence number 进行 probe。

但非常重要：

> **不要真的把保存的 receiver window 改成 1。**

只是计算 push 时：

```
effective_window = window_size == 0 ? 1 : window_size;
```

文档明确说这是 **zero-size window 唯一的特殊行为**。 check3

这个很可能有专门测试。

---

# 5. Retransmission timer 怎么实现

这是 Checkpoint 3 的重点。

初始有：

```
initial_RTO
```

当前：

```
RTO = initial_RTO
```

只要有第一个 outstanding segment，就启动 timer。 check3

假设：

```
RTO = 1000 ms
```

发了 segment A：

```
t = 0
```

然后：

```
tick(400)
tick(300)
tick(299)
```

累计：

```
999ms
```

不重传。

再：

```
tick(1)
```

达到 1000：

```
retransmit A
```

---

## 超时时到底重传哪个？

不是全部重传。

只重传：

> **earliest outstanding segment**

也就是 sequence number 最小的那个。 check3

所以数据结构最自然的是：

```
std::deque<OutstandingSegment>
```

或类似有序结构。

通常：

```
front() = oldest / earliest unacked segment
```

---

# 6. Exponential Backoff

如果发生 timeout，而且：

```
receiver window > 0
```

你要做：

```
consecutive_retransmissions++;
RTO *= 2;
```

然后重新启动 timer。 check3

例如：

```
initial RTO = 1000

第一次 timeout:
1000 ms
重传
RTO -> 2000

第二次 timeout:
2000 ms
重传
RTO -> 4000

第三次:
4000 ms
...
```

这就是：

```
exponential backoff
```

---

# 7. 但 zero-window 情况不要 exponential backoff

这里非常容易漏。

文档写的是：

```
If the window size is nonzero:
    increment retransmissions
    double RTO
```

所以 receiver window 为 0 时，即使 timeout 后发送 probe：

```
不要：
consecutive_retransmissions++
```

也不要：

```
RTO *= 2
```

check3

这很可能也是 hidden test / unit test 的重点。

---

# 8. 收到“新的 ACK”时 timer 怎么变化

这是另一个高频 bug 点。

如果 receiver ACK 了**新的数据**：

```
new_ack > previous_ack
```

则：

```
RTO = initial_RTO;
consecutive_retransmissions = 0;
```

如果还有 outstanding segment：

```
restart timer
```

从头等待：

```
initial_RTO
```

如果已经没有 outstanding：

```
stop timer
```

check3

注意是：

> **ACK acknowledges new data**

不是收到任何 ACK 都 reset。

duplicate ACK：

```
ack = previous_ack
```

不能算新 ACK。

---

# 9. 哪些 segment 要进入 outstanding queue？

只有：

```
message.sequence_length() > 0
```

的 segment。

也就是包含至少一个：

```
SYN
payload byte
FIN
```

例如：

```
SYN=0
FIN=0
payload=""
```

sequence length：

```
0
```

这种不能存 outstanding，也不能重传。 check3

---

# 10. SYN / FIN 是非常容易写错的地方

假设 initial sequence number：

```
ISN = 100
```

第一次发送：

```
SYN = true
seqno = 100
```

SYN 本身占：

```
sequence number 100
```

所以第一个 payload byte 的 absolute sequence number 实际相当于：

```
101
```

类似地：

```
FIN
```

也占一个 sequence number。

例如：

```
SYN + "abc" + FIN
```

sequence length 是：

```
1 + 3 + 1 = 5
```

而不是 3。

文档专门强调了这一点。 check3

---

# 11. 我建议你实现时先确定几个 invariant

这个作业很适合用 invariant 来写。

### Invariant 1

```
next_seqno
=
所有已经发送过的新 sequence space 的右边界
```

重传不能增加：

```
next_seqno
```

因为它不是新数据。

---

### Invariant 2

outstanding queue 里的 segment：

```
已经发送
但是没有被完全 ACK
```

---

### Invariant 3

如果：

```
outstanding.empty()
```

那么：

```
timer must not be running
```

---

### Invariant 4

如果：

```
!outstanding.empty()
```

通常：

```
outstanding.front()
```

就是 timeout 时要重传的 segment。

---

### Invariant 5

```
bytes_in_flight
```

等于 outstanding sequence space 的总长度。

很多实现会直接维护：

```
uint64_t sequence_numbers_in_flight_;
```

这样 `push()` 非常方便。

---

# 12. 你最终还要完成 Hands-on Activity

这个 PDF 不只是让你过 unit test。

它还要求你实际验证自己的 TCP 能和 Linux TCP 通信。 check3

大概包括：

### Linux TCP ↔ Linux TCP

先确认测试环境正常。

然后：

### 你的 TCP ↔ Linux TCP

使用：

```
./build/apps/tcp_ipv4
```

和：

```
./build/apps/tcp_native
```

互相通信。

必要时用：

```
tcpdump
wireshark
```

debug。 check3

---

# 13. 还有一个文件传输挑战

文档让你测试：

```
12 bytes
65534 bytes
65537 bytes
200000 bytes
1000000 bytes
```

然后比较：

```
sha256sum
```

确认发送前后的文件完全相同。 check3

这里实际上是在验证：

```
segmentation
sequence numbering
ACK
window
retransmission
FIN
```

这些组合起来是不是真的正确。

---

# 14. 最后还要修改 `webget.cc`

Checkpoint 0 的 `webget` 原来用 Linux TCP。

现在要换成你自己的 TCP stack。

文档要求主要改：

```
#include "socket.hh"
```

变成：

```
#include "tcp_minnow_socket.hh"
```

并把：

```
TCPSocket
```

换成：

```
CS144TCPSocket
```

最后调用：

```
socket.wait_until_closed();
```

然后：

```
make check_webget
```

验证你的 HTTP client 可以跑在**自己实现的 TCP 上**。 check3

这个其实是整个 CS144 前几个 lab 非常漂亮的闭环：

```
HTTP
 ↓
your webget
 ↓
your TCP sender + receiver
 ↓
IPv4
 ↓
Internet
```

---

# 15. 提交时特别注意

只能修改：

```
src/*.hh
src/*.cc
apps/webget.cc
```

不要改 public interface。 check3

提交前按顺序：

```
git status
cmake --build build --target format
cmake --build build --target check3
cmake --build build --target tidy   # optional
```

check3

另外还要写：

```
writeups/check3.md
```

大约：

```
20–50 lines
```

包括：

```
Program Structure and Design
Alternative design choices
Implementation Challenges
Remaining Bugs
Hands-on Activity
```

以及花了多少小时。 check3

---

# 16. 如果你准备开始写，我认为最值得先想清楚的是这张图

```
                    receiver advertises
                 ACK=A, window=W
                        │
                        ▼

sequence space:

0 ---------------- A ---------------- N ------------- A+W
     ACKed            outstanding       available
                      already sent       to send

                      <---->
                  bytes_in_flight

                  N = next_seqno
```

然后：

```
available window
=
effective_window - bytes_in_flight
```

其中：

```
effective_window =
    receiver_window == 0 ? 1 : receiver_window;
```

`push()` 做的事情，本质就是：

```
while available_window > 0:
    构造最大的合法 segment
    transmit
    放入 outstanding
    next_seqno += sequence_length
```

`receive()`：

```
推进左边界 A
删除 fully ACKed segments
更新 W
必要时 reset/restart timer
```

`tick()`：

```
如果 timer 过期：
    retransmit outstanding.front()
    根据 window 是否为 0 决定是否 backoff
```

**所以这次作业真正考的不是 C++，而是你能不能维护好 TCP sender 的几个状态和不变量。**

尤其先盯住这 6 个坑：

1. `SYN/FIN` 都占 sequence number。
2. `window_size` 不是“还能发送多少字节”。
3. zero window 在 `push()` 中临时按 1 处理。
4. timeout 只重传最早的 outstanding segment。
5. duplicate ACK 不能 reset RTO。
6. zero-window timeout 不做 exponential backoff。

如果这几个点理解透了，Checkpoint 3 的主体设计其实会比较清晰。