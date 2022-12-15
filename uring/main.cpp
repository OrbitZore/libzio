#include <cstring>
#include <memory>
#include "uringtest.h"
#include "zio_ip.hpp"
#include "zio_types.hpp"
using namespace zio;
using namespace zio::ip;
awaitable<int> g1() {
  co_yield 10;
  co_return 11;
}
awaitable<int> g2() {
  co_return 100;
}
awaitable<int> g3() {
  co_yield 1000;
  co_yield 1001;
  co_return 1002;
}
awaitable<void> f() {
  auto G1 = g1();
  auto G2 = g2();
  auto G3 = g3();
  while (true) {
    co_await wait_any(G1, G2, G3);
    auto x = G1.get_return();
    auto y = G2.get_return();
    auto z = G3.get_return();
    if (!x && !y && !z)
      break;
    if (x)
      cout << *x << endl;
    if (y)
      cout << *y << endl;
    if (z)
      cout << *z << endl;
  }
  co_return;
}
template<types::Address Address,types::Protocol Protocol>
awaitable<void> echo_server(connection<Address,Protocol> con) {
  // const char *c="hello!";
  // co_await con.async_write(c, strlen(c));
  char c[1024];
  while (1) {
    int n1 = co_await con.async_read(c, 1024);
    cerr << "recv:" << n1 << endl;
    if (n1 <= 0)
      break;
    int n2 = co_await con.async_write(c, n1);
    cerr << "send:" << n2 << endl;
    if (n2 <= 0)
      break;
  }
  con.close();
  co_return;
}
awaitable<void> server(io_context& ctx) {
  tcp::acceptor<ipv4> acceptor(ipv4::from_pret("0.0.0.0", 4000));
  while (1) {
    connection con = co_await acceptor.async_accept();
    if (con) {
      ctx.reg(echo_server(std::move(con)));
    }
  }
  co_return;
}
template<types::Address Address,types::Protocol Protocol>
awaitable<void> echo_client(connection<Address,Protocol> con) {
  char c[1024];
  while (1) {
    int n1 = co_await con.async_read(c, 1024);
    cerr << "recv:" << n1 << endl;
    if (n1 <= 0)
      break;
    int n2 = co_await con.async_write(c, n1);
    cerr << "send:" << n2 << endl;
    if (n2 <= 0)
      break;
  }
  con.close();
  co_return;
}
template<types::Address Address,types::Protocol Protocol>
awaitable<void> request_uuid_client(connection<Address,Protocol> con) {
  {
    static const char* uuid_request =
        "GET /uuid HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "User-Agent: curl/7.86.0\r\n"
        "accept: */*\r\n\r\n";
    static const int len = strlen(uuid_request);
    const char* c = uuid_request;
    while (c != uuid_request + len) {
      int n = co_await con.async_send_zc(c, len - (c - uuid_request));
      if (n <= 0)
        break;
      c += n;
    }
    cerr << "write::" << (c - uuid_request) << endl;
  }
  char c[1024];
  string res;
  while (1) {
    int n1 = co_await con.async_recv(c, 1024);
    if (n1 <= 0)
      break;
    res += string_view(c, c + n1);
    cerr << "recv:" << string_view(c, c + n1) << endl
         << (int)res.back() << " " << (int)res.end()[-2] << endl;
    if (res.ends_with("\r") || res.ends_with("\n"))
      break;
  }
  con.close();
  cerr << res << endl;
  co_return;
}
awaitable<void> client(io_context& ctx) {
  const auto ip = ipv4::from_pret("127.0.0.1", 1244);
  while (true) {
    connection con = co_await tcp::async_connect(ip);
    if (con) {
      ctx.reg(echo_server(std::move(con)));
    } else {
      cerr << "connect fail" << endl;
    }
  }
  co_return;
}
awaitable<void> echo_client_await(io_context& ctx, ipv4 addr) {
  while (1) {
    auto con = tcp::async_connect(addr);
    auto tm = async_timeout(3s);
    co_await wait_any(con, tm);
    if (!con) {
      auto connection = con.get_return();
      if (connection) {
        cout << "connect success!" << con.get_return().fd << ","
             << con.get_return().status << endl;
        ctx.reg(request_uuid_client(::move(connection)));
      } else {
        cout << "connect fail!" << con.get_return().status << endl;
      }
    } else {
      cout << "connect timeout:" << con.get_return().status << endl;
    }
    co_await wait_all(tm, con);
  }
}
template<types::Address Address,types::Protocol Protocol>
awaitable<void> udp_echo_server(io_context& ctx, connection<Address,Protocol> con) {
  cerr << "listening" << endl;
  char buffer[8];
  io_vector v;
  v.push_back({buffer, 8});
  message_header m{};
  union {
    ipv4 addr4;
    ipv6 addr6;
  } addr;
  m.set_address(&addr, sizeof(addr));
  m.set_io_vector(v);
  m.msg_flags |= MSG_TRUNC;
  int n;
  while (1) {
    while ((n = co_await con.async_recvmsg(&m)) >= 0) {
      string ip;
      if (addr.addr4.sin_family == ipv4::AF)
        ip = addr.addr4.to_pret();
      if (addr.addr4.sin_family == ipv6::AF)
        ip = addr.addr6.to_pret();
      cerr << ip << " recived " << n << " bytes" << endl;
      v[0].iov_len = n;
      co_await con.async_sendmsg(&m);
      v[0].iov_len = 8;
    }
    cerr << "exit:" << strerror(abs(n)) << endl;
    co_await async_timeout(1s);
  }
}

void curl_httpbin() {
  io_context ctx;
  const auto ip = *ipv4::from_url("http://httpbin.org");
  ctx.reg(echo_client_await(ctx, ip));
  ctx.run();
}

int main(int argc, char* argv[]) {
  io_context ctx;
  // const auto ip =ipv4::from_pret("127.0.0.1",1244);
  // const auto ip =*ipv4::from_url("http://httpbin.org");
  // auto ip = *ipv4::from_url("0.0.0.0:21412");
  // cerr<<ip->to_pret()<<endl;
  // ctx.reg(echo_client_await(ctx,ip));
  // ctx.reg(udp_echo_server(ctx, udp::open(ip)));
  // ctx.reg(server(ctx));
  ctx.reg(f());
  ctx.run();
  // struct io_uring_cqe* cqe;
  /* get an sqe and fill in a READV operation */

  /* tell the kernel we have an sqe ready for consumption */
  // io_uring_submit(&ring);
  // /* wait for the sqe to complete */
  // io_uring_wait_cqe(&ring, &cqe);
  // /* read and process cqe event */
  // // app_handle_cqe(cqe);
  // io_uring_cqe_seen(&ring, cqe);
  // io_uring_queue_exit(&ring);
}