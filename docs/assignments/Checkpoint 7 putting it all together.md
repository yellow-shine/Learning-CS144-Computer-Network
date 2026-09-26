这个 **Checkpoint 7: putting it all together**，和前几个 checkpoint 很不一样：**主要不是让你再实现新的网络协议代码，而是把你前面写的 ByteStream、TCP、NetworkInterface、Router 全部串起来，做一次真正的端到端联调。** 如果前面的实现都正确，这一关理论上可能几乎不用写代码。 check7

## 这次作业到底要做什么

可以把目标理解成：

> 用你自己的网络栈作为一台 Host + 一台 Router，和另一个同学的 Host + Router 连起来，然后真正建立 TCP 连接并传 1 MB 文件。

前面的 checkpoint 大致是：

```
check0
ByteStream

check1-3
TCP
  TCPSender
  TCPReceiver
  Reassembler
  TCP connection

check5
NetworkInterface
Ethernet / ARP / IP

check6
Router
IP forwarding

check7
把上面所有东西接起来
```

文档第 2 页的图就是整个实验拓扑。每个同学贡献：

```
Host
 ├── ByteStream
 ├── TCP
 └── NetworkInterface
        |
      Router
 ├── NetworkInterface (host_side)
 └── NetworkInterface (internet_side)
        |
     relay server
        |
另一个同学的 Router + Host
```

客户端这边大概是：

```
Client Host
192.168.0.50
      |
192.168.0.1
Client Router
      |
10.0.0.192
      |
cs144.keithw.org
      |
10.0.0.172
Server Router
      |
172.16.0.1
      |
Server Host
172.16.0.100
```

也就是说，一个 TCP packet 真正走的路径是：

```
你的 TCP sender
    ↓
你的 IP
    ↓
你的 NetworkInterface
    ↓
你的 Router
    ↓
relay server
    ↓
同学的 Router
    ↓
同学的 NetworkInterface
    ↓
同学的 TCP receiver
```

所以 Checkpoint 7 本质上是一个 **integration test / interoperability test**。 check7

---

## 第一件事：先合并 check7 starter code

文档要求先确保之前的代码都 commit 了，然后：

```
git fetch --all
git merge origin/check7-startercode

cmake -S . -B build
cmake --build build
```

如果要开 sanitizer：

```
cmake -S . -B build -DSANITIZED_APPS=True
```

然后开始填写：

```
writeups/check7.md
```

这里有一个很重要的注意点：

> 不要随便修改 `src` 顶层之外的文件，否则 merge starter code 可能发生问题。 check7

---

# 第一阶段：自己和自己测试

这一点非常重要。

老师明确要求：

> **不要一上来就找同学联调。**

先开两个 Terminal，在自己的机器上同时运行 client 和 server。

这样网络实际上变成：

```
你的 TCP
 ↓
你的 Router
 ↓
你的 Router
 ↓
你的 TCP
```

也就是：

```
your implementation
        ↕
your implementation
```

如果这里都失败，就没必要找同学，因为问题基本一定在自己的实现。

文档也明确说，这种方式比直接和另一个人的实现联调容易 debug。 check7

---

# 第二阶段：运行 endtoend

新的程序在：

```
build/apps/endtoend
```

你要随机选一个：

```
1024 ~ 64000
```

之间的 **偶数**。

例如：

```
42000
```

不要直接使用文档示例里的 `3000`，因为 group ID 需要避免和别人冲突。 check7

然后 server：

```
./build/apps/endtoend server cs144.keithw.org 42000
```

client：

```
./build/apps/endtoend client cs144.keithw.org 42001
```

注意规律：

```
server = 偶数
client = 偶数 + 1
```

比如：

```
42000
42001
```

---

# 第三个目标：首先让 TCP handshake 成功

正常情况下 Client 应该看到类似：

```
Connecting from 192.168.0.50:xxxxx...
Connecting to 172.16.0.100:1234...
Successfully connected to 172.16.0.100:1234.
```

Server：

```
New connection from 192.168.0.50:xxxxx.
```

只要看到这里，就意味着非常多东西已经一起工作了：

```
ARP
Ethernet
IPv4
Routing
TCP SYN
TCP SYN+ACK
TCP ACK
```

也就是至少：

```
check0 + check1 + check2 + check3
+ check5 + check6
```

已经真正组合起来了。

文档称这一点为：

> 两台计算机已经成功交换了 TCP handshake。 check7

---

# 第四个目标：双向发送数据

建立 TCP 后，在一边直接打字：

```
hello
```

另一边应该看到。

然后反过来也测试。

所以不是只有：

```
client → server
```

还要验证：

```
client ↔ server
```

这是一个 **full-duplex TCP connection**。

---

# 第五个目标：正确关闭连接

这里是我觉得这次作业非常值得注意的一点。

你需要在一边按：

```
Ctrl-D
```

它不是简单地：

```
kill connection
```

而是在语义上：

```
结束这个方向的 outbound ByteStream
```

例如：

```
Client ----FIN----> Server
```

Client 自己仍然应该可以继续接收：

```
Client <---data---- Server
```

直到 Server 也结束自己的 ByteStream。

所以这里其实是在测试：

```
TCP half-close
```

而不是：

```
一边关闭 → 整个 TCP 马上消失
```

文档明确要求验证：一边结束 outbound stream 后，仍然继续接收对方数据；等双方都结束 ByteStream，linger 完成后程序才应该正常退出。 check7

这个地方如果有问题，通常要回头看你前面的：

```
FIN handling
TCPReceiver
TCPSender
connection shutdown
linger logic
```

---

# 第六个目标：传输 1 MB 文件

这是核心验收。

先创建 1 MB 随机文件：

```
dd if=/dev/urandom bs=1M count=1 of=/tmp/big.txt
```

Server：

```
./build/apps/endtoend server cs144.keithw.org 42000 < /tmp/big.txt
```

Client：

```
</dev/null ./build/apps/endtoend client cs144.keithw.org 42001 \
    > /tmp/big-received.txt
```

然后：

```
sha256sum /tmp/big.txt
sha256sum /tmp/big-received.txt
```

两个 hash 必须一致。

这比发送 `"hello"` 强很多，因为 1 MB 数据会真正压测：

```
segmentation
retransmission
flow control
sequence numbers
reassembly
routing
ARP
TCP FIN
```

所以：

> **能连上 TCP 不代表你的 TCP 一定正确；1 MB 文件完全一致才是更强的验证。** check7

---

# 第七阶段：再和另一个同学互联

自己跑通以后，才找 lab partner。

例如：

```
你：
Client implementation

同学：
Server implementation
```

然后最好再交换：

```
你：
Server

同学：
Client
```

因为这可以验证 interoperability：

```
your TCP sender → their TCP receiver
their TCP sender → your TCP receiver
```

有时你的代码“自己和自己”完全能工作：

```
bug A + bug A
```

两个相同 bug 恰好兼容。

但是换成另一份实现：

```
bug A + correct implementation
```

就暴露问题了。

这其实也是 Checkpoint 7 最有价值的地方。

---

# 如果失败，怎么 debug

第一招：

```
./build/apps/endtoend ... debug
```

在命令最后追加：

```
debug
```

它会打印所有 Ethernet frames，包括：

```
ARP
IPv4
TCP
```

所以你可以沿网络栈逐层看：

```
有没有发 ARP request？
        ↓
有没有收到 ARP reply？
        ↓
IP datagram 有没有产生？
        ↓
Router 有没有 forward？
        ↓
TCP SYN 有没有出去？
        ↓
SYN+ACK 有没有回来？
```

文档专门建议这么做。 check7

第二招则是：

```
cmake --build build --target test
```

重新跑完整 unit tests。

还可以开 sanitizer，它能发现：

```
undefined behavior
invalid memory access
```

等问题。 check7

---

# 最后到底要提交什么？

主要是：

```
writeups/check7.md
```

要求大概：

```
30–70 行
每行 <= 80 characters
```

内容分三部分。

### Solo portion

回答：

```
1. 自己和自己能否建立、结束连接？
2. 能否成功传输 1 MB 文件？
3. SHA-256 是否一致？
4. 为此修改了什么代码？
```

### Group portion

回答：

```
1. lab partner 是谁？
2. SUNet ID 是什么？
3. 两套实现能否互相连接？
4. client/server 两种角色是否都测试？
5. 能否传 1 MB 文件？
6. 是否为了 interoperability 修改代码？
```

### Creative portion

可选 extra credit。

例如老师说可以：

```
更复杂的多人网络
修改 endtoend.cc
其他有趣实验
```

完全不是必须的。 check7

---

# 代码修改有一个很重要的限制

如果发现前面 checkpoint 的 bug，需要改代码：

**只能修改：**

```
src 顶层的 .hh
src 顶层的 .cc
```

你可以：

```
增加 private members
```

但是：

**不能修改 public interface。**

而且：

```
不要增加额外文件
```

因为 autograder 不会看它们，甚至可能因此编译失败。 check7

---

# 我建议你实际按这个顺序做

```
① merge check7 starter code
          ↓
② cmake + build
          ↓
③ 跑完整 unit tests
          ↓
④ 自己开两个 terminal
          ↓
⑤ endtoend 建立 TCP
          ↓
⑥ 双向输入文字
          ↓
⑦ Ctrl-D 测 half-close
          ↓
⑧ 传输 1 MB 文件
          ↓
⑨ SHA256 一致
          ↓
⑩ 找 partner
          ↓
⑪ 你的 client → 对方 server
          ↓
⑫ 对方 client → 你的 server
          ↓
⑬ 再传 1 MB
          ↓
⑭ 写 check7.md
```

## 最需要注意的 5 个点

如果你只是想抓住重点，我会特别留意：

1. **这不是新的实现作业，而是最终 integration test。**
2. **先自己和自己跑通，再找 partner。**
3. **server 用随机偶数，client 用 `even + 1`。**
4. **不仅要 handshake，还要测试 Ctrl-D / half-close 和 1 MB 文件。**
5. **如果需要修 bug，只动 `src` 顶层 `.cc/.hh`，不要改 public interface，也不要新增文件。**

而从学习角度看，Checkpoint 7 最重要的其实是让你第一次完整看到：

```
Application
   ↓
ByteStream
   ↓
TCP
   ↓
IP
   ↓
NetworkInterface
   ↓
Ethernet / ARP
   ↓
Router
   ↓
真实网络
   ↓
Router
   ↓
Ethernet / ARP
   ↓
IP
   ↓
TCP
   ↓
ByteStream
   ↓
Application
```

也就是 **CS144 前面所有 checkpoint 最终拼成了一个真正能工作的 TCP/IP 网络栈。**