# TCPSender：切段、占窗口、超时只重传最早一段

`src/tcp_sender.cc` 只做发送方向。应用把字节推进 `ByteStream`，调用方再调 `push` / `receive` / `tick`。Sender 自己不读时钟，也不在 `receive` 里顺便 `push`。

对外三件事：

- `push`：从流出字节，在窗口里尽量切成段，交给 `transmit`。
- `receive`：记下对端的窗口，丢掉已经被**完整**确认的段，必要时重置 RTO。
- `tick`：倒计时。到期只重传 `outstanding_` 最前面那一段。

`sequence_numbers_in_flight()` 和 `consecutive_retransmissions()` 是给测试和上层看的计数。超过 `TCPConfig::MAX_RETX_ATTEMPTS`（8）要不要拆连接，不在这个类里做。这里会一直重传，只把次数加下去。

## 序号空间

绝对序号从 0 起，和 ISN 无关。线上的 32-bit 序号是 `Wrap32::wrap(abs, isn_)`，也就是 ISN 加上绝对序号再取低 32 位。

SYN 占绝对序号 0，第一个载荷字节是 1。FIN 占载荷之后的下一个号。空段（没有 SYN、没有载荷、没有 FIN）`sequence_length()` 为 0，不占号。

```text
绝对序号:  0=SYN   1 2 3 ... n=载荷   n+1=FIN   next_abs_
线上 seqno: ISN    ISN+1 ...          ISN+n+1
```

三个边界：

| 变量 | 含义 |
| --- | --- |
| `ack_abs_` | 对端下一个想要的绝对序号。它左边都已确认 |
| `next_abs_` | 下一个还没发出去的绝对序号。重传不加它 |
| `window_size_` | 对端通告的窗口，原样保存。0 就是 0 |

还没收到任何通告时，`window_size_` 初始是 1，不是 0。所以第一次 `push` 只能发出 SYN。

`ackno` 和段的 `seqno` 拿回来时用 `unwrap(isn_, next_abs_)`。checkpoint 用 `next_abs_`，因为未确认的段和对端的 ack 都应该离“已经发到哪”不远，不会跨过半圈（2^31）。窗口最大 65535，正常 ack 落在这一圈里。

## 两块缓冲，不要混

| | ByteStream | `outstanding_` |
| --- | --- | --- |
| 装什么 | 应用写了、Sender 还没读走的字节 | 已经发出、还没被完整 ACK 的**整段** |
| 谁写入 | `writer().push` | `send_segment` |
| 谁清空 | `read()` 在 `push` 里 pop | `receive` 里整段弹出 |
| 重传从哪读 | 不从这里读。字节已经被 pop 走了 | 直接把存着的 `TCPSenderMessage` 再 `transmit` |

`read()` 会 pop。段一旦发出，载荷的唯一副本在 deque 里。所以 deque 存整段，不存“序号区间再回流里找字节”。

`deque` 够用：发送顺序就是序号顺序，ACK 只从前面丢，超时只重传 `front()`。不用 map。

部分确认不裁段。发了 `seq=1, "abcdef"`（长度 6），ack 到 4，`"abc"` 其实已经到了，这段仍整段留着，`sequence_numbers_in_flight()` 仍算 6。只有 `seg_start + sequence_length() <= ack_abs_` 才 `pop_front`。后面的段序号更大，前面这段没被完整确认，后面也不可能，所以扫到第一段不够就 `break`。

因此：

```text
sequence_numbers_in_flight()  >=  next_abs_ - ack_abs_
```

没有“卡在段中间的 ack”时两者相等。有部分 ack 时，在途计数把已经确认的前缀也算进去，比真实未确认字节多。

窗口也按这个偏大的在途数来卡，不按 RFC 的右边界 `ack + window`。结果是：一段只被确认了一半、对端又把窗口开大时，这份实现要等整段被确认才继续填。测试要的就是“段是原子的”，所以这里故意偏保守，不另维护 `next - ack`。

## 状态

```text
next_abs_          下一个要发的绝对序号
ack_abs_           已确认到的绝对序号
window_size_       上次通告的窗口；初始 1
outstanding_       未完整确认的整段，按发送顺序
syn_sent_ fin_sent_  这两个标志各只能占一次号
rto_ms_            当前超时。新 ack 时回到 initial_RTO_ms_
timer_running_
timer_remaining_ms_  倒计时，不是墙上时钟
consecutive_retransmissions_  自上次新 ack 以来、且窗口非 0 的超时次数
```

不变量：

1. `next_abs_` 只在发出**新**段时增加。重传不加。
2. `outstanding_` 按序号递增，中间没有洞。我们从不跳号发。
3. 定时器在第一次发出占号的段时启动；队列被新 ack 掏空时关掉。后面的新段不重置它——它跟的是最早那段。
4. `syn_sent_` / `fin_sent_` 一旦为真就不再放第二个 SYN/FIN。置位发生在 `transmit` 之前；正常路径上段一定进了队列。
5. `window_size_ == 0` 只在 `push` 的局部变量里当成 1。存下来的值不变，`tick` 才能看出“这是零窗口，不要退避”。

## push：把窗口填满

```text
出错 → 发一个空 RST，返回。不读流，不进 outstanding_。

effective = window_size_ == 0 ? 1 : window_size_

循环:
  在途 >= effective → 停
  remaining = effective - 在途

  还没发过 SYN → 放上，used = 1
  载荷上限 = min(remaining - used, MAX_PAYLOAD_SIZE)   // 1000，只限载荷
  read() 读那么多
  流已结束、还没发 FIN、used < remaining → 放 FIN

  used == 0 → 没东西可发，停
  发出，入队，next_abs_ += sequence_length()
```

一次 `push` 可以发出多段，直到窗口满或流空。每段载荷不超过 `TCPConfig::MAX_PAYLOAD_SIZE`（1000）。SYN/FIN 不算在这 1000 里。窗口够的话，一段可以是 `SYN + 1000 字节 + FIN`，占 1002 个序号。

`remaining - used` 不会下溢：能进循环说明 `remaining >= 1`，SYN 最多再占 1。

FIN 的条件是 `reader().is_finished()`：Writer 已经 `close()`，并且 Reader 里一个字节都不剩。所以 FIN 只会挂在**读走最后几个字节的那一段**上，而且窗口还得剩至少 1 个序号。窗口刚好被载荷填满，FIN 留到下次。

例子：窗口 2500，流里 2500 字节且已 close。

- 两段 1000，在途 2000，剩下 500。
- 第三段读 500，`used == remaining`，FIN 放不进去。
- 在途 2500，循环结束。FIN 要等这 2500 被确认、窗口重新打开。

窗口若是 2501，第三段就是 500 字节加 FIN。

零窗口：`effective` 局部为 1，存着的 `window_size_` 仍是 0。已经有 1 个序号在途，就不再发。那个在途的 1（一个字节，或单独的 FIN，或还没被确认的 SYN）就是探测段。

出错路径不走 `send_segment`。空 RST 的 `seqno` 是 `next_abs_`（下一个还没用的号），`sequence_length()` 为 0，不入队，不启动定时器。循环里那句 `msg.RST = input_.has_error()` 在这条路径上走不到；正常路径里它恒为 false。

`make_empty_message()` 是同一件事的只读版：空载荷、无 SYN/FIN、`seqno = wrap(next_abs_)`，流已经出错则带 RST。不占号，不重传。测试里的 `ExpectSeqno` 看的就是它。

## receive：先改窗口，再决定 ack 算不算

```text
对端 RST → writer().set_error()，返回。窗口和定时器都不动。

window_size_ = msg.window_size     // 没有 ackno、ack 非法，也要更新

没有 ackno → 返回
abs_ack > next_abs_ → 返回          // 确认了还没发过的号：不删段，不重置 RTO
abs_ack <= ack_abs_ → 返回          // 旧 ack 或重复 ack：同样不重置

否则这是新 ack:
  从队头丢掉已经被完整盖住的段
  consecutive_retransmissions_ = 0
  rto_ms_ = initial_RTO_ms_
  队列空 → 停定时器
  队列不空 → 用初始 RTO 重开定时器
```

窗口更新放在 ack 判断前面。所以：

- 还没发 SYN，对端可以只给窗口、不给 ackno。下一次 `push` 就能在这个窗口里把 SYN、载荷、FIN 塞进同一段。测试 “Receiving before transmit” 就是这条。
- 非法 ack（比如只发到 `isn+5`，对端 ack 到 `isn+1000`）仍会改窗口，但定时器按原 RTO 继续走。`send_extra` 里 “invalid ackno” 靠这个区分：超时已经把 RTO 加倍之后，非法 ack 不能把它打回初始值。

`abs_ack == next_abs_` 是合法的：把发过的全部确认掉。`abs_ack == ack_abs_` 是重复 ack，不算新数据，不重置退避。更老的 ack 同样忽略，但窗口已经更新过了。

新 ack 即使没把队列掏空，也把 RTO 和重传次数清掉，定时器从头按**初始** RTO 再数。退避不保留。后面若再超时，次数从 1 重新计，等待时间也从初始 RTO 起，不是从加倍后的值起。

`receive` 不调用 `push`。ack 把窗口打开之后，要调用方再 `push` 才会把新字节发出去。测试夹具的 `AckReceived` 自己做了 `receive` 然后 `push`，读测试时不要把这当成 Sender 的行为。

## tick：倒计时，到期只发队头

时间只来自参数 `ms_since_last_tick`。没有 `chrono`。

```text
定时器没开 → 返回
剩余 > 这次流逝 → 减掉，返回
否则到期:
  队列空 → 关掉定时器（正常路径不该发生）
  transmit(outstanding_.front())     // 不经 send_segment，不复制进队列
  若 window_size_ != 0:
      consecutive_retransmissions_++
      rto_ms_ *= 2
  用当前 rto_ms_ 重开定时器
```

`剩余 == 流逝` 也算到期（判断是 `>`，不是 `>=`）。所以 `tick(rto - 1)` 不发，再 `tick(1)` 发。一次 `tick(rto)` 也发。

一次 tick 跳过很长时间，也只重传一次。多出来的毫秒丢掉，定时器按当前 RTO 整段重开。不会在一次 tick 里连退多次。

重传不走 `send_segment`，所以：

- 队列里不会出现第二份拷贝，在途序号不变；
- `next_abs_` 不变；
- 发出去的是当初存下的那段，序号、SYN、FIN、载荷都相同。

只重传最早一段，不重传后面的，也不把多段拼起来，也不裁掉部分 ack 已经盖住的前缀。

`window_size_ != 0` 才加倍、才增加 `consecutive_retransmissions_`。零窗口超时仍重传那 1 个序号的探测段，但 RTO 不变，次数也不加。零窗口不是丢包，不能把探测越推越稀，也不能把连接判死。

窗口是 1 并且已经有 1 个序号在途，看起来也是“发不出新东西”，但 `window_size_` 不是 0，超时**要**退避。`send_extra` 用这个把两种情况分开：零窗口每次都是同一个 RTO；窗口为 1 时依次等 `rto`、`2*rto`、`4*rto`。

RTO 没有上限。`uint64_t` 加倍，测试里最多九次超时，不会绕回。这是实验简化，不是生产 TCP。

## 调用顺序

典型一次交换，调用方（测试或以后的连接对象）要自己排好：

```text
writer().push(data) / close()     字节进入流，还没切段
sender.push(transmit)             按当前窗口切段发出
sender.receive(ack, window)       推进 ack_abs_，可能停表
sender.push(transmit)             用新打开的窗口再切
sender.tick(ms, transmit)         也许重传队头
```

定时器的几个时刻：

| 事件 | 定时器 |
| --- | --- |
| 发出第一段占号的段 | 用当前 RTO 启动。已在跑则不动 |
| 又发出后续段 | 不动。表跟的是最早那段 |
| 新 ack，队列还有段 | 回到初始 RTO，重开 |
| 新 ack，队列空 | 停止。之后 `tick(很大)` 也不重传 |
| 重复 ack / 旧 ack / 超过 `next_abs_` 的 ack | 不动 |
| 超时，窗口非 0 | 重传队头，RTO 加倍，重开 |
| 超时，窗口为 0 | 重传队头，RTO 不变，重开 |

## 一条具体时间线

ISN = 100，初始 RTO = 1000。应用先没写数据。

1. `push`。窗口初始 1，发出 `seq=100, SYN`。`next_abs_=1`，在途 1，定时器 1000。流里就算已经有数据，也挤不进这 1 个序号。
2. `tick(999)` 无事。`tick(1)` 重传同一段 SYN。次数变为 1，RTO 变为 2000。`next_abs_` 仍是 1。
3. `receive(ack=101, win=137)`，然后调用方 `push`。101 解开是绝对序号 1，SYN 整段被丢掉，定时器停，RTO 回到 1000。
4. 应用写入 `"hello"` 并 `close`，再 `push`。一段搞定：`seq=101, "hello", FIN`，长度 6。`next_abs_=7`，在途 6。
5. `tick(1000)` 重传这一整段，不是只重传 FIN。次数 1，RTO 2000。
6. `receive(ack=107)`。队列空，定时器停。`make_empty_message().seqno` 仍是 `ISN+7`，因为空段不消耗序号。

零窗口接着走。假设第 3 步的窗口是 0，流里是 `"abc"` 且已 close：

1. `push` 把窗口临时当成 1，只发 `"a"`。FIN 占不下。
2. 连续超时都重传 `"a"`，RTO 不变，`consecutive_retransmissions_` 保持 0。
3. ack 掉 `"a"` 且窗口仍是 0：这段弹出，定时器停，调用方的下一次 `push` 发 `"b"`。`"c"` 和 FIN 同样各占一次探测。
4. 若在 `"a"` 还在途时 `close`，`push` 看到在途已经是 1，什么都不发。FIN 不能插队。

部分 ack：第 4 步若发的是 `"abc"`（绝对序号 1..3），ack 到 3（绝对序号 2）。队头段的结尾是 4，`4 <= 2` 不成立，整段留下。在途仍是 3。超时重传的还是 `"abc"`，不是 `"bc"`。

## RST

两个方向都是“做最少的事”：

- 流出错：之后的 `push` 发一个空 RST 就返回，不再读 `"hello"` 这种新载荷。`make_empty_message()` 的 RST 位跟着 `has_error()` 走。
- 收到 RST：`writer().set_error()` 后立刻返回。不更新窗口，不处理 ackno。

已经在 `outstanding_` 里的段是发出时的快照。之后才出错，`tick` 重传的仍是旧段，RST 位不会被改写。Checkpoint 3 的测试不查这条。上层若要中止连接，应看错误标志和重传次数，不要假设队列里的旧段会自己变成 RST。

## 这份实现不做的事

- 不估 RTT。RTO 只有“初始值”和“超时加倍”，新 ack 打回初始值。没有 Karn 算法，也没有上限。
- 没有拥塞窗口，只有对端通告的窗口。
- 不裁段，没有 SACK。窗口按未裁剪的在途和来卡，不是 `ack + window - next`。
- 零窗口没有单独的 persist timer，只是同一把重传表、但不退避。
- 重传次数超过 8 不在这里拆连接。`tick` 第 9 次超时后次数变成 9，测试因此认为 exceeded；Sender 自己继续发。
- 不在 `receive` 里自动 `push`。

读 `tests/send_retx.cc` 里 “Retx after multiple sends” 时不要看走眼。`Push "BB"` 已经把 B 放进测试的输出队列，但测试没把它取走。随后 `tick` 只重传 A。队列里于是有两条：先是没取走的 B，再是重传的 A。注释写 “order does not matter”，是因为那个 `ExpectMessage` 不检查内容，不是说一次超时会把 A 和 B 都重传。下一次超时检查的是只有 A。
