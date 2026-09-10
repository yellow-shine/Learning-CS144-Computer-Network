Checkpoint 0 Writeup
====================

My name: [Stanford identity/email homework not executed]

My SUNet ID: [not executed]

I collaborated with: none

I would like to credit/thank these classmates for their help: none

This lab took me about [n] hours to do. I did not attend the lab session.

My secret code from section 2.1 was: [Stanford email homework not executed]

I was surprised by or edified to learn that: HTTP/1.1 keep-alive means the client must send Connection: close and read until EOF, or it hangs waiting for the next request.

ByteStream: `std::string buffer_` plus `start_` on the base class (not `deque`).

- string: `peek()` is one contiguous `string_view` (tests compare the whole view); one allocation; compact with `erase` when `start_ > 4096` and half consumed.
- `deque<char>` / `deque<string>`: extra fragments, so `peek()` needs a copy or cannot be one view; more code for little gain.

Chose string.

webget: `TCPSocket` to `host:http`, `GET` HTTP/1.1 with `Host` and `Connection: close`, `\r\n` line endings, `shutdown(SHUT_WR)`, read until EOF. Depends on external HTTP to `cs144.keithw.org:80`.

外部待验证: DNS resolves `104.196.238.229`; host direct TCP to :80 times out; container connect then immediate EOF (0 bytes); HTTP proxy `http://192.168.71.40:7890` returns 502. Did not change tests.

- Optional: I had unexpected difficulty with: this network cannot reach the course hasher over port 80.
