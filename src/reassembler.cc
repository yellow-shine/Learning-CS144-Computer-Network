#include "reassembler.hh"

#include <algorithm>

using namespace std;

// 实现思路：map 存不重叠区间。窗口是 [next_index, next_index+available_capacity)，
// 窗外丢弃。eof 先按原始长度记下，裁掉之后不能提前 close。

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 结束下标用未裁剪的长度。空的最后一段也是合法 EOF。
  if ( is_last_substring ) {
    eof_index_ = first_index + data.size();
  }

  Writer& w = output_.writer();
  const uint64_t lo = next_index_;
  const uint64_t hi = next_index_ + w.available_capacity();

  if ( first_index < lo ) {
    const uint64_t skip = min<uint64_t>( data.size(), lo - first_index );
    data.erase( 0, skip );
    first_index += skip;
  }
  if ( first_index >= hi ) {
    data.clear();
  } else if ( data.size() > hi - first_index ) {
    data.resize( hi - first_index );
  }

  if ( not data.empty() ) {
    auto it = pending_.upper_bound( first_index );
    if ( it != pending_.begin() ) {
      auto prev = std::prev( it );
      if ( prev->first + prev->second.size() >= first_index ) {
        const uint64_t prev_end = prev->first + prev->second.size();
        // 只拼尚未覆盖的后缀，重复字节不再存一份。
        if ( first_index + data.size() > prev_end ) {
          prev->second.append( data.substr( prev_end - first_index ) );
        }
        it = prev;
      } else {
        it = pending_.emplace_hint( it, first_index, move( data ) );
      }
    } else {
      it = pending_.emplace_hint( it, first_index, move( data ) );
    }

    auto nxt = std::next( it );
    while ( nxt != pending_.end() && it->first + it->second.size() >= nxt->first ) {
      const uint64_t cur_end = it->first + it->second.size();
      if ( nxt->first + nxt->second.size() > cur_end ) {
        it->second.append( nxt->second.substr( cur_end - nxt->first ) );
      }
      nxt = pending_.erase( nxt );
    }
  }

  while ( not pending_.empty() && pending_.begin()->first == next_index_ ) {
    auto node = pending_.extract( pending_.begin() );
    const uint64_t n = node.mapped().size();
    w.push( move( node.mapped() ) );
    next_index_ += n;
  }

  // 洞填上之前不能 close，即使已经看到 FIN。
  if ( eof_index_.has_value() && next_index_ >= *eof_index_ ) {
    w.close();
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t n = 0;
  for ( const auto& [_, s] : pending_ ) {
    n += s.size();
  }
  return n;
}
