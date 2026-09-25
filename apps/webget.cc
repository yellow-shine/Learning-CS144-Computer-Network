#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <sys/socket.h>

using namespace std;

// 实现思路：用操作系统的 TCPSocket 发一次 HTTP/1.1 GET。行尾必须是 \r\n。
// Connection: close 让服务器回完就关写端；客户端要读到 EOF，一次 read 不够。
void get_URL( const string& host, const string& path )
{
  TCPSocket sock;
  sock.connect( Address { host, "http" } );
  const string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  sock.write( req );
  sock.shutdown( SHUT_WR ); // 本端不再发请求，服务器才能结束响应
  while ( not sock.eof() ) {
    string buf;
    sock.read( buf );
    cout << buf;
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
