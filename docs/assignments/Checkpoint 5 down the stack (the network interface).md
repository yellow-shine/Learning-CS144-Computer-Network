这份 `check5.pdf` 是 **CS144 Checkpoint 5: down the stack (the network interface)**。和前面的 TCP sender/receiver 不同，这次你要再往网络栈下面走一层，实现一个 **NetworkInterface（网络接口）**：负责在 **IP Datagram** 和 **Ethernet Frame** 之间转换，并实现最核心的 **ARP 地址解析**。 check5

从第 2 页的图来看，它在整个 CS144 项目里的位置大概是：

```
Application
    ↓
TCP
    ↓
IP Datagram
    ↓
NetworkInterface    ← Checkpoint 5
    ↓
Ethernet Frame
    ↓
NIC
```

也就是说，这次你开始真正处理链路层问题。这个 `NetworkInterface` 后面在 Checkpoint 6 做 router 时还会继续复用。 check5

---

## 1. 这次作业最核心的任务是什么？

主要实现：

```
NetworkInterface
```

里的三个方法：

```
void send_datagram(const InternetDatagram &dgram,
                   const Address &next_hop);

void recv_frame(const EthernetFrame &frame);

void tick(size_t ms_since_last_tick);
```

并维护一张：

```
IP address → Ethernet/MAC address
```

的 ARP cache。 check5

你可以把整个作业理解成下面这个状态机：

```
我要发送 IP Datagram
       │
       ▼
知道 next-hop 的 MAC 吗？
       │
   ┌───┴────┐
   │        │
  Yes       No
   │        │
   ▼        ▼
封 Ethernet  发 ARP Request
Frame       +
直接发送     暂存 Datagram
             │
             ▼
          收到 ARP Reply
             │
             ▼
       记录 IP → MAC
             │
             ▼
      把暂存 Datagram 发掉
```

这基本就是 Checkpoint 5 的主体。

---

# 2. `send_datagram()` 要做什么？

函数：

```
void NetworkInterface::send_datagram(
    const InternetDatagram &dgram,
    const Address &next_hop);
```

别人告诉你的 NetworkInterface：

> “请把这个 IP datagram 发给 next hop。”

注意这里有一个**非常重要的区分**：

```
IP datagram destination
≠
next_hop
```

例如：

```
你的电脑
   |
Router A
   |
Router B
   |
8.8.8.8
```

IP packet 最终目标：

```
8.8.8.8
```

但当前 Ethernet frame 的目标可能只是：

```
Router A 的 MAC
```

所以这个 lab 里面 ARP 查的是：

```
next_hop IP → next_hop MAC
```

不是最终 destination IP。PDF 专门提醒不要混淆这两个概念。 check5 check5

---

## 情况 1：MAC 已经知道

假设 cache 里有：

```
10.0.0.1
   ↓
aa:bb:cc:dd:ee:ff
```

那么直接构造 Ethernet frame：

```
EthernetFrame
 ├─ dst = aa:bb:cc:dd:ee:ff
 ├─ src = my_mac
 ├─ type = IPv4
 └─ payload = serialize(IP datagram)
```

然后：

```
transmit(frame);
```

文档明确要求：

```
type = EthernetHeader::TYPE_IPv4
```

payload 是 serialized datagram。 check5

---

# 3. 如果不知道 MAC 呢？

比如：

```
next_hop IP = 10.0.0.1
```

但是 cache 里没有 MAC。

那你不能构造：

```
dst MAC = ???
```

于是要发 ARP：

```
Who has 10.0.0.1?
Tell 10.0.0.2
```

这就是：

```
ARP Request
```

而且 Ethernet destination 是：

```
ff:ff:ff:ff:ff:ff
```

即广播。

同时：

> 原来的 IP datagram 不能丢。

你需要先把它排队：

```
pending datagrams for 10.0.0.1
```

等以后收到 ARP reply：

```
10.0.0.1 is at aa:bb:cc:dd:ee:ff
```

再把 queue 里的 datagram 都封成 Ethernet frame 发出去。 check5

---

# 4. 一个非常容易漏掉的要求：5 秒 ARP 限流

假设：

```
t=0
send datagram A → 10.0.0.1
```

不知道 MAC：

```
发 ARP Request
```

然后：

```
t=1s
send datagram B → 10.0.0.1
```

仍然不知道 MAC。

你**不能再发一个 ARP request**。

只需要：

```
queue A
queue B
```

因为：

> 同一个 IP，如果最近 5 秒已经发送过 ARP request，不得重复发送。

check5

所以你大概率要记录类似：

```
unordered_map<uint32_t, size_t> arp_request_age;
```

或者：

```
struct PendingARP {
    size_t elapsed_ms;
};
```

逻辑是：

```
第一次：
ARP request
timer = 0

< 5000ms：
只 queue datagram

>= 5000ms：
下一次 send_datagram 可以重新 ARP
```

这个点非常可能是测试重点。

---

# 5. `recv_frame()` 做什么？

第二个核心函数：

```
void NetworkInterface::recv_frame(
    const EthernetFrame &frame);
```

即：

> 网卡收到一个 Ethernet frame，现在交给你处理。

第一步不是马上 parse，而是检查：

```
这个 frame 是给我的吗？
```

只有两种 destination 接受：

```
my MAC
```

或者：

```
broadcast MAC
ff:ff:ff:ff:ff:ff
```

其他 MAC 地址全部忽略。 check5

---

# 6. 收到 IPv4 Ethernet frame

如果：

```
frame.header.type == EthernetHeader::TYPE_IPv4
```

那么：

```
Ethernet payload
       ↓
parse
       ↓
InternetDatagram
```

parse 成功：

```
ParseResult::NoError
```

才放入：

```
datagrams_received queue
```

大概可以理解成：

```
InternetDatagram dgram;

if (parse(dgram, frame.payload) == ParseResult::NoError) {
    datagrams_received_.push(dgram);
}
```

check5

---

# 7. 收到 ARP frame

这是整个 lab 更重要的部分。

如果：

```
frame.header.type == EthernetHeader::TYPE_ARP
```

就 parse：

```
ARPMessage arp;
```

然后无论它是：

```
ARP Request
```

还是：

```
ARP Reply
```

都要学习：

```
sender IP → sender MAC
```

而且保存：

```
30 秒
```

。 check5

例如收到：

```
ARP Reply

sender IP:
10.0.0.1

sender MAC:
aa:bb:cc:dd:ee:ff
```

就缓存：

```
10.0.0.1
→
aa:bb:cc:dd:ee:ff
```

---

# 8. 这里一个很重要的小细节：ARP Request 也要学习

很多人自然会写：

```
收到 ARP Reply
→ cache
```

但题目明确要求：

> Learn mappings from both requests and replies.

也就是：

```
ARP Request：
Who has 10.0.0.2?
Tell 10.0.0.1 / aa:bb:...
```

虽然它是 request，但已经告诉你：

```
10.0.0.1
→
aa:bb:...
```

所以一样应该 cache。

check5

这个也是非常值得注意的测试点。

---

# 9. 收到 ARP Reply 后还必须做什么？

假设之前：

```
send Datagram A → 10.0.0.1
send Datagram B → 10.0.0.1
```

因为不知道 MAC：

```
pending[10.0.0.1] = {A, B}
```

现在收到：

```
10.0.0.1 → aa:bb:cc...
```

此时不能只更新 ARP cache。

还必须：

```
A → EthernetFrame → transmit()
B → EthernetFrame → transmit()
```

否则你的 datagram 永远卡在 queue。

虽然 PDF 的三条要求是分开写的，但这一步是从：

> queue the IP datagram so it can be sent after the ARP reply is received

直接推出来的。 check5

---

# 10. 如果别人 ARP 查询的是我呢？

例如收到：

```
ARP Request

Who has 10.0.0.2?
```

而：

```
10.0.0.2 == my IP
```

那么你必须发送：

```
ARP Reply

10.0.0.2 is at MY_MAC
```

。 check5

因此 `recv_frame()` 实际要处理三类动作：

```
IPv4
   → parse datagram

ARP
   → learn sender

ARP request for me
   → send ARP reply
```

---

# 11. `tick()` 是干什么的？

第三个方法：

```
void NetworkInterface::tick(
    const size_t ms_since_last_tick);
```

它相当于 NetworkInterface 的时钟。

主要维护：

```
ARP cache 生命周期
```

ARP cache 中的 mapping：

```
IP → MAC
```

有效期：

```
30 秒
```

所以：

```
t = 0
learn:
10.0.0.1 → MAC

...

t >= 30s
delete
```

。 check5

同时实际实现时，通常也会在这里更新：

```
last ARP request elapsed time
```

从而实现前面的：

```
5 秒内不要重复 ARP
```

---

# 12. 因此你实际需要维护的状态大概有三类

虽然题目不强制数据结构，但从需求推导，很自然是：

```
ARP cache
IP → {
    MAC,
    age
}
```

例如：

```
unordered_map<uint32_t, ARPCacheEntry>
```

第二个：

```
pending datagrams
IP → vector<InternetDatagram>
```

第三个：

```
recent ARP requests
IP → elapsed time
```

组合起来：

```
NetworkInterface
│
├── arp_cache
│     IP → MAC + age
│
├── pending_datagrams
│     IP → [dgram1, dgram2, ...]
│
└── arp_requests
      IP → time_since_request
```

题目允许你自己决定数据结构。 check5

---

# 13. 我认为这个作业最需要注意的 7 个点

### ① next hop ≠ final destination

这个一定先搞清楚：

```
IP destination = 最终目的地

Ethernet destination = 下一跳
```

Check 5 只关心：

```
next_hop → MAC
```

。 check5

### ② ARP unknown 时 datagram 不能丢

必须：

```
queue
```

等待 ARP reply。

### ③ 同一 IP 5 秒内只发一次 ARP request

但是期间来的 datagrams：

```
仍然全部 queue
```

。

### ④ ARP cache 只能存在 30 秒

收到 ARP 信息后：

```
30s TTL
```

然后 `tick()` 删除。

### ⑤ ARP Request 和 Reply 都能更新 cache

不要只学习 reply。

### ⑥ 收到 ARP Reply 后，要 flush pending datagrams

这是非常容易漏的。

### ⑦ Ethernet frame 不是给自己的就直接忽略

只接受：

```
my MAC
broadcast MAC
```

。

这些要求分别集中在三个接口的规格里。 check5

---

# 14. 有些“现实世界会做”的事情，这次反而不要做

PDF 特别说了几个不用实现的功能。

例如 ARP request 一直没人回答：

现实中可能：

```
retransmit
→ eventually ICMP host unreachable
```

但这个 lab：

```
不用做
```

。 check5

另外：

```
pending datagram 等了很久
```

现实中可能 timeout/drop。

这个 lab：

```
也不用实现
```

。

所以不要过度设计。

---

# 15. 代码量其实不大

官方预期：

```
network_interface.cc
约 100–150 行
```

。 check5

真正难的地方不是代码量，而是把这些状态和 timer 关系理顺。

---

# 16. 推荐你按这个顺序写

我建议不要三个函数混在一起写。

先做：

```
Step 1
recv_frame()
  ├─ MAC filter
  ├─ IPv4 parse
  └─ ARP parse
```

然后：

```
Step 2
ARP cache
IP → MAC + age
```

再做：

```
Step 3
send_datagram()
  └─ cache hit
```

先让：

```
known MAC
→ Ethernet frame
→ transmit
```

工作。

然后才加：

```
Step 4
cache miss
→ ARP request
→ pending queue
```

然后：

```
Step 5
ARP reply
→ flush queue
```

最后：

```
Step 6
tick()
  ├─ 30s cache expiry
  └─ 5s ARP-request timer
```

这样 debug 会简单很多。

---

# 17. 怎么测试？

核心：

```
cmake --build build --target check5
```

这个测试：

> 不依赖你之前实现的 TCP。

也就是说，即使前面的 TCP sender/receiver 有 bug，这里依然可以独立测试。 check5

提交前按顺序：

```
git status

cmake --build build --target format

cmake --build build --target check5

# optional
cmake --build build --target tidy
```

。 check5

---

# 18. 文件修改范围要注意

允许：

```
src/*.cc
src/*.hh
```

并且可以：

```
给 NetworkInterface 增加 private members
```

但是：

```
不要修改 public interface
```

。 check5

这点要特别注意，因为你可能很自然想改：

```
send_datagram(...)
```

的参数或者返回值。

不要。

---

# 19. 还需要写 `check5.md`

这次不只是代码。

还要写：

```
writeups/check5.md
```

大约：

```
20–50 行
```

并且每行：

```
≤ 80 characters
```

包括三部分：

```
Program Structure and Design

Implementation Challenges

Remaining Bugs
```

。 check5

而且还要填写：

```
花了多少小时
```

。 check5

---

## 最后把 Checkpoint 5 压缩成一句话

你这次实际上是在实现：

```
IP Datagram
      ↓
next-hop IP
      ↓
ARP cache
      ↓
next-hop MAC
      ↓
Ethernet Frame
```

以及反方向：

```
Ethernet Frame
      ↓
ARP / IPv4
      ↓
学习 ARP mapping
或者
交出 IP Datagram
```

**真正核心就是三个概念：**

```
ARP cache：30 秒

ARP request suppression：5 秒

pending datagrams：
不知道 MAC 时先缓存，知道以后再发送
```

如果你准备开始写代码，我建议下一步先把 `network_interface.hh/.cc` 的 starter code 发给我。我可以**不直接替你写答案**，而是按 CS144 这种 lab 最适合的方式，先带你把现有成员、三个方法的输入输出和需要维护的状态一项项设计出来。