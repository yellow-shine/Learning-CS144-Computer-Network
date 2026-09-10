#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint32_t checkpoint_wrapped = wrap( checkpoint, zero_point ).raw_value_;
  const int32_t offset = static_cast<int32_t>( raw_value_ - checkpoint_wrapped );
  const int64_t abs = static_cast<int64_t>( checkpoint ) + offset;
  if ( abs >= 0 ) {
    return static_cast<uint64_t>( abs );
  }
  return static_cast<uint64_t>( abs + ( int64_t { 1 } << 32 ) );
}
