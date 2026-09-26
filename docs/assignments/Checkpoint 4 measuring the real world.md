这个作业是 **CS144 Checkpoint 4: measuring the real world**。和前面的 Checkpoint 不一样，这次基本**不写网络协议实现**，而是做一个小型网络测量实验：选几条真实 Internet 路径，持续收集 ping 数据，然后分析丢包、RTT、相关性和吞吐量，最后交一份 PDF 报告。

作业明确要求你至少选择 **3 条“有意思”的网络路径**。每条路径都是“你的电脑/VM → 某个远端主机”。所谓有意思，至少满足下面之一：

- RTT > 100 ms，也就是比较远的跨地域路径；
- 路径中存在比较特殊的链路，比如 Wi-Fi、手机热点、蜂窝网络、卫星网络。

老师建议最好混合选择，比如一条“普通但跨洲”的长距离路径，再加一条经过 Wi-Fi/手机热点之类的路径。 check4

比如你可以考虑：

```
Path A: 家里宽带 → 英国 Oxford
Path B: 家里宽带 → 新西兰 Canterbury
Path C: Mac → 手机热点 → 某个远程服务器
```

---

## 1. 每条路径先收集数据

首先选一个远程 host，然后用：

```
mtr hostname
```

或者：

```
traceroute hostname
```

观察从你这里到目标机器中间经过哪些路由器。作业也给了一些候选地址，比如 Oxford、北京大学、新西兰 Canterbury、Rwanda 等。 check4

然后最重要的一步是：

> **持续 ping 至少 1 小时。**

参考命令：

```
ping -D -n -i 0.2 hostname | tee data.txt
```

这里：

```
-D
```

给每一行添加 timestamp。

```
-n
```

不做 reverse DNS lookup。

```
-i 0.2
```

每隔 0.2 秒发一个 ping，也就是：

```
5 ping / second
```

运行一小时大约会得到：

```
5 × 3600 ≈ 18,000 个 ping
```

PDF 这里写成了 approximately 3,600 echo requests，但按照它给的 `5 × 3600` 算应该是 18,000；你实际分析时应根据真实 sequence number/发送数量计算，不要依赖这个文字里的数字。作业还特别提醒：长时间实验不要比 0.2 秒间隔更快地 flood 对方。 check4

---

## 2. 真正的大头：分析这些 ping 数据

对于**每条路径**，你基本要回答下面这些问题。

### ① 整体 packet delivery rate

计算：

```
收到的 ping reply 数
--------------------
发出的 ping request 数
```

例如：

```
sent     = 18000
received = 17950

delivery rate ≈ 99.72%
```

这里有一个非常重要的坑：

Linux `ping` 对**丢掉的包不会输出一行记录**。

例如你可能看到：

```
icmp_seq=101
icmp_seq=102
icmp_seq=105
```

那就说明：

```
103 丢了
104 丢了
```

所以你的分析脚本需要根据：

```
icmp_seq
```

找洞，而不能简单地数输出行。 check4

---

### ② 最长连续成功次数

比如：

```
✓ ✓ ✓ ✓ ✓ ✗ ✓ ✓
```

最长成功 streak 是：

```
5
```

---

### ③ 最长连续丢包次数

例如：

```
✓ ✓ ✗ ✗ ✗ ✗ ✓
```

最长 loss burst：

```
4
```

这两个指标其实是在让你观察：

> 丢包究竟是随机零星发生，还是一次出现就容易连续丢很多个？

check4

---

# 3. 最难理解的一项：packet loss autocorrelation

这是这个作业里我认为最值得你提前搞懂的部分。

老师要求对：

```
k = -10 ... 10
```

计算两类 conditional probability。

第一类：

```
P(packet N+k 成功 | packet N 成功)
```

也就是：

> 如果当前这个 packet 成功了，那么前后第 k 个 packet 成功的概率是多少？

第二类：

```
P(packet N+k 丢失 | packet N 丢失)
```

也就是：

> 如果当前 packet 丢了，那么附近的 packet 也丢的概率是多少？

然后和总体成功率/丢包率进行比较。 check4

比如总体丢包率只有：

```
1%
```

但是你计算发现：

```
P(loss at N+1 | loss at N) = 40%
```

这说明什么？

说明丢包**强烈具有 burstiness**。

也就是说不是：

```
随机地每 100 个包丢一个
```

而可能更像：

```
正常正常正常正常正常
丢 丢 丢 丢
正常正常正常……
```

这就是 autocorrelation 这一部分真正想让你观察的东西。

---

# 4. RTT 分析

接下来是延迟。

你首先要找：

```
minimum RTT
maximum RTT
```

其中老师特别指出：

> minimum RTT 可以近似理解成这条网络路径的真实 MinRTT。

check4

然后需要画几张图。

### RTT over time

横轴：

```
真实时间
```

比如：

```
14:00
14:10
14:20
...
15:00
```

纵轴：

```
RTT(ms)
```

你大概率会看到这种现象：

```
30ms
32ms
31ms
90ms  ← spike
35ms
33ms
120ms ← spike
```

这张图可以观察 jitter、拥塞、瞬时异常等。 check4

---

# 5. RTT CDF

还需要画：

```
RTT 的 CDF
```

横轴：

```
RTT(ms)
```

纵轴：

```
P(RTT <= x)
```

例如：

```
RTT <= 30ms     20%
RTT <= 40ms     80%
RTT <= 50ms     95%
RTT <= 100ms    99%
```

CDF 特别适合回答：

> “大多数请求的 RTT 到底是多少？”

而不是只看 average。

老师还要求你描述：

> 这个 distribution 大致是什么形状？

check4

---

# 6. 相邻 RTT correlation

还需要画一个 scatter plot：

```
x = RTT(N)
y = RTT(N+1)
```

例如：

```
RTT_N    RTT_N+1

30       32
31       30
100      95
90       92
```

如果散点明显沿着：

```
y = x
```

聚集，就说明：

> 一个包 RTT 高，后面的包也很可能 RTT 高。

也就是说 RTT 在时间上有相关性。

如果完全散开，则说明相邻 RTT 相对独立。 check4

---

# 7. 做一个很短的高流量实验

这是另一个容易忽略的部分。

老师让你短时间增加：

```
packet size
ping frequency
```

但实验要：

> **少于 10 秒。**

例如 Linux 下：

```
ping -s 1400 -i 0.01 -c 500 hostname
```

其中 packet size：

```
<= 1400 bytes
```

不要超过 1400。 check4

然后计算：

```
reply data rate
=
packet size × replies
---------------------
duration
```

接着画：

```
x: request data rate
y: reply data rate
```

你要看看：

> 随着发送速率不断增加，收到的数据速率是否最终 plateau / level off？

最后报告：

```
maximum throughput
```

check4

注意，这里不是让你长时间疯狂 ping。高频实验明确要求只做很短时间。

---

# 8. 最后不是只交图，还要写分析

老师最后还要求你回答：

> 网络表现符合预期吗？

> 哪些结果让你意外？

以及：

> 3 条路径之间有什么有意思的区别？

check4

所以不能只交：

```
图1
图2
图3
```

你应该写类似：

```
Path A 的平均 RTT 很稳定，但偶尔有明显 spike。

Path B 的平均 RTT 更高，但 jitter 反而更低。

Path C 使用 cellular tethering，整体 delivery rate 依然很高，
但 loss autocorrelation 显著增强：
一旦出现 packet loss，接下来几个 packet 的 loss probability
明显高于 unconditional loss probability。
```

这其实才是这个实验最主要的学习目的。

---

# 你最终需要交什么

最终交的是：

> **一份 PDF report，通过 Gradescope 提交。** check4

我建议你的报告结构直接做成：

```
Checkpoint 4: Measuring the Real World

1. Experiment Setup
   - Path A
   - Path B
   - Path C

2. Path A
   - traceroute
   - delivery rate
   - longest success streak
   - longest loss burst
   - loss autocorrelation
   - min/max RTT
   - RTT over time
   - RTT CDF
   - RTT(N) vs RTT(N+1)
   - throughput experiment

3. Path B
   ...

4. Path C
   ...

5. Comparison
   - latency
   - loss
   - burstiness
   - RTT stability
   - throughput

6. Conclusions
```

### 我认为你最需要注意的 5 个坑

1. **不是只测一条路径，而是至少 3 条。**
2. **每条路径正常 ping 至少 1 小时。**
3. Linux ping 丢包时没有输出，所以必须根据 `icmp_seq` 补出 missing packets。
4. `autocorrelation` 不是简单算 loss rate，而是算 `P(N+k success | N success)` 和 `P(N+k loss | N loss)`。
5. 高频、大包的测试只做 `<10 秒`，packet size 不超过 `1400`。

从代码量来说，这次作业其实不算大；主要工作会集中在**数据解析 + matplotlib/pandas 画图 + 理解这些统计量意味着什么**。如果你愿意，我下一步可以直接按这个 Checkpoint 给你设计一套 **Python 分析脚本的数据结构和实现思路**，但不直接替你把作业答案写完。