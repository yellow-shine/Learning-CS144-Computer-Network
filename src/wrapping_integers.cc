#include "wrapping_integers.hh"

using namespace std;

// 实现思路：wrap 就是加 ISN 后取低 32 位。unwrap 取离 checkpoint 最近的那一圈，
// 用有符号 32-bit 偏移，不用逐圈搜索。

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint32_t checkpoint_wrapped = wrap( checkpoint, zero_point ).raw_value_;
  // 转成 int32 后是 [-2^31, 2^31) 的最短距离。
  const int32_t offset = static_cast<int32_t>( raw_value_ - checkpoint_wrapped );
  const int64_t abs = static_cast<int64_t>( checkpoint ) + offset;
  if ( abs >= 0 ) {
    return static_cast<uint64_t>( abs );
  }
  // checkpoint=0 且答案是 2^32-1 时，有符号加法会得到负数。
  return static_cast<uint64_t>( abs + ( int64_t { 1 } << 32 ) );
}
