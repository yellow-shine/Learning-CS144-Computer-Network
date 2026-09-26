这个 Checkpoint 6 的核心任务很明确：

> **在你 Checkpoint 5 已经实现好的 `NetworkInterface` 之上，实现一个真正的 IPv4 Router。**

Router 会有多个 `NetworkInterface`。它收到 IP datagram 后，要根据 routing table 决定两件事：

1. **从哪个 interface 发出去**
2. **发给哪个 next hop**

这次不需要实现 RIP / OSPF / BGP，也不需要自己处理 ARP/Ethernet；只需要实现“按照现有 routing table 转发数据包”的逻辑。 check6

---

## 1. 你真正需要写什么

主要修改：

```
src/router.hh
src/router.cc
```

核心就是两个方法：

```
void add_route(
    uint32_t route_prefix,
    uint8_t prefix_length,
    optional<Address> next_hop,
    size_t interface_num
);

void route();
```

以及你可以在 `Router` 里增加 private member，例如：

```
struct RouteEntry {
    uint32_t prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
    size_t interface_num;
};

std::vector<RouteEntry> routes_;
```

具体数据结构由你决定。课程明确说 **O(N) 扫描整个 routing table 完全可以接受**，所以这里没有必要一上来实现 trie / radix tree。 check6 check6

---

# 2. `add_route()` 要做什么

这个其实非常简单：

> 把一条 route 保存下来，供以后 `route()` 使用。

一条 route 可以理解成：

```
match:
    destination 是否属于 prefix/prefix_length

action:
    从 interface_num 发出去
    发给 next_hop
```

例如：

```
18.47.0.0/16
```

表示：

```
前 16 bit 是：

00010010 00101111
  18       47
```

所有：

```
18.47.x.y
```

都匹配这条 route。 check6

所以：

```
add_route(...)
```

基本不需要做 routing 决策：

```
routes_.push_back(...);
```

就够了。

---

# 3. `next_hop` 最容易搞混

这里有两种情况。

### 情况 A：直接连接的网络

比如 Router 自己有：

```
eth0 = 10.0.0.1

route:
10.0.0.0/8 direct
```

收到：

```
dst = 10.1.2.3
```

这时候：

```
next_hop == std::nullopt
```

含义不是：

> 不知道 next hop。

而是：

> **目标主机本身就是 next hop。**

所以实际发送给：

```
datagram.header.dst
```

PDF 明确规定：

> 如果 router directly attached 到这个 network，`next_hop` 是 empty optional，此时 next hop 就是 datagram 的 destination address。 check6

---

### 情况 B：经过另一个 Router

比如：

```
143.195.0.0/17 via 143.195.0.1
```

这里：

```
destination:
143.195.x.x

next hop:
143.195.0.1
```

你不是直接把 Ethernet frame 发给最终目标，而是先交给：

```
143.195.0.1
```

至于：

> 143.195.0.1 的 MAC 是什么？

**不是 Router 层负责。**

下面的 `NetworkInterface` 会负责。

这正是这个 lab 想让你理解的分层关系。

---

# 4. 真正的核心：`route()`

整个作业最重要的算法就是：

> **Longest Prefix Match**

PDF 给的规则可以直接翻译成：

```
for 每个收到的 datagram:

    找 routing table 中所有匹配 destination 的 route

    选择 prefix_length 最大的

    如果一个都没有:
        drop

    检查/减少 TTL

    如果 TTL <= 0:
        drop

    否则:
        找出 next hop
        从指定 NetworkInterface 发出去
```

check6

---

# 5. 什么叫 Longest Prefix Match

这是整个作业最核心的知识点。

假设 routing table：

```
0.0.0.0/0
10.0.0.0/8
10.1.0.0/16
10.1.2.0/24
```

现在：

```
destination = 10.1.2.99
```

它其实匹配：

```
0.0.0.0/0
10.0.0.0/8
10.1.0.0/16
10.1.2.0/24
```

但是要选：

```
10.1.2.0/24
```

因为：

```
24
```

是最大的 prefix length，也就是：

> **最具体的 route。**

所以你不能：

```
找到第一个匹配的就 break;
```

必须：

```
扫描所有 route
记录 prefix_length 最大的 match
```

---

# 6. Prefix Match 怎么写

这是 PDF 明确指出“最 tricky”的部分。 check6

假设：

```
uint32_t destination;
uint32_t route_prefix;
uint8_t prefix_length;
```

目标是比较：

```
destination 的最高 N bit
route_prefix 的最高 N bit
```

一种常见思路：

```
mask = 前 N 位为 1，后面为 0
```

例如 `/8`：

```
11111111 00000000 00000000 00000000
```

于是：

```
(destination & mask) == (route_prefix & mask)
```

---

# 7. 一个非常重要的坑：`/0`

例如：

```
0.0.0.0/0
```

这是 default route：

```
任何 destination 都匹配
```

你很容易写：

```
uint32_t mask = 0xffffffff << (32 - prefix_length);
```

看起来没问题。

但如果：

```
prefix_length == 0
```

就变成：

```
0xffffffff << 32
```

### 这是 Undefined Behavior

PDF 特别提醒：

> 在 C/C++ 中，对 32-bit integer shift 32 bits 可能产生 undefined behavior，而且测试会开 sanitizer 检测。 check6

所以你必须专门处理 `/0`。

例如逻辑上：

```
if (prefix_length == 0) {
    // 一定匹配
}
```

或者构造 mask 时显式处理：

```
uint32_t mask =
    prefix_length == 0
        ? 0
        : 0xffffffff << (32 - prefix_length);
```

这个很可能是本作业最常见 bug 之一。

---

# 8. `/32` 也要考虑

另外一端：

```
1.2.3.4/32
```

表示：

> 只匹配一个 IP。

mask：

```
11111111 11111111 11111111 11111111
```

也就是：

```
0xffffffff
```

你的实现应该正确支持完整：

```
prefix_length ∈ [0, 32]
```

PDF 明确规定范围就是 0 到 32。 check6

---

# 9. TTL 处理也非常重要

Router 转发数据包时必须减少 TTL。

例如：

```
TTL = 64
```

Router 转发后：

```
TTL = 63
```

如果：

```
TTL = 1
```

经过 Router 后：

```
TTL = 0
```

必须：

```
drop
```

如果收到时：

```
TTL = 0
```

同样直接 drop。

PDF 的规则：

> decrement TTL；如果原来已经是 0，或者 decrement 后变成 0，就丢弃。 check6

因此要避免：

```
--ttl;
if (ttl == 0) ...
```

在：

```
ttl == 0
```

而 TTL 又是 unsigned 类型时可能发生下溢。

逻辑最好先明确处理：

```
TTL <= 1
    drop
else
    TTL--
    forward
```

---

# 10. 不需要实现 ICMP

现实中的 Router 如果：

```
TTL expired
```

通常可能发：

```
ICMP Time Exceeded
```

如果：

```
no route
```

可能发：

```
Destination Unreachable
```

但这个 lab **完全不用做**。

直接：

```
drop
```

即可。 check6

---

# 11. Router 不应该碰 ARP / Ethernet / TCP

这个设计思想其实比代码本身更重要。

你的 Router 层应该只看到：

```
InternetDatagram
        ↓
routing decision
        ↓
NetworkInterface::send_datagram(...)
```

不要在 Router 里面考虑：

```
MAC address
ARP request
Ethernet frame
TCP
```

PDF 特别强调：

> Router only thinks about Internet datagrams，通过 `NetworkInterface` abstraction 和 link layer 交互。 check6

这其实就是非常经典的网络分层：

```
          Router

     InternetDatagram
            │
            ▼
     routing decision
            │
            ▼
   NetworkInterface
      │
      ├── ARP
      ├── Ethernet
      └── frame queue
```

Checkpoint 5 已经把下面一层做好了。

Checkpoint 6 只是在它上面加：

```
IP forwarding
```

---

# 12. `route()` 还需要处理多个 interface 上的 datagram

Figure 1 表达的结构是：

```
               Router
         Longest Prefix Match

          ↑      ↑      ↑
          │      │      │
         NI0    NI1    NI2
```

每个 NetworkInterface 都可能收到：

```
InternetDatagram
```

Router 需要把这些收到的数据包拿出来并逐个 routing。

也就是说不要理解成：

```
Router::route(datagram)
```

而更像：

```
Router::route()
```

里面：

```
遍历所有 interfaces
    遍历这个 interface 收到的 datagrams
        route each datagram
```

PDF 图 1 就是在强调这个结构。 check6

---

# 13. `Address` 和 `uint32_t` 的转换

你会频繁碰到：

```
Address
```

和：

```
uint32_t
```

之间转换。

PDF 给了两个 API：

```
Address::ipv4_numeric()
```

把：

```
Address → uint32_t
```

以及：

```
Address::from_ipv4_numeric(...)
```

把：

```
uint32_t → Address
```

check6

这一点对：

```
destination prefix comparison
```

以及：

```
direct route 的 next hop
```

都会用到。

---

# 14. 我建议你脑子里保持这个算法

可以把 `route()` 想成这段伪代码：

```
for interface in interfaces:

    while interface 有 datagram:

        dgram = 取出 datagram

        if TTL <= 1:
            drop
            continue

        best_route = none

        for route in routing_table:

            if destination matches route.prefix:
                if best_route == none
                   or route.prefix_length > best.prefix_length:

                    best_route = route

        if best_route == none:
            drop
            continue

        dgram.TTL--

        if route.next_hop exists:
            next_hop = route.next_hop
        else:
            next_hop = destination

        interface(route.interface_num)
            .send_datagram(dgram, next_hop)
```

具体 API 名字还是要以 `router.hh` / `network_interface.hh` 为准，但逻辑基本就是这样。

---

# 15. 一个例子走完整流程

假设 Router：

```
eth0: 10.0.0.1
eth1: 172.16.0.1
eth2: 192.168.0.1
```

routing table：

```
10.0.0.0/8       direct → eth0
172.16.0.0/16    direct → eth1
192.168.0.0/24   direct → eth2
0.0.0.0/0        via 171.67.76.1 → default interface
```

收到：

```
dst = 192.168.0.42
TTL = 64
```

匹配：

```
192.168.0.0/24
0.0.0.0/0
```

Longest Prefix：

```
/24
```

所以：

```
TTL: 64 → 63

next hop:
192.168.0.42

interface:
eth2
```

---

如果收到：

```
dst = 8.8.8.8
```

前三条都不匹配。

但：

```
0.0.0.0/0
```

永远匹配，所以：

```
next hop = 171.67.76.1
```

走 default route。

PDF 第 5 页的模拟网络图就是这种 routing table：有多个 direct route、若干 via-next-hop route，以及 default route。 check6

---

# 16. 这次作业最应该注意的 8 件事

我会重点盯这几个：

1. **Longest Prefix Match，不是 first match。**
2. **`/0` default route 要正确处理。**
3. **不能 shift 32 bits，否则 UB。**
4. **`prefix_length` 必须支持完整 `[0,32]`。**
5. **`next_hop == nullopt` 表示直接发送给 destination。**
6. **TTL <= 1 的 datagram 要 drop。**
7. **没有 route 的 datagram 直接 drop，不需要 ICMP。**
8. **Router 层不要自己处理 ARP / Ethernet。**

其中最容易导致 hidden test 失败的大概率就是：

```
/0
TTL
longest-prefix match
direct route
```

---

# 17. 提交时还有几个限制

只应该修改：

```
src/*.hh
src/*.cc
```

可以添加 private members，但：

> **不要修改 public interface。** check6

提交前要求：

```
cmake --build build --target format
cmake --build build --target check6
```

可选：

```
cmake --build build --target tidy
```

check6

另外还要写：

```
writeups/check6.md
```

大约：

```
20–50 行
每行不超过 80 characters
```

包括三个部分：

```
Program Structure and Design

Implementation Challenges

Remaining Bugs
```

以及记录完成作业花了多少小时。 check6

顺便提醒一下，PDF 第 4～5 页把 router test 命令写成了：

```
cmake --build build --target check5
```

但最终提交章节明确写的是：

```
cmake --build build --target check6
```

所以这里文档本身看起来存在一处不一致；最终应至少以提交章节的 `check6` 为准。 check6 check6

---

## 这次 Lab 在整个 CS144 里的位置

如果把你最近做的几个 checkpoint 连起来，其实非常漂亮：

```
Checkpoint 4
TCP connection / reassembly
        │
        ▼
Checkpoint 5
NetworkInterface
ARP + Ethernet + IP ↔ Ethernet
        │
        ▼
Checkpoint 6
Router
IP forwarding
Longest Prefix Match
```

这里开始从“主机协议栈”切到真正的：

> **网络核心 forwarding plane。**

而且 Checkpoint 6 代码量其实并不大，PDF 预计你的实现新增大约 **30–60 行**。难点不是代码量，而是把 **CIDR / prefix match / next hop / TTL / 网络分层** 这几件事真正想清楚。 check6