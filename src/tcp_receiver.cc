#include "tcp_receiver.hh"

#include <algorithm>
#include <cstdint>

using namespace std;

// 实现思路：只记 ISN。流下标 = 绝对序号减去 SYN 占的那一位。
// ackno 是下一个要的绝对序号：bytes_pushed+1，流已关闭再加 FIN。

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  // SYN 之前的数据/纯 FIN 都不能组装，也不能出 ackno。
  if ( not isn_.has_value() ) {
    if ( not message.SYN ) {
      return;
    }
    isn_ = message.seqno;
  }

  // checkpoint 用已推进的绝对序号（含 SYN），不用流下标。
  const uint64_t checkpoint = writer().bytes_pushed() + 1;
  const uint64_t abs_seqno = message.seqno.unwrap( *isn_, checkpoint );
  // SYN 段的 payload 从流下标 0 开始；后续段要减掉 SYN。
  const uint64_t first_index = message.SYN ? abs_seqno : abs_seqno - 1;
  reassembler_.insert( first_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  const uint64_t cap = writer().available_capacity();
  msg.window_size = static_cast<uint16_t>( min( cap, uint64_t { UINT16_MAX } ) );
  msg.RST = reader().has_error();
  if ( isn_.has_value() ) {
    uint64_t abs_ack = writer().bytes_pushed() + 1; // +1 是 SYN
    if ( writer().is_closed() ) {
      ++abs_ack; // FIN 也占一个序号，但不在 ByteStream 里
    }
    msg.ackno = Wrap32::wrap( abs_ack, *isn_ );
  }
  return msg;
}
