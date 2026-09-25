#include "router.hh"

#include <iostream>

using namespace std;

// 实现思路：路由表线性扫描，取最长前缀。无匹配或 TTL 减到 0 就丢。
// 直接连接的下一跳是目的地址本身。改 TTL 后必须重算校验和。

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
    return true; // uint32 左移 32 位是未定义行为，默认路由单独处理
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
        continue; // 减之前已经是 0，或减完会变成 0
      }
      --dgram.header.ttl;
      dgram.header.compute_checksum();
      const Address hop = best->next_hop.has_value() ? *best->next_hop
                                                     : Address::from_ipv4_numeric( dgram.header.dst );
      interface( best->interface_num )->send_datagram( dgram, hop );
    }
  }
}
