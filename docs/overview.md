# 一次 HTTP 请求怎么穿过 minnow

应用交出的是字节：`GET /path HTTP/1.1\r\nHost: …\r\nConnection: close\r\n\r\n`。往下走一层，对端再反向走回来。

1. **应用字节 → ByteStream（CP0）**  
   `Writer::push` 写入有容量上限的流；对端 `Reader::peek` / `pop` 取出。流结束靠 `close()`，出错靠 `set_error()`。

2. **ByteStream → TCP 段（CP1–3）**  
   `TCPSender` 按对端 window 切段，带 seqno、SYN/FIN、载荷。对端 `TCPReceiver` 把载荷按绝对序号交给 `Reassembler`，拼回 ByteStream，回 ackno + window。32-bit 环绕序号是 `Wrap32`。Sender 的切段、窗口和重传见 [docs/notes/tcp-sender.md](notes/tcp-sender.md)。

3. **TCP 段 → IPv4 数据报**  
   段装进 IP（源/目的地址、TTL、checksum）。封装在课程已给的 `tcp_over_ip` / `tcp_minnow_socket`，本实验不重写。

4. **IP → Ethernet / ARP（CP5）**  
   `NetworkInterface` 查下一跳 MAC：命中 ARP 表就封装 Ethernet 发出；未命中则广播 ARP、把数据报排队。

5. **Router（CP6）**  
   多网卡之间做最长前缀匹配。TTL ≤ 1 丢弃，否则减 TTL、重算 checksum，从出接口交给该口的 `NetworkInterface`。

6. **反向路径**  
   Ethernet → 解 ARP/IP → `TCPReceiver` 重组 → ByteStream → HTTP 响应字节 → 再经 `TCPSender` / IP / Ethernet 回来。

`webget` 走操作系统 `TCPSocket`，不经过自己的 TCP/IP 栈。自己的栈从 CP3 的 `tcp_ipv4` 起；CP7 `endtoend` 把两端 TCP 经 IP + ARP + Router 对打。CP4 只做 ping 日志分析，不实现协议。

## 四层各装什么

| 层 | 装什么 | 实验模块 |
| --- | --- | --- |
| 应用 | HTTP 请求/响应字节 | `apps/webget.cc`（OS TCP） |
| 传输 | seqno / ackno / window / SYN FIN RST / 载荷 | ByteStream、Reassembler、Wrap32、TCPReceiver、TCPSender |
| 网络 | IPv4 源目的、TTL、checksum | Router；datagram 封装为已给代码 |
| 链路 | Ethernet 源目的 MAC、ARP 请求/应答 | NetworkInterface |

## C++ 只记三处

- `string_view`：`Reader::peek()` 返回缓冲里一段连续视图，不拷贝。
- `optional`：未收到 SYN 时 `TCPReceiver::isn_` 为空，因而没有 ackno；路由下一跳可以是直连（空）或网关。
- 继承切开 Reader / Writer：二者继承同一 `ByteStream`，看到同一缓冲，接口互不混用。细节见 [docs/notes/byte-stream-interface-split.md](notes/byte-stream-interface-split.md)。

## 和 Go 的两处对照

- 切片 vs `string_view`：Go `[]byte` 是带长度的底层数组窗口，可再切；`string_view` 同样非拥有，但原 `string` 重分配会使 view 失效。
- `map` 合并区间：Reassembler 用有序的 `std::map<uint64_t, string>` 存未就绪片段，插入时与左右邻合并。Go 的 `map` 无序，同样的合并要另找有序结构。
