这个是 Stanford CS144 的 **Lab Checkpoint 0: networking warmup**。它不是单纯让你写一个小程序，而是在给后面的 TCP 实现做准备：先让你从“应用看到的可靠字节流”开始，再逐步理解后面为什么需要自己实现 TCP。整个 warmup 官方预计约 **2–6 小时**。 check0

核心可以拆成 **4 个任务**。

1. **准备 Linux + C++ 环境**

课程要求 GNU/Linux，并且最终代码会在 **Ubuntu 24.04 + g++ 13.3** 上测试。即使你平时在 macOS 上开发，也要保证代码在这个环境下能编译运行。 check0

这部分本身没什么算法，主要是环境准备：

- git
- cmake
- g++
- clang / clang-tidy / clang-format
- gdb
- tcpdump / tshark

你现在是 macOS，所以我会特别提醒：**不要因为 macOS 上 clang 编译通过，就认为作业一定没问题。**

---

2. **手工体验 HTTP / SMTP / TCP byte stream**

这部分主要是建立网络直觉。

你需要用 `telnet` 手工连接 HTTP server，然后自己输入：

```
GET /hello HTTP/1.1
Host: cs144.keithw.org
Connection: close
```

最后的空行非常重要，因为它表示 HTTP header 结束。 check0

然后还要：

- 请求指定 URL，记录返回的 secret code。
- 用 SMTP 命令手工发送一封邮件。
- 用 `netcat` + `telnet`，观察 client/server 两端的 byte stream。 check0 check0

这一部分真正想让你理解的是：

> TCP socket 对应用暴露的是一个 **可靠、有序、双向的 byte stream**。

你写进去：

```
hello
```

对方最终按顺序读到的也是：

```
hello
```

应用不需要处理 packet 丢失、乱序、重复等问题。

但底层 Internet 实际提供的只是 best-effort datagram，可能发生：

- 丢包
- 乱序
- 数据损坏
- 重复

TCP 才负责把这个不可靠网络包装成可靠字节流。 check0

这个思想是整个 CS144 后面所有 lab 的主线。

---

3. **写 `webget`**

这是第一个真正的 C++ 编程任务。

文件：

```
apps/webget.cc
```

你需要实现一个简单 HTTP client，大概只有十行左右代码。

逻辑本质上就是：

```
创建 TCP socket
        ↓
连接 host:80
        ↓
发送 HTTP request
        ↓
不断读取 response
        ↓
一直读到 EOF
        ↓
打印所有内容
```

官方明确要求你使用已有的：

```
TCPSocket
Address
```

而不是自己直接调用 POSIX `socket()`。 check0

这里有几个非常容易踩的坑。

**第一，HTTP 换行必须是：**

```
"\r\n"
```

不是：

```
"\n"
```

也不要简单理解成 `endl`。 check0

**第二，一定要发送：**

```
Connection: close
```

因为这个作业通过 server 关闭连接产生的 EOF 来判断 HTTP response 已经结束。 check0

**第三，不能只 read 一次。**

网络读取可能是：

```
第 1 次 read -> 300 bytes
第 2 次 read -> 700 bytes
第 3 次 read -> 100 bytes
...
EOF
```

所以必须循环读取，直到 EOF。文档明确说 single read 不够。 check0

还有一个隐藏测试要注意：

不要 hardcode：

```
cs144.keithw.org
/hello
```

grader 会换 hostname 和 path。 check0

---

4. **实现 `ByteStream`**

这才是 Checkpoint 0 最核心的部分。

你需要修改：

```
src/byte_stream.hh
src/byte_stream.cc
```

实现一个**内存中的、有容量限制的可靠字节流**。 check0

你可以先把它想成：

```
Writer                     Reader
  │                           │
  │ push("hello")             │
  ▼                           │
┌───────────────────────────────┐
│       internal buffer         │
│        h e l l o              │
└───────────────────────────────┘
                              │
                              │ peek()
                              │ pop(2)
                              ▼
                            "he"
```

Writer 接口：

```
void push(std::string data);
void close();
bool is_closed() const;
uint64_t available_capacity() const;
uint64_t bytes_pushed() const;
```

Reader 接口：

```
std::string_view peek() const;
void pop(uint64_t len);
bool is_finished() const;
bool has_error() const;
uint64_t bytes_buffered() const;
uint64_t bytes_popped() const;
```

check0

### 最重要的理解：capacity ≠ stream 最大长度

假设：

```
capacity = 3
```

完全可以传：

```
abcdefghij...
```

甚至 TB 级数据。

只不过任何时刻：

```
未读的数据 <= 3 bytes
```

例如：

```
push("abc")
buffer = "abc"       full

pop(2)
buffer = "c"

push("de")
buffer = "cde"       full

pop(3)
buffer = ""

push("fgh")
...
```

所以：

```
capacity
```

限制的是：

> **当前 buffered、尚未被 Reader 消费的数据**

不是：

> 整条 stream 总长度。

文档专门强调了这一点。 check0

---

## 你实现 ByteStream 时最应该守住的几个 invariant

我建议你写代码之前，先牢记这几个关系。

### 1. buffer 永远不能超过 capacity

始终：

```
bytes_buffered() <= capacity
```

因此：

```
available_capacity
= capacity - bytes_buffered
```

比如：

```
capacity = 10
buffered = 4

available_capacity = 6
```

如果：

```
push("abcdefgh")
```

只能接受前 6 bytes。

---

### 2. `bytes_pushed()` 是累计值，不是当前 buffer 大小

比如：

```
capacity = 5

push("abc")
bytes_pushed = 3
buffered = 3

pop(2)
bytes_pushed = 3
buffered = 1

push("defg")
bytes_pushed = 7
buffered = 5
```

所以：

```
bytes_pushed
```

不会因为 `pop()` 减少。

类似地：

```
bytes_popped
```

也是累计值。

通常应该满足：

```
bytes_buffered
=
bytes_pushed - bytes_popped
```

这个 invariant 非常适合你 debug。

---

### 3. `close()` 和 `finished` 不是一回事

这一点特别容易写错。

例如：

```
buffer = "abc"
writer.close()
```

此时：

```
writer.is_closed() == true
```

但是：

```
reader.is_finished() == false
```

因为 `"abc"` 还没读。

只有：

```
writer 已 close
AND
buffer 已经被全部 pop
```

才：

```
reader.is_finished() == true
```

文档对 finished 的定义就是“closed and fully popped”。 check0

可以直接记：

```
finished = closed && buffer.empty();
```

概念上基本就是这样。

---

### 4. `peek()` 不消费数据

例如：

```
buffer = "hello"
```

调用：

```
peek()
```

得到：

```
"hello"
```

再调用：

```
peek()
```

还是：

```
"hello"
```

只有：

```
pop(2)
```

之后才变成：

```
"llo"
```

这其实就是典型的：

```
peek = 看
pop  = 消费
```

---

### 5. `push()` 必须接受“部分数据”

例如：

```
capacity = 5
buffered = 3
```

所以只剩：

```
available = 2
```

此时：

```
push("abcdef")
```

不能：

- 报错
- 强行塞 6 bytes
- 等 Reader 消费

而应该只接受：

```
"ab"
```

因为接口说明就是：

> Push data to stream, but only as much as available capacity allows. check0

---

## C++ 部分需要特别注意

这门课明确要求使用现代 C++ 风格。

文档甚至直接说：

- 不用 `malloc/free`
- 不用 `new/delete`
- 基本不用 raw pointer
- 不需要 smart pointer
- 不需要 thread/lock
- 不需要 virtual
- 避免 C string
- 尽量 `const`
- 参数优先 const reference
- 使用 RAII check0

所以这个作业**不是考你炫技 C++**。

不要一上来搞：

```
unique_ptr
template
mutex
custom allocator
linked list
```

一个非常普通的数据结构就足够。

你最近正在学 `string_view`，这个 lab 恰好非常适合拿来理解它。

例如：

```
std::string_view peek() const;
```

返回的是：

> 对 ByteStream 内部 buffer 中一段数据的非 owning view。

这意味着实现时要特别关注：

```
string_view 指向的数据什么时候失效？
```

这是这个 lab 里一个很值得理解的 C++ 点。

---

## 性能上不要写成 O(n²)

文档要求最终 benchmark 至少达到：

```
0.1 Gbit/s
```

而正常实现甚至可以超过：

```
10 Gbit/s
```

check0

因此如果你每次：

```
pop(1)
```

都做：

```
string.erase(0, 1);
```

那就值得警惕，因为不断从字符串头部 erase 可能反复搬移剩余数据。

更合适的思路通常是：

```
storage
+
read offset
```

而不是每读几个 byte 就把整个 buffer 左移。

这其实和你前几天问的这段代码高度相关：

```
string_view Reader::peek() const
{
  if ( chunks_.empty() ) {
    return {};
  }

  return string_view( chunks_.front() ).substr( front_offset_ );
}
```

现在你应该能看到它的设计动机了：

```
chunks_.front()
        +
front_offset_
```

就是在避免：

```
每次 pop 都移动/copy 整块内存
```

---

## 提交时最容易犯的错误

作业明确规定只应该修改：

```
apps/webget.cc
src/byte_stream.hh
src/byte_stream.cc
```

不要修改 tests，也不要改 `util` 里的 helper。 check0

最后按顺序跑：

```
git status
cmake --build build --target format
cmake --build build --target check0
cmake --build build --target tidy   # optional
```

然后确保 commit + push。 check0

---

### 如果把整个 Checkpoint 0 压缩成一张知识图

```
             Internet
                │
                │ unreliable datagrams
                ▼
          ┌───────────┐
          │    TCP    │
          └───────────┘
                │
                │ reliable byte stream
                ▼
        ┌────────────────┐
        │ stream socket  │
        └────────────────┘
                │
          ┌─────┴─────┐
          ▼           ▼
       HTTP          SMTP
      webget

                ↑
                │
     先理解这个 abstraction
                │
        ┌────────────────┐
        │   ByteStream   │
        │  in-memory     │
        └────────────────┘
                │
                ▼
         后续自己实现 TCP
```

所以 **Checkpoint 0 真正的重点不是 HTTP，也不是 telnet**。

它实际上是在让你建立一个非常重要的模型：

> **应用需要的是 reliable byte stream；TCP 的任务就是在 unreliable datagram network 上实现这个 abstraction。**

而你最后写的 `ByteStream`，就是后面几周 TCP sender / receiver / reassembler 最底层的基础组件。 check0

以你现在刚在补 C++ 的阶段，我建议把主要精力放在 **`ByteStream` 的状态/invariant、`string_view` 生命周期，以及避免无谓 copy** 上。算法本身并不难，但它非常适合练习“接口 → invariant → 数据结构 → 实现”的工程思路。