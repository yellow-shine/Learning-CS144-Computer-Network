#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t n = 0;
  for ( const auto& seg : outstanding_ ) {
    n += seg.sequence_length();
  }
  return n;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

TCPSenderMessage TCPSender::make_message( uint64_t abs_seqno, bool syn, string payload, bool fin ) const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( abs_seqno, isn_ );
  msg.SYN = syn;
  msg.payload = move( payload );
  msg.FIN = fin;
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::send_segment( const TCPSenderMessage& msg, const TransmitFunction& transmit )
{
  transmit( msg );
  if ( msg.sequence_length() == 0 ) {
    return;
  }
  outstanding_.push_back( msg );
  if ( not timer_running_ ) {
    timer_running_ = true;
    timer_remaining_ms_ = rto_ms_;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return make_message( next_abs_, false, {}, false );
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( input_.has_error() ) {
    transmit( make_empty_message() );
    return;
  }

  const uint64_t effective_window = window_size_ == 0 ? 1 : window_size_;

  while ( true ) {
    const uint64_t inflight = sequence_numbers_in_flight();
    if ( inflight >= effective_window ) {
      return;
    }
    const uint64_t remaining = effective_window - inflight;

    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_abs_, isn_ );
    msg.RST = input_.has_error();

    uint64_t used = 0;
    if ( not syn_sent_ ) {
      msg.SYN = true;
      syn_sent_ = true;
      used += 1;
    }

    const uint64_t cap = min<uint64_t>( remaining - used, TCPConfig::MAX_PAYLOAD_SIZE );
    string payload;
    read( reader(), cap, payload );
    used += payload.size();
    msg.payload = move( payload );

    if ( reader().is_finished() && not fin_sent_ && used < remaining ) {
      msg.FIN = true;
      fin_sent_ = true;
      used += 1;
    }

    if ( used == 0 ) {
      return;
    }

    send_segment( msg, transmit );
    next_abs_ += msg.sequence_length();
  }
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    writer().set_error();
    return;
  }

  window_size_ = msg.window_size;

  if ( not msg.ackno.has_value() ) {
    return;
  }

  const uint64_t abs_ack = msg.ackno->unwrap( isn_, next_abs_ );
  if ( abs_ack > next_abs_ ) {
    return;
  }

  const bool new_ack = abs_ack > ack_abs_;
  if ( new_ack ) {
    ack_abs_ = abs_ack;
    while ( not outstanding_.empty() ) {
      const auto& seg = outstanding_.front();
      const uint64_t seg_abs = seg.seqno.unwrap( isn_, next_abs_ );
      if ( seg_abs + seg.sequence_length() <= ack_abs_ ) {
        outstanding_.pop_front();
      } else {
        break;
      }
    }
    consecutive_retransmissions_ = 0;
    rto_ms_ = initial_RTO_ms_;
    if ( outstanding_.empty() ) {
      timer_running_ = false;
    } else {
      timer_running_ = true;
      timer_remaining_ms_ = rto_ms_;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( not timer_running_ ) {
    return;
  }
  if ( timer_remaining_ms_ > ms_since_last_tick ) {
    timer_remaining_ms_ -= ms_since_last_tick;
    return;
  }

  timer_remaining_ms_ = 0;
  if ( outstanding_.empty() ) {
    timer_running_ = false;
    return;
  }

  transmit( outstanding_.front() );
  if ( window_size_ != 0 ) {
    ++consecutive_retransmissions_;
    rto_ms_ *= 2;
  }
  timer_running_ = true;
  timer_remaining_ms_ = rto_ms_;
}
