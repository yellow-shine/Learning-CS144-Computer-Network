#include "byte_stream.hh"

#include <cassert>
#include <iostream>
#include <string>

using namespace std;

// 只构造 ByteStream。Reader / Writer 没有自己的成员，
// reader() / writer() 把同一个对象转成两种接口。
int main()
{
  ByteStream stream { 8 };

  Writer& out = stream.writer();
  Reader& in = stream.reader();
  assert( static_cast<ByteStream*>( &in ) == &stream );
  assert( static_cast<ByteStream*>( &out ) == &stream );

  out.push( "hello" );
  cout << "peek=\"" << in.peek() << "\""
       << " buffered=" << in.bytes_buffered() << " room=" << out.available_capacity()
       << " pushed=" << out.bytes_pushed() << "\n";

  // 容量只限制还没读走的字节。多出来的尾部丢掉，不算 error。
  out.push( " world!!!" );
  assert( in.peek() == "hello wo" );
  assert( out.bytes_pushed() == 8 );
  assert( out.available_capacity() == 0 );
  assert( !stream.has_error() );
  cout << "overflow peek=\"" << in.peek() << "\" pushed=" << out.bytes_pushed() << "\n";

  in.pop( 5 ); // 只丢掉 "hello"，不拷贝
  assert( in.peek() == " wo" );
  assert( out.available_capacity() == 5 );
  cout << "after pop peek=\"" << in.peek() << "\" room=" << out.available_capacity() << "\n";

  out.push( "xyz" );
  out.close();
  assert( out.is_closed() );
  assert( !in.is_finished() ); // 关了，但缓冲区还有字节
  out.push( "ignored" );       // close 之后再写无效

  string got;
  read( in, 100, got ); // peek + pop，最多 100 字节
  cout << "read=\"" << got << "\" finished=" << in.is_finished() << " popped=" << in.bytes_popped() << "\n";
  assert( got == " woxyz" );
  assert( in.is_finished() );
  assert( in.bytes_popped() == 11 );
  assert( out.bytes_pushed() == 11 );
}

/*
peek="hello" buffered=5 room=3 pushed=5
overflow peek="hello wo" pushed=8
after pop peek=" wo" room=5
read=" woxyz" finished=1 popped=11
*/
