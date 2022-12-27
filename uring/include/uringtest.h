#pragma once
#include <bits/chrono.h>
#include <bits/types/struct_iovec.h>
#include <bits/utility.h>
#include <sys/socket.h>
#include <charconv>
#include <coroutine>
#include <cstddef>
#include <cstring>
#include <functional>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include "cardinal.hpp"
#include "liburing.h"
#include "zio_types.hpp"
namespace zio::tools {
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
inline string_view strip(string_view sv){
  while (sv.size()&&isspace(sv.front())) sv.remove_prefix(1);
  while (sv.size()&&isspace(sv.back())) sv.remove_suffix(1);
  return sv;
}
inline optional<int> to_int(string_view sv){
  int x=0;
  for (auto c:sv)
    x=c-'0'+x*10;
  return x;
}
inline string format_now(string format){
	time_t t=time(NULL);
	return (stringstream()<<put_time(gmtime(&t), format.c_str())).str();
}
inline unsigned int asciihex_to_uint(char c){
	return isdigit(c)?c-'0':toupper(c)-'A'+10;
}
}  // namespace zio::tools
namespace zio::debug {
void perrno(int errno_);
}
namespace zio {
template <class T>
struct awaitable;
template <>
struct awaitable<void>;
template <class T>
struct promise;
template <>
struct promise<void>;
struct timeout_await;
}  // namespace zio

template <class T, class... Args>
struct coroutine_traits<zio::awaitable<T>, Args...> {
  using promise_type = zio::promise<T>;
};
namespace zio {
template <class T>
concept waitable = requires(T a, const T ca) {
                     (bool)ca;
                     a.get_return();
                   };
struct io_context;
template <class T>
struct promise;
struct promise_base;
struct await {
  io_context* ctx{NULL};
  promise_base* waiter{NULL};
  bool done{false}, from_ctx{false};
  void set_context(io_context& _ctx);
  void wake_others();
};
struct promise_base : public await {
  virtual ~promise_base();
  int wait_cnt{0};
  coroutine_handle<promise_base> handle();
};
struct io_await : public await {
  int ret;
  bool submit{false};
  operator bool() const { return !done; }
  virtual void prepare(io_uring_sqe* sqe) = 0;
  void complete(int _ret);
  template <class U>
  void await_suspend(coroutine_handle<U> h);
  int get_return();
};
struct io_context {
  multimap<time_t, timeout_await*> sleep_queue;
  deque<promise_base*> work_queue;
  io_uring ring;
  int uring_submit_cnt = 0;
  io_context();
  template <class T>
  void reg(awaitable<T>& a) {
#ifdef DEBUG
    cerr << "Create(ref):" << a.get_promise() << endl;
#endif
    a.get_promise()->ctx = this;
    work_queue.push_back(a.get_promise());
  }
  template <class T>
  void reg(awaitable<T>&& a) {
#ifdef DEBUG
    cerr << "Create(move):" << a.get_promise() << endl;
#endif
    a.get_promise()->from_ctx = true;
    a.get_promise()->ctx = this;
    work_queue.push_back(a.get_promise());
    a.detached();
  }
  inline io_uring_sqe* get_sqe() {
    auto sqe = io_uring_get_sqe(&ring);
    while (!sqe) {
#ifdef DEBUG
      cerr << "full" << endl;
#endif
      uring_submit_cnt += io_uring_submit(&ring);
      sqe = io_uring_get_sqe(&ring);
    }
    return sqe;
  }
  inline void reg(io_await&& ioa) {
    auto sqe = get_sqe();
    ioa.prepare(sqe);
    io_uring_sqe_set_data(sqe, NULL);
  }
  void run();
};
template <class T>
struct promise : promise_base {
  auto initial_suspend() { return suspend_always{}; }
  auto final_suspend() noexcept { return suspend_always{}; }
  void unhandled_exception() { terminate(); }
  template <waitable awaitableT>
  struct awaitble {
    awaitableT a;
    constexpr bool await_ready() { return !a; }
    auto await_suspend(coroutine_handle<promise> h) {
      return a.await_suspend(h);
    }
    auto await_resume()
      requires types::IS_AWAITABLE<awaitableT>
    {
      return *a.get_return();
    }
    auto await_resume()
      requires (!types::IS_AWAITABLE<awaitableT>)
    {
      return a.get_return();
    }
  };
  template <waitable awaitableT>
  auto await_transform(awaitableT&& a) {
    a.set_context(*ctx);
    return awaitble<awaitableT&&>{std::forward<awaitableT>(a)};
  }
  awaitable<T> get_return_object() { return {this}; }
  void return_value(T& value) {
#ifdef DEBUG
    // cerr << "return" << value << endl;
#endif
    value_ = value;
    done = true;
  }
  void return_value(T value) {
#ifdef DEBUG
    // cerr << "return" << value << endl;
#endif
    value_ = ::move(value);
    done = true;
  }
  auto yield_value(T& value) {
    value_ = value;
    return suspend_always{};
  }
  auto yield_value(T value) {
    value_ = ::move(value);
    return suspend_always{};
  }
  optional<T> value_;
};
template <class U>
inline void io_await::await_suspend(coroutine_handle<U> h) {
#ifdef DEBUG
  cerr << "con" << endl;
#endif
  if (!submit){
    auto sqe = ctx->get_sqe();
    prepare(sqe);
    io_uring_sqe_set_data(sqe, this);
    submit=true;
  }
  waiter = &h.promise();
  h.promise().wait_cnt++;
}

template <>
struct promise<void> : promise_base {
  suspend_always initial_suspend();
  suspend_always final_suspend() noexcept;
  void unhandled_exception();
  
  template <waitable awaitableT>
  struct awaitble {
    awaitableT a;
    constexpr bool await_ready() { return !a; }
    auto await_suspend(coroutine_handle<promise> h) {
      return a.await_suspend(h);
    }
    auto await_resume()
      requires types::IS_AWAITABLE<awaitableT>
    {
      return *a.get_return();
    }
    auto await_resume()
      requires (!types::IS_AWAITABLE<awaitableT>)
    {
      return a.get_return();
    }
  };
  template <waitable awaitableT>
  auto await_transform(awaitableT&& a) {
    a.set_context(*ctx);
    return awaitble<awaitableT&&>{std::forward<awaitableT>(a)};
  }
  awaitable<void> get_return_object();
  void return_void();
  suspend_always yield_void();
};
template <class... Args>
struct any_await {
  tuple<Args...> t;
  operator bool() const {
    return std::apply([](auto&&... args) { return (((bool)args) || ...); }, t);
  }
  void set_context(io_context& ctx) {
    return std::apply(
        [&](auto&&... args) { return (args.set_context(ctx), ...); }, t);
  }
  template <class U>
  void await_suspend(coroutine_handle<U> h) {
    h.promise().wait_cnt = 0;
    std::apply(
        [&](auto&&... args) { ((args ? args.await_suspend(h) : void()), ...); },
        t);
    h.promise().wait_cnt = min(h.promise().wait_cnt, 1);
  }
  void get_return() {}
};
template <class... Args>
any_await<Args&&...> wait_any(Args&&... args) {
  return any_await<Args&&...>(tuple<Args&&...>{forward<Args>(args)...});
}
template <class... Args>
struct all_await {
  tuple<Args...> t;
  operator bool() const {
    return std::apply([](auto&&... args) { return (((bool)args) || ...); }, t);
  }
  void set_context(io_context& ctx) {
    return std::apply(
        [&](auto&&... args) { return (args.set_context(ctx), ...); }, t);
  }
  template <class U>
  void await_suspend(coroutine_handle<U> h) {
    h.promise().wait_cnt = 0;
    std::apply(
        [&](auto&&... args) { ((args ? args.await_suspend(h) : void()), ...); },
        t);
  }
  void get_return() {}
};
template <class... Args>
all_await<Args&&...> wait_all(Args&&... args) {
  return all_await<Args&&...>({forward<Args>(args)...});
}
template <class T>
struct awaitable_base {
  promise<T>* p;
  promise<T>* get_promise() { return p; }
  void set_context(io_context& ctx) {
    if (p)
      p->ctx = &ctx;
  }
  // true for not done
  // false for done or has value
  operator bool() const {
    if constexpr (!is_same_v<T, void>) {
      return p && !p->done && !p->value_;
    } else {
      return p && !p->done;
    }
  }
  template <class U>
  void await_suspend(coroutine_handle<U> h) const {
    p->ctx->work_queue.push_back(p);
    p->waiter = &h.promise();
    h.promise().wait_cnt++;
  }
  coroutine_handle<promise<T>> handle() {
    return coroutine_handle<promise<T>>::from_promise(*p);
  }
  void detached() { p = nullptr; }
  ~awaitable_base() {
    if (p) {
#ifdef DEBUG
      cerr << "delete:" << p << endl;
#endif
      handle().destroy();
    }
  }
};
template <class T>
struct awaitable : public awaitable_base<T> {
  using awaitable_base<T>::p;
  using awaitable_base<T>::get_promise;
  using awaitable_base<T>::handle;
  using awaitable_base<T>::await_suspend;
  void is_awaitble();
  // true for not done
  // false for done
  optional<T> get_return() {
    if (!p)
      return {};
    auto ret = std::move(p->value_);
    p->value_.reset();
    if (p->done) {
      handle().destroy();
      p = {};
    }
    return ret;
  }
};
template <>
struct awaitable<void> : public awaitable_base<void> {
  void get_return();
};

struct timeout_await : public await {
  time_t t;
  timeout_await(time_t t);
  bool operator<(const timeout_await& ta) const { return t < ta.t; }
  operator bool() const { return time(NULL) <= t; }
  template <class U>
  void await_suspend(coroutine_handle<U> h) {
    waiter = &h.promise();
    h.promise().wait_cnt++;
    ctx->sleep_queue.insert({t, this});
  }
  void get_return();
};

struct io_await_read : public io_await {
  io_await_read();
  int fd;
  char* c;
  size_t n;
  u64 offset;
  io_await_read(int fd, char* c, size_t n, u64 offset);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_write : public io_await {
  io_await_write();
  int fd;
  const char* c;
  size_t n;
  u64 offset;
  io_await_write(int fd, const char* c, size_t n, u64 offset);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_recv : public io_await {
  io_await_recv();
  int fd;
  char* c;
  size_t n;
  int flags;
  io_await_recv(int fd, char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_send : public io_await {
  io_await_send();
  int fd;
  const char* c;
  size_t n;
  int flags;
  io_await_send(int fd, const char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_vector : public vector<iovec> {
  using vector<iovec>::vector;
};

struct message_header : public msghdr {
  template <types::Address Address>
  void set_address(Address& addr) {
    msg_name = &addr;
    msg_namelen = addr.length();
  }
  void set_address(void* addr, int addrlen);
  void set_io_vector(io_vector& v);
};

struct io_await_recvmsg : public io_await {
  io_await_recvmsg();
  int fd;
  message_header* msg;
  int flags;
  io_await_recvmsg(int fd, message_header* msg, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_sendmsg : public io_await {
  io_await_sendmsg();
  int fd;
  message_header* msg;
  int flags;
  io_await_sendmsg(int fd, message_header* msg, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_send_zc : public io_await {
  io_await_send_zc();
  int fd;
  const char* c;
  size_t n;
  int flags;
  io_await_send_zc(int fd, const char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

template <types::Address Address, types::Protocol Protocol>
struct connection;

template <types::Address Address, types::Protocol Protocol>
struct io_await_accept : public io_await {
  io_await_accept();
  int fd;
  io_await_accept(int fd) : fd(fd) {}
  void prepare(io_uring_sqe* sqe) {
    io_uring_prep_accept(sqe, fd, NULL, NULL, 0);
  }
  connection<Address, Protocol> get_return() {
    return connection<Address, Protocol>(ret, 0);
  }
};

template <types::Address Address, types::Protocol Protocol>
struct acceptor {
  int fd;
  ~acceptor() { close(fd); }
  acceptor(const Address& addr) {
    fd = socket(addr.AF(), Protocol::SOCK, Protocol::PROTO);
    if (::bind(fd, (const sockaddr*)&addr, addr.length()) < 0)
      throw exception();
    ::listen(fd, 16);
  }
  io_await_accept<Address, Protocol> async_accept() { return {fd}; }
};

template <types::Address Address, types::Protocol Protocol>
struct io_await_connect : public io_await {
  int fd;
  sockaddr* addr;
  socklen_t len;
  io_await_connect() {}
  io_await_connect(int fd, sockaddr* addr, socklen_t len)
      : fd(fd), addr(addr), len(len) {}
  void prepare(io_uring_sqe* sqe) { io_uring_prep_connect(sqe, fd, addr, len); }
  connection<Address, Protocol> get_return() {
    return connection<Address, Protocol>(fd, ret);
  }
};

template <types::Address Address, types::Protocol Protocol>
io_await_connect<remove_cvref_t<Address>, Protocol> async_connect(
    Address& addr) {
  int fd = socket(addr.AF(), Protocol::SOCK, Protocol::PROTO);
  return {fd, (sockaddr*)&addr, addr.length()};
}

template <types::Address Address, types::Protocol Protocol>
connection<remove_cvref_t<Address>, Protocol> open(Address addr) {
  int fd = socket(addr.AF(), Protocol::SOCK, Protocol::PROTO);
  int status = ::bind(fd, (const sockaddr*)&addr, addr.length());
  if (status < 0) {
    status = errno;
    cerr << strerror(errno) << endl;
  }
  return {fd, status};
}

template <class Rep, class Period>
timeout_await async_timeout(
    const std::chrono::duration<Rep, Period>& sleep_duration) {
  return {
      time(NULL) +
      std::chrono::duration_cast<std::chrono::duration<time_t>>(sleep_duration)
          .count()};
}
io_await_read async_read(int fd, char* c, size_t n, u64 offset = 0);
io_await_write async_write(int fd, const char* c, size_t n, u64 offset = 0);
io_await_recv async_recv(int fd, char* c, size_t n, int flags);
io_await_send async_send(int fd, const char* c, size_t n, int flags);
template <types::Address T>
io_await_recvmsg async_recvmsg(int fd, message_header* msg, int flags) {
  return {fd, msg, flags};
}
template <types::Address T>
io_await_sendmsg async_sendmsg(int fd, message_header* msg, int flags) {
  return {fd, msg, flags};
}
io_await_send_zc async_send_zc(int fd, const char* c, size_t n, int flags);

template <types::Address Address, types::Protocol Protocol>
struct connection {
  int fd{-1};
  int status{0};
  explicit connection(int fd, int status) : fd(fd), status(status) {}
  connection(const connection&) = delete;
  connection(connection&& con) {
    fd = con.fd;
    status = con.status;
    con.fd = -1;
  }
  ~connection() { close(); }
  operator bool() { return fd != -1 && status == 0; }
  void close() {
    if (fd >= 0)
      ::close(fd);
  }

  Address getpeer(){
    Address address;
    unsigned int len=sizeof(address);
    getpeername(fd,(sockaddr*)&address, &len);
    address.resize(len);
    return address;
  }
  
  Address getaddr(){
    Address address;
    unsigned int len=sizeof(address);
    getsockname(fd,(sockaddr*)&address, &len);
    address.resize(len);
    return address;
  }

  int getopt(int level, int opt, void* optval, socklen_t* optlen) {
    return getsockopt(fd, level, opt, optval, optlen);
  }
  int setopt(int level, int opt, const void* optval, socklen_t optlen) {
    return setsockopt(fd, level, opt, optval, optlen);
  }

  io_await_write async_write(const char* c, size_t n) {
    return zio::async_write(fd, c, n);
  }

  io_await_read async_read(char* c, size_t n) {
    return zio::async_read(fd, c, n);
  }

  io_await_recv async_recv(char* c, size_t n, int flags = 0) {
    return zio::async_recv(fd, c, n, flags);
  }

  io_await_send async_send(const char* c, size_t n, int flags = 0) {
    return zio::async_send(fd, c, n, flags);
  }

  io_await_sendmsg async_sendmsg(message_header* msg, int flags = 0) {
    return {fd, msg, flags};
  }
  io_await_recvmsg async_recvmsg(message_header* msg, int flags = 0) {
    return {fd, msg, flags};
  }

  io_await_send_zc async_send_zc(const char* c, size_t n, int flags = 0) {
    return zio::async_send_zc(fd, c, n, flags);
  }
};

template <types::Address Address, types::Protocol Protocol>
struct buffered_connection : protected connection<Address, Protocol> {
  using connection<Address, Protocol>::async_write;
  using connection<Address, Protocol>::async_send;
  using connection<Address, Protocol>::async_sendmsg;
  using connection<Address, Protocol>::getopt;
  using connection<Address, Protocol>::setopt;
  char c[4096];
  int i{0}, n1{0};
  void close() {
    if (this->fd >= 0)
      ::close(this->fd);
  }
  template <types::MatchFunction MatchFunction>
  awaitable<int> async_read(MatchFunction& func) {
    while (true) {
      while (i < n1) {
        auto [ii, opt] = func(c + i, c + n1);
        i = ii - c;
        if (opt)
          co_return 0;
      }
      int n1 = co_await async_read(this->fd, c, 4096);
      if (n1 <= 0) {
        co_return n1;
      }
      i = 0;
    }
  }
};
}  // namespace zio