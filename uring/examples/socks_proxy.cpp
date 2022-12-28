#include <bits/stdc++.h>
#include <netinet/in.h>
#include <memory>
#include "uringtest.h"
#include "zio_buffer.hpp"
#include "zio_ip.hpp"
#include "zio_types.hpp"

using namespace zio;
using namespace zio::buffer;
using namespace std;
io_context ctx;

template <size_t N, types::Address Address, types::Protocol Protocol>
awaitable<optional<array<char, N>>> async_read(
    connection<Address, Protocol>& con) {
  array<char, N> a;
  int c = 0;
  while (c < N) {
    int n = co_await con.async_read(&a[c], N - c);
    if (n <= 0)
      co_return {};
    c += n;
  }
  co_return a;
}

template <types::Address Address, types::Protocol Protocol>
awaitable<optional<vector<char>>> async_read(connection<Address, Protocol>& con,
                                             size_t N) {
  vector<char> a;
  a.resize(N);
  int c = 0;
  while (c < N) {
    int n = co_await con.async_read(&a[c], N - c);
    if (n <= 0)
      co_return {};
    c += n;
  }
  co_return a;
}

template <types::Address Address, types::Protocol Protocol>
awaitable<optional<char>> async_getchar(connection<Address, Protocol>& con) {
  char c;
  int n = co_await con.async_read(&c, 1);
  if (n <= 0)
    co_return {};
  co_return c;
}

template <types::Address Address, types::Protocol Protocol>
awaitable<bool> async_read(connection<Address, Protocol>& con,
                           vector<char>& a) {
  auto N = a.size();
  int c = 0;
  while (c < N) {
    int n = co_await con.async_read(&a[c], N - c);
    if (n <= 0)
      co_return false;
    c += n;
  }
  co_return true;
}

template <types::Address Address, types::Protocol Protocol, size_t N>
awaitable<bool> async_read(connection<Address, Protocol>& con,
                           array<char, N>& a) {
  int c = 0;
  while (c < N) {
    int n = co_await con.async_read(&a[c], N - c);
    if (n <= 0)
      co_return false;
    c += n;
  }
  co_return true;
}

template <types::Address Address, types::Protocol Protocol>
awaitable<bool> async_write_r(connection<Address, Protocol>& con,
                              char* a,
                              int N) {
  int c = 0;
  while (c < N) {
    int n = co_await con.async_write(a + c, N - c);
    if (n <= 0)
      co_return false;
    c += n;
  }
  co_return true;
}

struct selection_request_message {
  char ver, nmethods;
  vector<char> methods;
  template <types::Address Address, types::Protocol Protocol>
  awaitable<bool> read_from(connection<Address, Protocol>& con) {
    auto oc = co_await async_getchar(con);
    if (!oc)
      co_return false;
    ver = *oc;
    auto onmethods = co_await async_getchar(con);
    if (!oc)
      co_return false;
    nmethods = *onmethods;
    auto omethods = co_await async_read(con, nmethods);
    if (!omethods)
      co_return false;
    methods = std::move(*omethods);
    co_return true;
  }
};

struct selection_response_message {
  char ver{0x05}, method{0x00};
  /*
          o  X'00' NO AUTHENTICATION REQUIRED
          o  X'01' GSSAPI
          o  X'02' USERNAME/PASSWORD
          o  X'03' to X'7F' IANA ASSIGNED
          o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
          o  X'FF' NO ACCEPTABLE METHODS
  */
  template <types::Address Address, types::Protocol Protocol>
  awaitable<bool> write_to(connection<Address, Protocol>& con) {
    co_return co_await async_write_r(con, (char*)this, 2);
  }
};

struct socks_request {
  char ver;
  char cmd;
  char rsv;
  char atype;
  vector<char> dst_addr;
  array<char, 2> dst_port;
  /*
          o  VER    protocol version: X'05'
          o  CMD
                  o  CONNECT X'01'
                  o  BIND X'02'
                  o  UDP ASSOCIATE X'03'
          o  RSV    RESERVED
          o  ATYP   address type of following address
                  o  IP V4 address: X'01'
                  o  DOMAINNAME: X'03'
                  o  IP V6 address: X'04'
          o  DST.ADDR       desired destination address
          o  DST.PORT desired destination port in network octetorder
  */
  template <types::Address Address, types::Protocol Protocol>
  awaitable<bool> read_from(connection<Address, Protocol>& con) {
    auto otuple4 = co_await async_read<4>(con);
    if (!otuple4)
      co_return false;
    ver = (*otuple4)[0];
    cmd = (*otuple4)[1];
    rsv = (*otuple4)[2];
    atype = (*otuple4)[3];
    if (atype == 0x01) {  // ipv4
      dst_addr.resize(4);
    } else if (atype == 0x03) {  // domainname
      auto oc = co_await async_getchar(con);
      if (!oc)
        co_return false;
      dst_addr.resize(*oc);
    } else if (atype == 0x04) {  // ipv6
      dst_addr.resize(16);
    }
    if (!co_await async_read(con, dst_addr))
      co_return false;
    if (!co_await async_read(con, dst_port))
      co_return false;
    co_return true;
  }
};

struct socks_response {
  char ver{0x05};
  char rep;
  char rsv{0x00};
  char atype;
  vector<char> bind_addr;
  array<char, 2> bind_port;
  /*
  o  VER    protocol version: X'05'
  o  REP    Reply field:
          o  X'00' succeeded
          o  X'01' general SOCKS server failure
          o  X'02' connection not allowed by ruleset
          o  X'03' Network unreachable
          o  X'04' Host unreachable
          o  X'05' Connection refused
          o  X'06' TTL expired
          o  X'07' Command not supported
          o  X'08' Address type not supported
          o  X'09' to X'FF' unassigned
  o  RSV    RESERVED
  o  ATYP   address type of following address
          o  IP V4 address: X'01'
          o  DOMAINNAME: X'03'
          o  IP V6 address: X'04'
  o  BND.ADDR       server bound address
  o  BND.PORT       server bound port in network octet order
  */

  template <types::Address Address, types::Protocol Protocol>
  awaitable<bool> write_to(connection<Address, Protocol>& con) {
    array<char, 4> c;
    c[0] = ver;
    c[1] = rep;
    c[2] = rsv;
    c[3] = atype;
    if (!co_await async_write_r(con, &c[0], 4))
      co_return false;
    if (!co_await async_write_r(con, bind_addr.data(), bind_addr.size()))
      co_return false;
    co_return co_await async_write_r(con, bind_port.data(), 2);
  }
};
awaitable<void> send(int fd, unique_ptr<array<char, 1024>> ap, int n) {
  co_await async_write(fd, ap->data(), n);
  co_return;
}

template <types::Address Address1,
          types::Protocol Protocol1,
          types::Address Address2,
          types::Protocol Protocol2>
awaitable<void> proxy(connection<Address1, Protocol1> con1,
                      connection<Address2, Protocol2> con2) {
  if (!con1) co_return;
  if (!con2) co_return;
  unique_ptr<array<char, 1024>> con1r = make_unique<array<char, 1024>>(),
                                con2r = make_unique<array<char, 1024>>();
  auto read1 = con1.async_read(con1r->begin(), 1024);
  auto read2 = con2.async_read(con2r->begin(), 1024);
  bool f1 = true, f2 = true;
  while (f1 || f2) {
    co_await wait_any(read1, read2);
    if (f1 && !read1) {
      if (read1.get_return() <= 0)
        f1 = false;
      else {
        ctx.reg(send(con2.fd, std::move(con1r), read1.get_return()));
        con1r = make_unique<array<char, 1024>>();
        read1 = con1.async_read(con1r->begin(), 1024);
      }
    }
    if (f2 && !read2) {
      if (read2.get_return() <= 0)
        f2 = false;
      else {
        ctx.reg(send(con1.fd, std::move(con2r), read2.get_return()));
        con2r = make_unique<array<char, 1024>>();
        read2 = con2.async_read(con2r->begin(), 1024);
      }
    }
  }
}

awaitable<void> socks_server(connection<ip::ipv4, ip::tcp> con) {
  selection_request_message srm;
  if (!co_await srm.read_from(con))
    co_return;
  selection_response_message sqm;
  sqm.method = 0;
  if (!co_await sqm.write_to(con))
    co_return;
  socks_request sr;
  if (!co_await sr.read_from(con))
    co_return;
  if (sr.cmd == 0x01) {  // connect
    ip::ipvx ipvx;
    memset(&ipvx, 0, sizeof(ipvx));
    if (sr.atype == 0x01) {  // ipv4
      ipvx.v4.sin_family = ip::ipv4::AF();
      memcpy(&ipvx.v4.sin_addr, sr.dst_addr.data(), 4);
      memcpy(&ipvx.v4.sin_port, sr.dst_port.data(), 2);
    } else if (sr.atype == 0x04) {  // ipv6
      ipvx.v6.sin6_family = ip::ipv6::AF();
      memcpy(&ipvx.v6.sin6_addr, sr.dst_addr.data(), 16);
      memcpy(&ipvx.v6.sin6_port, sr.dst_port.data(), 2);
    } else if (sr.atype == 0x03) {  // domain
      string s(sr.dst_addr.begin(), sr.dst_addr.end());
      ipvx.v4 = *ip::ipv4::from_url(s.data());
      ipvx.v4.sin_port = htons((unsigned char)sr.dst_port[0] << 8 |
                               (unsigned char)sr.dst_port[1]);
    }
    auto con2 = co_await async_connect<ip::ipvx, ip::tcp>(ipvx);
    socks_response sp;
    if (con2) {
      cerr<<"From "<<con.getpeer().to_pret()<<" to "<<con2.getpeer().to_pret()<<endl;
      sp.rep = 0x00;
      auto local_addr = con2.getaddr();
      if (local_addr.AF() == ip::ipv4::AF()) {
        sp.atype = 0x01;
        sp.bind_addr.resize(4);
        memcpy(&sp.bind_addr[0], &local_addr.v4.sin_addr, 4);
        memcpy(&sp.bind_port[0], &local_addr.v4.sin_port, 2);
      } else {
        sp.atype = 0x04;
        sp.bind_addr.resize(16);
        memcpy(&sp.bind_addr[0], &local_addr.v6.sin6_addr, 16);
        memcpy(&sp.bind_port[0], &local_addr.v6.sin6_port, 2);
      }
      co_await sp.write_to(con);
      ctx.reg(proxy(std::move(con), std::move(con2)));
    } else {
      sp.rep = 0x01;
      co_await sp.write_to(con);
    }
  }
}

awaitable<void> server(ip::ipv4 ip) {
  auto acceptor = ip::tcp::acceptor<ip::ipv4>(ip);
  while (true) {
    auto x = co_await acceptor.async_accept();
    if (x) {
      timeval timeout;
      timeout.tv_sec = 3;
      timeout.tv_usec = 0;
      if (x.setopt(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0 ||
          x.setopt(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0) {
        perror("setsocketopt error!");
      }
      ctx.reg(socks_server(std::move(x)));
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: simple socks proxy server\n";
    cerr << " <listen_address> <listen_port>\n";
    cerr << "Version: 1.0\n";
    return 1;
  }
  signal(SIGPIPE, SIG_IGN);
  ctx.reg(server(ip::ipv4::from_pret(argv[1], *tools::to_int(argv[2]))));
  ctx.run();
}