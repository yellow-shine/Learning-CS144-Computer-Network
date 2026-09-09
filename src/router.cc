#include "router.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  routes_.push_back( { route_prefix, prefix_length, next_hop, interface_num } );
}

static bool prefix_match( uint32_t addr, uint32_t prefix, uint8_t len )
{
  if ( len == 0 ) {
    return true; // shifting a uint32_t by 32 is undefined
  }
  const uint32_t mask = ~uint32_t { 0 } << ( 32 - len );
  return ( addr & mask ) == ( prefix & mask );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& iface : interfaces_ ) {
    auto& q = iface->datagrams_received();
    while ( not q.empty() ) {
      InternetDatagram dgram = move( q.front() );
      q.pop();

      optional<Route> best;
      for ( const auto& r : routes_ ) {
        if ( prefix_match( dgram.header.dst, r.route_prefix, r.prefix_length ) ) {
          if ( not best.has_value() || r.prefix_length > best->prefix_length ) {
            best = r;
          }
        }
      }
      if ( not best.has_value() ) {
        continue;
      }
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      --dgram.header.ttl;
      dgram.header.compute_checksum();
      const Address hop = best->next_hop.has_value() ? *best->next_hop
                                                     : Address::from_ipv4_numeric( dgram.header.dst );
      interface( best->interface_num )->send_datagram( dgram, hop );
    }
  }
}
