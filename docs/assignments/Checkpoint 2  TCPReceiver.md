这个 Checkpoint 2 的核心任务是：**把你前面写好的 `ByteStream` 和 `Reassembler` 接到 TCP 的语义上，实现 TCP 接收端 `TCPReceiver`。** 换句话说，前两个 checkpoint 主要还是“通用字节流处理”，这一章才真正开始接触 TCP。 check2

你可以把整个作业拆成 **两个主要编程任务 + 一个报告**。

1. **实现 32-bit TCP sequence number 的 wrap / unwrap**

前面的 `Reassembler` 使用的是很简单的 64-bit stream index：

```
0, 1, 2, 3, 4, ...
```

但真实 TCP header 里的 sequence number 只有 **32 bit**，因此会发生 wrap-around：

```
... 4294967294
    4294967295
    0
    1
    2
```

而且 TCP sequence number **不是从 0 开始**，而是从随机的 ISN（Initial Sequence Number）开始。更重要的是，TCP 的 `SYN` 和 `FIN` 自己也各占一个 sequence number。 check2

所以这里要求你实现：

```
static Wrap32 Wrap32::wrap(
    uint64_t n,
    Wrap32 zero_point
);
```

也就是：

```
absolute sequence number
        ↓
32-bit TCP seqno
```

以及：

```
uint64_t Wrap32::unwrap(
    Wrap32 zero_point,
    uint64_t checkpoint
) const;
```

也就是：

```
32-bit TCP seqno
        ↓
absolute sequence number
```

其中 `unwrap()` 最容易出错，因为一个 32-bit seqno 可能对应多个 64-bit absolute seqno。

比如：

```
ISN = 0
seqno = 17
```

它可能代表：

```
17
2^32 + 17
2*2^32 + 17
3*2^32 + 17
...
```

所以作业额外给你一个：

```
checkpoint
```

要求你选择**距离 checkpoint 最近的那个 absolute sequence number**。在 TCPReceiver 中，这个 checkpoint 通常就是当前“第一个还没 assembled 的位置”。 check2

这里文档特别提示了一点：

> `wrap()` 大概一行；`unwrap()` 应该少于 10 行。

所以如果你写出了几十行复杂逻辑，通常说明思路绕远了。 check2

---

第二个任务，也是这个 checkpoint 真正的主体，是实现：

```
TCPReceiver
```

它其实像一个**协议适配层**：

```
TCPSenderMessage
      │
      ▼
 TCPReceiver
      │
      ├── seqno → stream index
      │
      ▼
 Reassembler
      │
      ▼
 ByteStream
```

与此同时，它还需要反过来告诉发送端：

```
ackno
window_size
RST
```

也就是：

```
             TCPReceiver
                  │
                  ▼
        TCPReceiverMessage
                  │
                  ▼
              TCPSender
```

文档甚至说这一部分预计大约只有 **15 行代码**，说明真正困难的不是代码量，而是你有没有把 sequence number 的语义想清楚。 check2

### `receive()` 需要做什么

当收到：

```
TCPSenderMessage
```

你需要处理这些字段：

```
Wrap32 seqno;
bool SYN;
std::string payload;
bool FIN;
bool RST;
```

其中一个 segment 占用的 sequence numbers 是：

```
SYN + payload.size() + FIN
```

所以：

```
SYN = 1
payload = "cat"  // 3
FIN = 1

总共占 5 个 seqno
```

check2

`receive()` 最关键的事情有两个：

- 第一次收到 `SYN` 时，记录 ISN。
- 把收到的 payload 转换成正确的 **stream index**，然后交给 `Reassembler`。

如果 segment 带有：

```
FIN = true
```

意味着：

> payload 的最后一个 byte 就是整个 ByteStream 的最后一个 byte。

check2

这里一个非常容易犯的错误是混淆这三套编号：

|概念|从哪里开始|SYN/FIN 是否占位置|是否 wrap|
|---|---|---|---|
|`seqno`|ISN|是|32-bit，会|
|absolute seqno|0|是|64-bit，不会|
|stream index|0|**否**|64-bit，不会|

check2

尤其注意：

```
absolute seqno 0 = SYN
absolute seqno 1 = stream byte 0
absolute seqno 2 = stream byte 1
```

因此通常：

```
stream_index = absolute_seqno - 1
```

这个 `-1` 非常重要。

---

### `send()` 要做什么

`send()` 需要生成：

```
TCPReceiverMessage
{
    std::optional<Wrap32> ackno;
    uint16_t window_size;
    bool RST;
}
```

check2

其中：

**`ackno` 表示“下一个我想要的 sequence number”。**

这不是：

```
最后成功收到的 byte
```

而是：

```
next sequence number needed
```

例如已经成功重组：

```
abc
```

那么接收端的意思其实是：

```
我下一步要 d
```

而不是：

```
我收到 c 了
```

如果还**没有收到 SYN**，则：

```
ackno = std::nullopt;
```

因为这时甚至还不知道 ISN 是多少。 check2

这里还有一个很重要的 FIN 细节。

假设：

```
SYN
abc
FIN
```

对应：

```
absolute seqno

SYN  abc   FIN
 0   1 2 3  4
```

当：

```
abc
```

已经完整 assembled，但 FIN 还没收到时：

```
ACK absolute = 4
```

收到 FIN 之后：

```
ACK absolute = 5
```

因为：

> FIN 自己也消耗一个 sequence number。

这是 Checkpoint 2 最容易出现 off-by-one bug 的地方之一。

---

### `window_size`

Receiver 还要告诉 sender：

```
我目前最多还能接受多少数据
```

也就是：

```
window_size
```

它来源于输出 `ByteStream` 的剩余 capacity，并且协议字段最大只能是：

```
UINT16_MAX
```

即：

```
65535
```

所以如果内部 capacity 比这个更大，也只能 advertise：

```
65535
```

check2

你可以把 ACK + window 理解成一个滑动窗口：

```
               receiver window

           ackno
             │
             ▼
─────────────┬──────────────────┬──────────
 already     │ sender may send  │ not wanted
 assembled   │                  │ yet
─────────────┴──────────────────┴──────────
             ▲                  ▲
          left edge         right edge
                        ackno + window
```

这就是 TCP flow control 的基础。文档明确说 ACK 表示“下一 byte 是什么”，window 表示“我愿意接受多大的范围”。 check2

---

## 你做这个作业时最需要注意的 7 个坑

第一，**一定区分 seqno / absolute seqno / stream index。** 这是整个 checkpoint 的核心。如果这三个混在脑子里，代码几乎必然出现 `±1` bug。

第二，**SYN 占一个 sequence number。** 所以 payload 的 stream index 往往需要：

```
absolute seqno - 1
```

第三，**FIN 也占一个 sequence number。** 但是 FIN **不是 ByteStream 里面的一个 byte**。它只是告诉 Reassembler：“这个 payload 后就是 EOF。”

第四，**32-bit seqno 会 wrap-around。** 不能简单写：

```
seqno.raw_value() - isn.raw_value()
```

然后认为永远正确，因为跨 `2^32` 边界就会出问题。

第五，`unwrap()` 必须使用 `checkpoint` 解决歧义。例如同一个：

```
seqno = 10
```

可能是：

```
10
2^32 + 10
2*2^32 + 10
```

需要选离 checkpoint 最近的一个。

第六，**没收到 SYN 之前不能发送有效 ACK。**

```
ackno = nullopt
```

这是协议语义，而不是随便给个 0。 check2

第七，别为了这个 checkpoint 重写 `Reassembler`。文档的设计其实是：

```
Checkpoint 0
ByteStream

Checkpoint 1
ByteStream
    ↑
Reassembler

Checkpoint 2
ByteStream
    ↑
Reassembler
    ↑
TCPReceiver
```

也就是说 Checkpoint 2 主要是在已有抽象之上加 TCP-specific translation。文档也明确说，大部分算法工作在前两个 checkpoint 已经做完了，这里主要是在把这些模块“接到 TCP 上”。 check2

---

最后还有几个**提交方面必须注意的要求**。

只修改：

```
src/*.hh
src/*.cc
```

可以增加 private member，但不要改类的 public interface。 check2

提交前按顺序跑：

```
git status

cmake --build build --target format
cmake --build build --target check2

# optional
cmake --build build --target tidy
```

check2

另外还要写：

```
writeups/check2.md
```

大约 **20–50 行**，每行不超过 80 字符，主要包含：

```
Program Structure and Design
Alternative design choices
Implementation Challenges
Remaining Bugs
耗时
其他 comments
```

check2

以及课程明确要求不要看其他学生代码或过去作业的 solution，并要求披露合作情况。 check2

如果从学习角度看，我建议你先不要直接写 `TCPReceiver`，而先确保自己能在纸上完全画清楚这一组转换：

```
                    SYN      c       a       t      FIN
absolute seqno       0       1       2       3       4
stream index                 0       1       2

seqno:
ISN
ISN+1
ISN+2
ISN+3
ISN+4
(mod 2^32)
```

**Checkpoint 2 本质上就是把这张表彻底搞懂，然后把它编码出来。**