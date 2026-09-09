#include "byte_stream.hh"

#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( closed_ || data.empty() ) {
    return;
  }
  const uint64_t n = min( available_capacity(), static_cast<uint64_t>( data.size() ) );
  buffer_.append( data.data(), static_cast<size_t>( n ) );
  bytes_pushed_ += n;
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( buffer_.size() - start_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return string_view { buffer_ }.substr( start_ );
}

void Reader::pop( uint64_t len )
{
  const uint64_t n = min( len, bytes_buffered() );
  start_ += n;
  bytes_popped_ += n;
  if ( start_ > 4096 && start_ * 2 >= buffer_.size() ) {
    buffer_.erase( 0, start_ );
    start_ = 0;
  }
}

bool Reader::is_finished() const
{
  return closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size() - start_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
