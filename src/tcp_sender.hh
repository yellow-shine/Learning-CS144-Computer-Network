#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

class TCPSender
{
public:
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), rto_ms_( initial_RTO_ms )
  {}

  TCPSenderMessage make_empty_message() const;
  void receive( const TCPReceiverMessage& msg );
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;
  void push( const TransmitFunction& transmit );
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  uint64_t sequence_numbers_in_flight() const;
  uint64_t consecutive_retransmissions() const;
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }
  TCPSenderMessage make_message( uint64_t abs_seqno, bool syn, std::string payload, bool fin ) const;
  void send_segment( const TCPSenderMessage& msg, const TransmitFunction& transmit );

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t rto_ms_;
  uint64_t next_abs_ {}; // 下一个要发送的绝对序号（含 SYN）
  uint64_t ack_abs_ {};  // 已确认到的绝对序号
  uint64_t window_size_ { 1 }; // 收到通告前按 1；0 只在 push 里临时当成 1
  uint64_t consecutive_retransmissions_ {};
  bool timer_running_ {};
  uint64_t timer_remaining_ms_ {};
  bool syn_sent_ {};
  bool fin_sent_ {};
  std::deque<TCPSenderMessage> outstanding_ {}; // 只存占序号的整段，不裁剪
};
