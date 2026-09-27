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

---

## 实现学习笔记：把 Reassembler 按职责拆开

对应源码：[`reassembler.cc`](../../src/reassembler.cc) 和 [`reassembler.hh`](../../src/reassembler.hh)。环境配置与 Docker 测试命令见 [`README.md`](../../README.md)。

这次重构保持算法和公开接口不变，主要改善变量命名、减少嵌套，并拆出三个私有函数。目标不是让行数最少，而是让读者能先理解主流程，再分别理解每个步骤。

### 1. 四个成员分别表示什么？

```cpp
ByteStream output_;
uint64_t next_index_ {};
std::map<uint64_t, std::string> pending_ {};
std::optional<uint64_t> eof_index_ {};
```

| 成员 | 含义 |
| --- | --- |
| `output_` | 接收已经按顺序组装好的数据；写入不代表 reader 已读走 |
| `next_index_` | 下一个等待组装的字节下标，即 first unassembled |
| `pending_` | 起始下标 → 待组装字符串，按下标有序；重叠或相邻区间会被合并 |
| `eof_index_` | 流结束下标，即最后一个字节之后的位置；没有值表示尚未知晓 EOF |

例如 `pending_ = { {5, "fgh"}, {10, "kl"} }` 表示 `[5, 8)` 和 `[10, 12)`。区间左闭右开，长度等于结束下标减去起始下标。

`eof_index_` 不能简单用 0 表示未知，因为长度为 0 的空流也是合法输入。

### 2. `insert()`：只组织流程和处理 EOF

主流程可以读成：

```text
记录原始 EOF
    ↓
trim_to_window：裁剪窗口
    ↓
merge_pending：合并、去重区间
    ↓
push_contiguous：输出连续数据
    ↓
判断是否到达 EOF，关闭 writer
```

对应核心代码：

```cpp
if ( is_last_substring ) {
  eof_index_ = first_index + data.size();
}

trim_to_window( first_index, data );
merge_pending( first_index, move( data ) );
push_contiguous();

if ( eof_index_.has_value() && next_index_ >= *eof_index_ ) {
  output_.writer().close();
}
```

**为什么先记 EOF，再裁剪？** 假设容量为 4，收到 `insert(0, "abcdef", true)`：流长度是 6，但当前只能接收 `"abcd"`。若裁剪后再记 EOF，就会误记成 4，提前关闭。被裁掉的 `"ef"` 没有保存，需要后续重新收到。

**为什么空数据不能让整个 `insert()` 直接返回？** `insert(0, "", true)` 表示合法空流，仍然需要执行关闭判断。只有 `merge_pending()` 对空数据直接返回，主流程继续执行。

EOF 判断保留在主函数中，能直接看到“先记录、最后关闭”的顺序，没有必要把每个短判断都拆成函数。

### 3. `trim_to_window()`：只留下允许接收的数据

```cpp
void Reassembler::trim_to_window( uint64_t& first_index, string& data ) const
```

两个引用参数允许函数同步修改字符串及其起始下标。末尾的 `const` 表示不修改 Reassembler 自身状态，不妨碍修改传入的引用参数。

#### 计算窗口

```cpp
const uint64_t first_unassembled = next_index_;
const uint64_t first_unacceptable = next_index_ + output_.writer().available_capacity();
```

可接收范围是 `[first_unassembled, first_unacceptable)`。

例如总容量为 10，已写入 6 字节，reader 已读走 2 字节：

```text
ByteStream 未读字节 = 6 - 2 = 4
available_capacity = 10 - 4 = 6
first_unassembled = 6
first_unacceptable = 6 + 6 = 12
可接收窗口 = [6, 12)
```

等价地，右边界是 `reader 已读字节数 + 总容量 = 2 + 10 = 12`。

这里**不能再减一次 pending 字节数**。pending 是窗口内已经收到的位置，窗口限制的是下标范围；它不是“再从右边界扣除已收到的数据”。

#### 裁掉左侧旧数据

```cpp
if ( first_index < first_unassembled ) {
  const uint64_t bytes_to_skip = min<uint64_t>( data.size(), first_unassembled - first_index );
  data.erase( 0, bytes_to_skip );
  first_index += bytes_to_skip;
}
```

例如 `next_index_ = 5`，收到 `[3, 8) → "defgh"`。下标 3、4 已经输出，裁掉 `"de"` 后变成 `[5, 8) → "fgh"`。

`min()` 保证最多删除实际存在的字节。若整段都是旧数据，字符串会被清空。

#### 裁掉右侧越界数据

```cpp
if ( first_index >= first_unacceptable ) {
  data.clear();
} else if ( data.size() > first_unacceptable - first_index ) {
  data.resize( first_unacceptable - first_index );
}
```

- 起点就在窗口外：整段丢弃。
- 起点在窗口内、尾部越界：只保留前缀。

例如窗口为 `[5, 8)`，收到 `[6, 10) → "ghij"`，只保留 `[6, 8) → "gh"`。先判断起点是否越界，也避免后面的无符号减法下溢。

### 4. `merge_pending()`：保存数据并消除重叠

这个函数不负责输出，只维护待组装区间。空字符串提前返回，避免把整个函数再套进一层 `if`。

#### 找到右侧位置，再检查前一个区间

```cpp
auto interval = pending_.upper_bound( first_index );
```

`upper_bound(x)` 返回第一个 key **严格大于** `x` 的元素。例如 key 为 2、8、15，`upper_bound(6)` 指向 8。

新数据也可能与前一个区间重叠，所以继续检查：

```cpp
if ( interval != pending_.begin() ) {
  auto previous_interval = prev( interval );
  if ( previous_interval->first + previous_interval->second.size() >= first_index ) {
    interval = previous_interval;
  }
}
```

- 先检查不是 `begin()`，才能安全取前一个元素。
- 使用 `>=`，因为相邻也能合并：`[2, 5)` 与 `[5, 8)` 可以变成 `[2, 8)`。
- 只需检查紧邻的前一个区间，因为已有区间有序且不重叠。

#### 插入新区间，或扩展旧区间

```cpp
if ( interval == pending_.end() || interval->first > first_index ) {
  interval = pending_.emplace_hint( interval, first_index, move( data ) );
} else {
  const uint64_t interval_end = interval->first + interval->second.size();
  if ( first_index + data.size() > interval_end ) {
    interval->second.append( data.substr( interval_end - first_index ) );
  }
}
```

经过前一步，如果找到了可以合并的前一个区间，就复用它；否则插入新区间。`emplace_hint()` 接收预计插入位置，map 仍负责维护正确顺序。

这次重构把原先两个分支中重复的插入操作集中到一处。

追加时只取未覆盖的后缀。例如：

```text
已有：[2, 6) → "cdef"
新来：[4, 8) → "efgh"

下标：2 3 4 5 6 7
已有：c d e f
新来：    e f g h
```

旧区间结束于 6，新字符串从 4 开始，所以偏移量为 `6 - 4 = 2`，`data.substr(2)` 是 `"gh"`。追加后得到 `[2, 8) → "cdefgh"`。如果新数据完全被旧区间覆盖，则无需追加。

这里没有新增重叠内容冲突检测；学习场景按同一下标代表同一原始字节理解。

#### 向右吞并后续区间

```cpp
auto next_interval = next( interval );
while ( next_interval != pending_.end()
        && interval->first + interval->second.size() >= next_interval->first ) {
  const uint64_t interval_end = interval->first + interval->second.size();
  if ( next_interval->first + next_interval->second.size() > interval_end ) {
    interval->second.append( next_interval->second.substr( interval_end - next_interval->first ) );
  }
  next_interval = pending_.erase( next_interval );
}
```

例如已有 `[2, 5) → "cde"` 和 `[8, 11) → "ijk"`，新来 `[5, 9) → "fghi"`：

1. 先向左合并成 `[2, 9) → "cdefghi"`。
2. 与右侧 `[8, 11)` 重叠，只追加 `"jk"`。
3. 得到 `[2, 11) → "cdefghijk"`，删除被吸收的旧区间。

`erase()` 返回下一个迭代器，因此用其返回值继续循环，不再使用已失效的迭代器。

### 5. `push_contiguous()`：立即输出连续数据

```cpp
while ( not pending_.empty() && pending_.begin()->first == next_index_ ) {
  auto interval = pending_.extract( pending_.begin() );
  const uint64_t byte_count = interval.mapped().size();
  output_.writer().push( move( interval.mapped() ) );
  next_index_ += byte_count;
}
```

map 按起始下标排序，只需检查最早的区间。如果 `next_index_ = 3`，最早区间却从 5 开始，说明 3、4 有缺口，后面的数据也不能输出。

`extract()` 把节点移出 map，并交给 node handle 持有；`mapped()` 访问节点中的字符串。这让待组装数据可以转交给 ByteStream，而不必先复制一份再删除。

**先取长度，再 move：** `std::move()` 允许接收方移动字符串，移动后不能依赖原字符串保留原内容和长度，所以提前保存 `byte_count`。

在合法输入与当前不变量下，待输出区间已经经过窗口裁剪、起点等于 `next_index_`，整段应能写入 ByteStream，因此可以把 `next_index_` 增加整段长度。

### 6. `count_bytes_pending()`：只统计等待组装的字节

```cpp
uint64_t byte_count = 0;
for ( const auto& [first_index, data] : pending_ ) {
  byte_count += data.size();
}
return byte_count;
```

`[first_index, data]` 是结构化绑定，分别对应 map 的 key 和 value。因为区间已经去重，直接相加各字符串长度即可。

这里不包含已经进入 ByteStream、但尚未被 reader 读取的数据。也不额外维护计数成员，避免每次插入、合并和删除都要同步冗余状态。

### 7. 完整推演：乱序、重叠、EOF 一起出现

容量为 10，原始数据是 `"abcdefgh"`，整个过程中 reader 暂不读取。

| 操作 | 处理重点 | `next_index_` | pending | ByteStream 未读内容 | writer 关闭？ |
| --- | --- | ---: | --- | --- | --- |
| 初始 | EOF 未知 | 0 | 空 | 空 | 否 |
| `insert(5, "fgh", true)` | 记录 EOF = 8；前面有缺口 | 0 | `[5, 8) → "fgh"` | 空 | 否 |
| `insert(0, "abc", false)` | 输出连续的开头 | 3 | `[5, 8) → "fgh"` | `"abc"` | 否 |
| `insert(2, "cdef", false)` | 裁旧数据、合并重叠、填洞 | 8 | 空 | `"abcdefgh"` | 是 |

第三次插入具体经过：

```text
收到 [2, 6) → "cdef"
    ↓ 裁掉已输出的下标 2
保留 [3, 6) → "def"
    ↓ 与 [5, 8) → "fgh" 合并
得到 [3, 8) → "defgh"
    ↓ 起点正好等于 next_index_ = 3
写入 ByteStream，next_index_ 更新为 8
    ↓ 到达 eof_index_ = 8
关闭 writer
```

此时 writer 已关闭，但 reader 尚未读空缓冲区：

```text
writer.is_closed() = true
reader.is_finished() = false
```

等 `"abcdefgh"` 被读完，`reader.is_finished()` 才变成 `true`。

### 8. 重构的边界与验证

| 函数 | 回答的问题 |
| --- | --- |
| `insert()` | 一次插入按什么顺序处理？ |
| `trim_to_window()` | 哪些输入字节可以接收？ |
| `merge_pending()` | 如何保存数据，同时消除重叠？ |
| `push_contiguous()` | 哪些数据现在可以输出？ |
| `count_bytes_pending()` | 还有多少字节等待组装？ |

这次修改提高的是可读性，并未更换数据结构、增加公开接口或改变容量与 EOF 规则。函数不是拆得越碎越好，而是每个函数承担一个能说清楚的职责。

重构后已在 Docker 中实际运行：

```bash
./scripts/dev.sh bash -lc \
  'cmake -S . -B build-linux -G Ninja && cmake --build build-linux --target check1'
```

结果为 **18/18 全部通过**，包含编译 fixture、功能测试和性能测试。测试通过是现有用例的验证，不是所有输入的形式化证明。

本机 LSP 对 `std::optional` 和 `std::map::extract` 报错；两者都是 C++17 起提供的功能，而项目配置为 C++20。容器编译成功说明本次构建支持它们；本机诊断应另行检查语言标准和编译数据库配置，不能当作已通过的检查。
