#include <iostream>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

static constexpr size_t ARP_RETX_MS = 5000;
static constexpr size_t ARP_TTL_MS = 30000;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_ip = next_hop.ipv4_numeric();
  if ( auto it = arp_table_.find( next_ip ); it != arp_table_.end() ) {
    EthernetFrame frame;
    frame.header.src = ethernet_address_;
    frame.header.dst = it->second.eth;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize( dgram );
    transmit( frame );
    return;
  }

  pending_datagrams_[next_ip].push_back( dgram );
  if ( arp_wait_ms_.contains( next_ip ) ) {
    return;
  }
  arp_wait_ms_[next_ip] = ARP_RETX_MS;
  ARPMessage arp;
  arp.opcode = ARPMessage::OPCODE_REQUEST;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = {};
  arp.target_ip_address = next_ip;
  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = ETHERNET_BROADCAST;
  frame.header.type = EthernetHeader::TYPE_ARP;
  frame.payload = serialize( arp );
  transmit( frame );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( move( dgram ) );
    }
    return;
  }

  if ( frame.header.type != EthernetHeader::TYPE_ARP ) {
    return;
  }

  ARPMessage arp;
  if ( not parse( arp, frame.payload ) ) {
    return;
  }

  arp_table_[arp.sender_ip_address] = ARPEntry { arp.sender_ethernet_address, ARP_TTL_MS };
  arp_wait_ms_.erase( arp.sender_ip_address );

  if ( auto it = pending_datagrams_.find( arp.sender_ip_address ); it != pending_datagrams_.end() ) {
    for ( const auto& dgram : it->second ) {
      send_datagram( dgram, Address::from_ipv4_numeric( arp.sender_ip_address ) );
    }
    pending_datagrams_.erase( it );
  }

  if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
    ARPMessage reply;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ethernet_address = ethernet_address_;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.target_ethernet_address = arp.sender_ethernet_address;
    reply.target_ip_address = arp.sender_ip_address;
    EthernetFrame out;
    out.header.src = ethernet_address_;
    out.header.dst = arp.sender_ethernet_address;
    out.header.type = EthernetHeader::TYPE_ARP;
    out.payload = serialize( reply );
    transmit( out );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if ( it->second.ttl_ms <= ms_since_last_tick ) {
      it = arp_table_.erase( it );
    } else {
      it->second.ttl_ms -= ms_since_last_tick;
      ++it;
    }
  }
  for ( auto it = arp_wait_ms_.begin(); it != arp_wait_ms_.end(); ) {
    if ( it->second <= ms_since_last_tick ) {
      pending_datagrams_.erase( it->first );
      it = arp_wait_ms_.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      ++it;
    }
  }
}
