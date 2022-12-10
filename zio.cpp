#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <exception>
#include <memory>
#include <queue>
#include <thread>
#include "cardinal.hpp"
template <size_t n>
ostream& operator<<(ostream& os, const array<char, n>& a) {
  for (size_t i = 0; i < n; i++) {
    if (!a[i])
      break;
    os << a[i];
  }
  return os;
}
namespace zio {
template <class T>
T server_addr(int fd) {
  T addr;
  unsigned int len = addr.length();
  getsockname(fd, (sockaddr*)&addr, &len);
  return addr;
}
template <class T>
T client_addr(int fd) {
  T addr;
  unsigned int len = addr.length();
  getpeername(fd, (sockaddr*)&addr, &len);
  return addr;
}
struct io_context;
template <class T>
concept socket_address = requires(T a, const T ca) {
                           a.from_pret;
                           a.length();
                           ca.to_pret();
                         };
template <socket_address T>
ostream& operator<<(ostream& os, const T& addr) {
  return os << addr.to_pret();
}
struct no_copy {
  no_copy() = default;
  no_copy(no_copy&&) = default;
  no_copy(no_copy&) = delete;
  no_copy& operator=(no_copy&) = delete;
  no_copy& operator=(no_copy&&) = default;
};
struct io_op : public no_copy {
  io_op() {}
  virtual ~io_op() {}
  virtual bool operator()(io_context&) = 0;
};
struct io_context {
  struct _fd_set {
    fd_set fdset;
    int fdmax;
    array<queue<shared_ptr<io_op>>, FD_SETSIZE> funcs;
    _fd_set() {
      FD_ZERO(&fdset);
      fdmax = 0;
    }
    void set(int fd){
      FD_SET(fd,&fdset);
    }
    void add(int fd, shared_ptr<io_op> func) {
      cmax(fdmax, fd);
      FD_SET(fd, &fdset);
      funcs[fd].emplace(move(func));
    }
    void del(int fd) {
      FD_CLR(fd, &fdset);
      while (funcs[fd].size())
        funcs[fd].pop();
    }
    size_t regsize(){
      size_t s=0;
      for (auto i:funcs)
        s+=i.size();
      return s;
    }
    int get(int fd) { return FD_ISSET(fd, &fdset); }
    void scan(io_context& ctx, const fd_set& s) {
      for (int i = 0; i <= fdmax; i++)
        if (FD_ISSET(i, &s)) {
          if (funcs[i].size()) {
            auto& q = funcs[i];
            if (q.front()->operator()(ctx))
              if (q.size())
                q.pop();
            if (q.empty())
              FD_CLR(i, &fdset);
          }
        }
    }
  };
  _fd_set read_op_set;
  _fd_set write_op_set;
  fd_set read_set;
  fd_set write_set;
  void run() {
    while (read_op_set.regsize()||write_op_set.regsize()) {
      timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      read_set = read_op_set.fdset;
      write_set = write_op_set.fdset;
      int cnt=select(max({read_op_set.fdmax, write_op_set.fdmax}) + 1, &read_set,
             &write_set, NULL, &tv);
      // this_thread::sleep_for(1s);
      #ifdef debug
      cerr<<"select:"<<cnt<<endl;
      #endif
      // if (i>=100) exit(-1);
      // for (int i=0;i<FD_SETSIZE;i++)
      //   if (FD_ISSET(i, &nread_op_set)) cout<<"R"<<i<<" ";
      // for (int i=0;i<FD_SETSIZE;i++)
      //   if (FD_ISSET(i, &nwrite_op_set)) cout<<"W"<<i<<" ";
      // cout<<endl;
      read_op_set.scan(*this, read_set);
      write_op_set.scan(*this, write_set);
    }
  }
};
struct io_read : public io_op {
  int fd;
  char* data;
  function<void(int)> completion_handler;
  int n;
  io_read(int fd, char* data, int n, function<void(int)> completion_handler)
      : fd(fd),
        data(data),
        completion_handler(move(completion_handler)),
        n(n) {}
  virtual ~io_read() {}
  virtual bool operator()(io_context& ctx) {
    int ccnt = read(fd, data, n);
    if (ccnt < 0 && ccnt != O_NONBLOCK) {
      completion_handler(ccnt);
      return true;
    }
    completion_handler(ccnt);
    return true;
  }
};
struct io_write : public io_op {
  int fd;
  unique_ptr<char[]> data;
  function<void(int, int)> completion_handler;
  int n, cnt{0};
  io_write(int fd,
           unique_ptr<char[]> data,
           int n,
           function<void(int, int)> completion_handler)
      : fd(fd),
        data(move(data)),
        completion_handler(move(completion_handler)),
        n(n) {}
  virtual ~io_write() {}
  virtual bool operator()(io_context& ctx) {
    int ccnt = write(fd, &data[0], n - cnt);
    if (ccnt < 0) {
      completion_handler(cnt, ccnt);
      return true;
    }
    if (cnt == 0) {
      completion_handler(cnt, 0);
      return true;
    }
    cnt += ccnt;
    if (cnt == n) {
      completion_handler(cnt, 0);
      return true;
    }
    return false;
  }
};
struct connection {
  io_context& ctx;
  int fd{-1};
  bool status{0};
  void close() {
    if (fd!=-1){
      ctx.read_op_set.del(fd);
      ctx.write_op_set.del(fd);
      ::close(fd);
      fd=-1;
    }
  }
  operator bool(){return fd!=-1&&status==0;}
  void async_write(char* c,
                   int n,
                   function<void(int, int)> completion_handler) {
    char* cc = new char[n];
    memcpy(cc, c, n);
    ctx.write_op_set.add(fd, make_shared<io_write>(fd, unique_ptr<char[]>(cc),
                                                   n, completion_handler));
  }
  void async_read(char* c, int n, function<void(int)> completion_handler) {
    ctx.read_op_set.add(fd, make_shared<io_read>(fd, c, n, completion_handler));
  }
};
template <class T>
struct io_accept : public io_op {
  function<bool(connection)> completion_handler;
  int fd;
  io_accept(int fd, function<bool(connection)> completion_handler)
      : fd(fd), completion_handler(move(completion_handler)) {}
  virtual ~io_accept() {}
  virtual bool operator()(io_context& ctx) {
    int sockfd = accept(fd, NULL, NULL);
    if (sockfd >= 0) {
      return completion_handler(connection{ctx, sockfd});
    }
    return false;
  }
};

template <class T>
struct io_connect : public io_op {
  function<void(connection)> completion_handler;
  int fd;
  io_connect(int fd, function<void(connection)> completion_handler)
      : fd(fd), completion_handler(move(completion_handler)) {}
  virtual ~io_connect() {}
  virtual bool operator()(io_context& ctx) {
    cerr<<2;
    int error=0;
    if (FD_ISSET(fd, &ctx.read_set) || FD_ISSET(fd, &ctx.write_set)){
      socklen_t len=sizeof(error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)<0)
        return false;
    }else error=-1;
    cerr<<error<<endl;
    connection c{ctx,fd,(bool)error};
    completion_handler(move(c));
    if (error&&c.fd!=-1) c.close();
    return true;
  }
};

struct none_callback {
  template <class... Args>
  void operator()(Args... args) {}
};
template <class T, class proto>
struct acceptor {
  io_context& ctx;
  int fd;
  ~acceptor() {
    ctx.read_op_set.del(fd);
    ctx.write_op_set.del(fd);
    close(fd);
  }
  acceptor(io_context& ctx, const T& addr) : ctx(ctx) {
    fd = socket(T::AF, proto::SOCK, proto::PROTO);
    int val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);
    if (::bind(fd, (const sockaddr*)&addr, addr.length())<0&&errno!=EINPROGRESS)
      throw exception();
    ::listen(fd, 16);
  }
  void async_accept(function<bool(connection)> func) {
    ctx.read_op_set.add(fd, make_shared<io_accept<T>>(fd, move(func)));
  }
};


template <class T, class proto>
struct connector {
  io_context& ctx;
  int fd;
  ~connector() {
    cerr<<1;
    ctx.read_op_set.del(fd);
    ctx.write_op_set.del(fd);
    close(fd);
  }
  connector(io_context& ctx) : ctx(ctx) {
    fd = socket(T::AF, proto::SOCK, proto::PROTO);
    int val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);
  }
  void async_connect(const T& addr,function<void(connection)> func) {
    int n;
    if ((n=::connect(fd, (sockaddr *)&addr, addr.length()))<0&&errno!=EINPROGRESS)
      throw exception();
    if (n==0)
      func(connection{ctx,fd,0});
    else ctx.read_op_set.add(fd, make_shared<io_connect<T>>(fd, move(func)));
  }
};
}  // namespace zio
namespace zio::ip {
struct ipv4 : public sockaddr_in {
  static constexpr auto AF = AF_INET;
  static constexpr size_t length() { return sizeof(ipv4); }
  static ipv4 from_pret(const char* c, u16 port) {
    ipv4 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF;
    inet_pton(AF, c, &addr.sin_addr);
    addr.sin_port = htons(port);
    return addr;
  }
  string to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin_addr, &str[0], INET_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin_port));
    return a;
  }
};
struct ipv6 : public sockaddr_in6 {
  static constexpr auto AF = AF_INET6;
  static constexpr size_t length() { return sizeof(ipv6); }
  static ipv6 from_pret(const char* c, u16 port) {
    ipv6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF;
    inet_pton(AF, c, &addr.sin6_addr);
    addr.sin6_port = htons(port);
    return addr;
  }
  string to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin6_addr, &str[0], INET6_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin6_port));
    return a;
  }
};
struct tcp {
  static constexpr auto SOCK = SOCK_STREAM;
  static constexpr auto PROTO = IPPROTO_TCP;
};
struct udp {
  static constexpr auto SOCK = SOCK_DGRAM;
  static constexpr auto PROTO = IPPROTO_UDP;
};

}  // namespace zio::ip
using namespace zio;

namespace echo_server {
  struct session {
    connection c;
    char buffer[1024];
    ~session() { c.close(); }
    void recv(int n) {
      if (n > 0) {
        c.async_write(buffer, n, [this](int n, int ret) { send(n, ret); });
      } else
        delete this;
    }
    void send(int n, int ret) {
      if (!ret) {
        c.async_read(buffer, 1024, [this](int n) { recv(n); });
      } else
        delete this;
    }
  };
  template <class T1,class T2>
  struct server{
    acceptor<T1, T2> a;
    server(io_context &ctx,T1 ip):a(ctx,ip){
      a.async_accept([i = 0](connection c) mutable {
        auto s = new session{c};
        c.async_read(s->buffer, 1024, [=](int n) mutable { s->recv(n); });
        return false;
      });
    }
  };
}

namespace echo_client {
  struct session {
    connection c;
    char buffer[1024];
    ~session() { c.close(); }
    void recv(int n) {
      if (n > 0) {
        c.async_write(buffer, n, [this](int n, int ret) { send(n, ret); });
      } else
        delete this;
    }
    void send(int n, int ret) {
      if (!ret) {
        c.async_read(buffer, 1024, [this](int n) { recv(n); });
      } else
        delete this;
    }
  };
  template <class T1,class T2>
  struct client{
    connector<T1, T2> a;
    client(io_context &ctx,T1 ip):a(ctx){
      a.async_connect(ip,[i = 0](connection c) mutable {
        cerr<<1;
        if (c){
          cerr<<client_addr<T1>(c.fd)<<endl;
          auto s = new session{c};
          c.async_read(s->buffer, 1024, [=](int n) mutable { s->recv(n); });
        }
      });
    }
  };
}

int main() {
  auto ip = ip::ipv4::from_pret("127.0.0.1", 5002);
  cout << ip << endl;
  io_context ctx;
  // echo_server::server<ip::ipv4, ip::tcp> a(ctx, ip);
  echo_server::server<ip::ipv4, ip::tcp> a(ctx, ip);
  ctx.run();
}
