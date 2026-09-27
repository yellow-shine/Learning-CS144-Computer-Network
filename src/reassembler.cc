#include "reassembler.hh"

#include <algorithm>
#include <iterator>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 结束下标用未裁剪的长度。空的最后一段也是合法 EOF。
  if ( is_last_substring ) {
    eof_index_ = first_index + data.size();
  }

  trim_to_window( first_index, data );
  merge_pending( first_index, move( data ) );
  push_contiguous();

  // 洞填上之前不能 close，即使已经看到 FIN。
  if ( eof_index_.has_value() && next_index_ >= *eof_index_ ) {
    output_.writer().close();
  }
}

void Reassembler::trim_to_window( uint64_t& first_index, string& data ) const
{
  const uint64_t first_unassembled = next_index_;
  // ByteStream 中尚未读取的字节也占容量，不能直接用 next_index_ + 总容量。
  const uint64_t first_unacceptable = next_index_ + output_.writer().available_capacity();

  if ( first_index < first_unassembled ) {
    const uint64_t bytes_to_skip = min<uint64_t>( data.size(), first_unassembled - first_index );
    data.erase( 0, bytes_to_skip );
    first_index += bytes_to_skip;
  }
  if ( first_index >= first_unacceptable ) {
    data.clear();
  } else if ( data.size() > first_unacceptable - first_index ) {
    data.resize( first_unacceptable - first_index );
  }
}

void Reassembler::merge_pending( uint64_t first_index, string data )
{
  if ( data.empty() ) {
    return;
  }

  auto interval = pending_.upper_bound( first_index );
  if ( interval != pending_.begin() ) {
    auto previous_interval = prev( interval );
    if ( previous_interval->first + previous_interval->second.size() >= first_index ) {
      interval = previous_interval;
    }
  }

  // 有重叠或相邻的前一个区间时复用它，否则插入新区间。
  if ( interval == pending_.end() || interval->first > first_index ) {
    interval = pending_.emplace_hint( interval, first_index, move( data ) );
  } else {
    const uint64_t interval_end = interval->first + interval->second.size();
    if ( first_index + data.size() > interval_end ) {
      // 只拼尚未覆盖的后缀，重复字节不再存一份。
      interval->second.append( data.substr( interval_end - first_index ) );
    }
  }

  auto next_interval = next( interval );
  while ( next_interval != pending_.end()
          && interval->first + interval->second.size() >= next_interval->first ) {
    const uint64_t interval_end = interval->first + interval->second.size();
    if ( next_interval->first + next_interval->second.size() > interval_end ) {
      interval->second.append( next_interval->second.substr( interval_end - next_interval->first ) );
    }
    next_interval = pending_.erase( next_interval );
  }
}

void Reassembler::push_contiguous()
{
  while ( not pending_.empty() && pending_.begin()->first == next_index_ ) {
    auto interval = pending_.extract( pending_.begin() );
    const uint64_t byte_count = interval.mapped().size();
    output_.writer().push( move( interval.mapped() ) );
    next_index_ += byte_count;
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t byte_count = 0;
  for ( const auto& [first_index, data] : pending_ ) {
    byte_count += data.size();
  }
  return byte_count;
}
