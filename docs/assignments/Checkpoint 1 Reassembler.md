这个作业是 **Stanford CS144 Winter 2025 的 Checkpoint 1**。核心主题可以浓缩成一句话：

> **你要实现一个 `Reassembler`，把乱序、重复、重叠到达的字符串片段，重新拼成一个连续的字节流。**

它其实是在给后面自己实现 TCP Receiver 打基础。TCP 收到的 segment 可能乱序、丢失、重复，但最终要给应用程序提供一个“可靠、按顺序”的 byte stream。 check1

我建议你把这个作业分成两个部分来看。

### 1. Hands-on 网络实验

前半部分要求你加入课程的 WireGuard 私网，然后和同学互相：

- `ping`
- 用 `tcpdump` 抓包
- 用 Wireshark 看 IP datagram
- 比较发送端和接收端抓到的 packet
- 自己用 raw socket 构造 IP datagram
- 甚至手工构造一个 UDP datagram

例如要求至少发 1000 次 ping，统计 RTT、delivery rate、loss rate、有没有重复包。 check1

然后还要修改：

```
apps/ip_raw.cc
```

自己发：

```
IP protocol = 5
```

以及：

```
IP protocol = 17  // UDP
```

让对方确实能够收到。 check1

如果你只是自己学习 CS144、不是正式上课，这一部分的现实意义主要是帮你直观理解：

```
application
    ↓
UDP / ICMP
    ↓
IP datagram
    ↓
WireGuard / Internet
```

真正后续 lab 最重要的是下面的 Reassembler。

---

# 2. 真正核心：实现 `Reassembler`

接口大概是：

```
void insert(uint64_t first_index,
            std::string data,
            bool is_last_substring);

uint64_t count_bytes_pending() const;

Reader& reader();
```

check1

你每次会收到：

```
first_index = 5
data = "world"
```

意思是：

```
stream index:
0 1 2 3 4 5 6 7 8 9
          w o r l d
```

问题在于数据不一定按顺序到。

例如真实 stream：

```
abcdefghij
```

可能这样到：

```
insert(5, "fgh", false)
insert(0, "abc", false)
insert(3, "de", false)
insert(8, "ij", true)
```

你的 Reassembler 必须最终输出：

```
abcdefghij
```

而不能输出：

```
fghabcdeij
```

---

# 最核心的思想：只有“连续的数据”才能 push

假设现在你已经输出：

```
abc
```

那么：

```
next expected index = 3
```

这时收到：

```
index 5: "fg"
```

不能输出。

因为：

```
3, 4
```

还不知道。

所以先保存：

```
pending:
5 -> "fg"
```

之后收到：

```
index 3: "de"
```

突然变成：

```
3 4 5 6
d e f g
```

连续了。

于是一次性：

```
push("defg")
```

作业明确要求：只要某个 byte 已经可以按顺序输出，就应该 **尽快写进 ByteStream**。只有它前面的 byte 还没 push 时，才允许继续留在 Reassembler 里。 check1

---

# 这个作业最容易错的地方有 5 个

## ① overlap

输入允许重叠。

比如：

```
insert(0, "abcdef", false)
insert(3, "defghi", false)
```

它们表示：

```
abcdef
   defghi
```

最终只能是：

```
abcdefghi
```

不能把重复数据存两份。

作业甚至明确要求：

> Reassembler 内部不能保存 overlapping substrings 的重复副本。

因为否则 capacity 就失去作为内存上限的意义。 check1

所以你需要处理诸如：

```
已有:
[5, 10)

新来:
[7, 12)
```

合并成：

```
[5, 12)
```

而不是存：

```
[5,10)
[7,12)
```

---

## ② 已经输出过的数据要直接丢掉

假设：

```
first unassembled index = 10
```

意味着：

```
[0,10)
```

都已经进入 ByteStream。

这时又收到：

```
insert(5, "abcdefghi", false);
```

范围：

```
[5,14)
```

你不能重新处理：

```
[5,10)
```

因为这些 byte 已经处理过了。

真正有意义的只有：

```
[10,14)
```

所以你可以把每次输入理解成先做一个裁剪：

```
incoming interval
        ∩
currently acceptable interval
```

---

# ③ capacity 是这个 Lab 最重要的坑

PDF 第 6 页的图非常重要。

它把数据分成三类：

```
blue   = application 已经 pop 掉的
green  = ByteStream 里已经 assemble、但还没读掉的
red    = Reassembler 里等待前面缺口的
```

而 capacity 限制的是一个“窗口”。图里的关键位置是：

```
first unpopped index
first unassembled index
first unacceptable index
```

其中：

```
first unacceptable index
=
first unpopped index + capacity
```

换句话说，不是：

```
first_unassembled + capacity
```

这点非常容易写错。

PDF 明确说明 capacity 同时约束：

- ByteStream 已经 buffered 的 bytes
- Reassembler 里等待组装的 bytes

而超出窗口的数据必须直接丢掉。 check1

例如：

```
capacity = 10
```

现在 application 还没读取：

```
ByteStream buffered = 6 bytes
```

那么 Reassembler 实际最多只能再接受：

```
10 - 6 = 4 bytes
```

不是还能再存 10 bytes。

这通常是 Checkpoint 1 最容易出 bug 的地方。

---

# ④ `is_last_substring` 不等于“现在可以 close”

假设收到：

```
insert(10, "xyz", true);
```

它告诉你：

```
整个 stream 最后一字节的位置已经知道了
```

也就是总长度：

```
10 + 3 = 13
```

但是如果目前：

```
0..9
```

还缺数据，你不能马上：

```
writer().close();
```

必须等所有：

```
[0,13)
```

都真正 assembly 完。

例如：

```
收到:
10 xyz   ← last

但:
0 1 2 ? ? ? ? ? ? ? 10 11 12
```

现在不能 close。

等最后：

```
0...............12
全部连续
```

才能 close。

---

# ⑤ `count_bytes_pending()` 不是 substring 长度之和

例如：

```
[0,5)   abcde
[3,8)      defgh
```

如果你直接：

```
5 + 5 = 10
```

就错了。

真实 unique pending bytes 是：

```
abcdefgh
```

一共：

```
8
```

作业明确要求重复的 index 只能存一次，所以这个函数应统计 **unique、还未组装的数据 byte 数量**。 check1

---

# 我建议你先建立这三个变量的脑图

无论最后代码怎么实现，先理解：

```
0
│
├──────────── 已经 pop
│
first_unpopped
│
├──────────── ByteStream 中等待 reader 读取
│
first_unassembled
│
├──────────── Reassembler pending
│
first_unacceptable
```

或者：

```
0       reader 已读        已 assemble          capacity 边界
|-----------|------------------|----------------|
            ↑                  ↑                ↑
     first_unpopped     first_unassembled   first_unacceptable

              green               red
```

第 6 页那张图基本就是整个作业的核心规格。 check1

---

# 数据结构怎么选

PDF 没规定具体数据结构，只说标准库都可以，而且预期至少使用一个数据结构。 check1

你大体有两种比较自然的方向。

一种是：

```
std::map<uint64_t, std::string>
```

比如：

```
start index → substring
```

维护不重叠区间：

```
5  -> "abc"
12 -> "xyz"
20 -> "foo"
```

插入时：

```
裁剪
→ 找 overlap
→ merge
→ 删除旧 interval
→ 插入 merged interval
→ 从 first_unassembled 开始尽可能 push
```

这个思路比较符合 C++ 和实际 TCP reassembly 的感觉。

另一种是按 byte 存，例如：

```
std::map<uint64_t, char>
```

或者：

```
vector<optional<char>>
```

更容易理解，但性能和空间行为可能没那么漂亮。

作业对性能的要求其实并不苛刻：benchmark 超过 `0.1 Gbit/s` 就算可接受，优秀实现能到 `10 Gbit/s`。 check1

对你现在学 C++ 的阶段，我更建议：

```
先写正确
↓
再优化 interval merging
```

不要一开始为了追求 10 Gbit/s 把代码写得特别复杂。

---

# 推荐你把 `insert()` 拆成这个思考流程

以后你自己实现时，可以按下面这个 mental model：

```
insert(index, data, is_last)
        │
        ↓
1. 如果 is_last:
      记录 stream end index
        │
        ↓
2. 计算 incoming interval:
      [index, index + data.size())
        │
        ↓
3. 按 acceptable window 裁剪:
      [first_unassembled,
       first_unacceptable)
        │
        ↓
4. 如果裁剪后为空:
      什么都不存
        │
        ↓
5. 与已有 pending intervals merge
        │
        ↓
6. 如果 first_unassembled 有连续数据:
      push 到 ByteStream
        │
        ↓
7. 继续检查下一个 interval
        │
        ↓
8. 如果已经到 stream end:
      writer.close()
```

如果你的代码最终不是严格这么写没关系，但逻辑上基本绕不开这些步骤。

---

# 作业还特别要求你注意这些工程细节

只能修改 `src` 下的 `.hh/.cc`，可以增加 private members，但不能改 public interface。 check1

提交前至少跑：

```
cmake --build build --target format
cmake --build build --target check1
```

可选：

```
cmake --build build --target tidy
```

check1

另外老师很强调：

```
清晰命名
small commits
defensive programming
检查 invariant / precondition
避免巨大函数
避免重复代码
Modern C++
```

check1

而且最终还要写：

```
writeups/check1.md
```

大约 20–50 行，主要解释：

```
Structure and Design
Alternative design choices
Implementation Challenges
Remaining Bugs
```

尤其要解释你用了什么数据结构，以及有没有冗余状态。 check1 check1

---

## 你真正应该抓住的三个 invariant

如果是我带你做这个 Lab，我会要求你在写代码前先把这三个 invariant 写在纸上：

```
Invariant 1:
[0, first_unassembled)
已经全部按照正确顺序写入 ByteStream。
```

```
Invariant 2:
Reassembler 中存储的数据互不重叠，
并且全部位于 acceptable window 内。
```

```
Invariant 3:
只要 first_unassembled 对应的数据已知，
它就不能继续停留在 Reassembler，
必须立刻 push 到 ByteStream。
```

只要这三个一直成立，你的实现大概率就是对的。

而这个 Lab 真正在训练的，也不只是字符串拼接，而是：

```
乱序数据
+ sequence number/index
+ receive window / capacity
+ duplicate elimination
+ interval merging
+ contiguous delivery
```

这些概念后面会直接变成 TCP receiver 的核心逻辑。 check1

如果你准备开始写，我建议下一步先不要看答案。我们可以直接拿 **3～4 个 insert() 例子手算 Reassembler 内部状态变化**，把 `first_unassembled / first_unacceptable / pending` 彻底搞清楚，再开始写 C++。