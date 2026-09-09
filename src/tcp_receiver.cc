#include "tcp_receiver.hh"

#include <algorithm>
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( not isn_.has_value() ) {
    if ( not message.SYN ) {
      return;
    }
    isn_ = message.seqno;
  }

  const uint64_t checkpoint = writer().bytes_pushed() + 1;
  const uint64_t abs_seqno = message.seqno.unwrap( *isn_, checkpoint );
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
    uint64_t abs_ack = writer().bytes_pushed() + 1;
    if ( writer().is_closed() ) {
      ++abs_ack;
    }
    msg.ackno = Wrap32::wrap( abs_ack, *isn_ );
  }
  return msg;
}
