#pragma once
#include <bits/chrono.h>
#include <bits/utility.h>
#include <sys/socket.h>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include "cardinal.hpp"
#include "zio_types.hpp"
#include "liburing.h"
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
  vector<promise_base*> wake_queue;
  bool done{false},from_ctx{false};
  void set_context(io_context& _ctx);
  void wake_others();
};
struct promise_base : public await {
  int wait_cnt{0};
  coroutine_handle<promise_base> handle();
};
struct io_await : public await {
  int ret;
  operator bool() const { return !done; }
  virtual void prepare(io_uring_sqe* sqe) = 0;
  void complete(int _ret);
  template <class U>
  void await_suspend(coroutine_handle<U> h);
  int get_return();
};
struct io_context {
  multimap<time_t,timeout_await*> sleep_queue;
  deque<promise_base*> work_queue;
  io_uring ring;
  io_context();
  void reg(promise_base* p);
  template <class T>
  void reg(awaitable<T>& a) {
    a.get_promise()->from_ctx=true;
    a.get_promise()->ctx = this;
    work_queue.push_back(a.get_promise());
  }
  template <class T>
  void reg(awaitable<T>&& a) {
    a.get_promise()->ctx = this;
    work_queue.push_back(a.get_promise());
    a.detached();
  }
  void run();
};
template <class T>
struct promise : promise_base {
  auto initial_suspend() { return suspend_always{}; }
  auto final_suspend() noexcept { return suspend_always{}; }
  void unhandled_exception() { terminate(); }
  template <waitable awaitableT>
  auto await_transform(awaitableT&& a) {
    a.set_context(*ctx);
    struct awaitble {
      awaitableT& a;
      constexpr bool await_ready() { return !a; }
      auto await_suspend(coroutine_handle<promise> h) {
        return a.await_suspend(h);
      }
      auto await_resume() { return a.get_return(); }
    };
    return awaitble{a};
  }
  awaitable<T> get_return_object() { return {this}; }
  template <class U>
  void return_value(U&& value) {
    value_ = ::forward<U>(value);
    done = true;
  }
  template <class U>
  auto yield_value(U&& value) {
    value_ = ::forward<U>(value);
    return suspend_always{};
  }
  T value_;
};
template <class U>
void io_await::await_suspend(coroutine_handle<U> h) {
#ifdef DEBUG
  cerr << "con" << endl;
#endif
  auto sqe = io_uring_get_sqe(&ctx->ring);
  if (!sqe) {
#ifdef DEBUG
    cerr << "full" << endl;
#endif
    io_uring_submit(&ctx->ring);
    sqe = io_uring_get_sqe(&ctx->ring);
  }
  prepare(sqe);
  io_uring_sqe_set_data(sqe, this);
  wake_queue.push_back(&h.promise());
  h.promise().wait_cnt++;
}

template <>
struct promise<void> : promise_base {
  suspend_always initial_suspend();
  suspend_always final_suspend() noexcept;
  void unhandled_exception();
  template <waitable awaitableT>
  auto await_transform(awaitableT&& a) {
    a.set_context(*ctx);
    struct awaitble {
      awaitableT& a;
      constexpr bool await_ready() { return !a; }
      auto await_suspend(coroutine_handle<promise> h) {
        return a.await_suspend(h);
      }
      auto await_resume() { return a.get_return(); }
    };
    return awaitble{a};
  }
  awaitable<void> get_return_object();
  void return_void();
  suspend_always yield_void();
};
template <class ...Args>
struct any_await{
  tuple<Args...> t;
  operator bool()const{
    return std::apply([](auto&& ...args){
      return (((bool)args)||...);
    },t);
  }
  void set_context(io_context& ctx){
    return std::apply([&](auto&& ...args){
      return (args.set_context(ctx),...);
    },t);
  }
  template<class U>
  void await_suspend(coroutine_handle<U> h) {
    int cnt=0;
    std::apply([&](auto&& ...args){
      ((args?args.await_suspend(h):void()),...);
    },t);
    h.promise().wait_cnt=1;
  }
  void get_return(){
    
  }
};
template <class ...Args>
any_await<Args&&...> wait_any(Args&& ...args){
  return any_await<Args&&...>({forward<Args>(args)...});
}
template <class ...Args>
struct all_await{
  tuple<Args...> t;
  operator bool()const{
    return std::apply([](auto&& ...args){
      return (((bool)args)||...);
    },t);
  }
  void set_context(io_context& ctx){
    return std::apply([&](auto&& ...args){
      return (args.set_context(ctx),...);
    },t);
  }
  template<class U>
  void await_suspend(coroutine_handle<U> h) {
    std::apply([&](auto&& ...args){
      ((args?args.await_suspend(h):void()),...);
    },t);
  }
  void get_return(){
    
  }
};
template <class ...Args>
all_await<Args&&...> wait_all(Args&& ...args){
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
  // false for done
  operator bool() const { return p && !p->done; }
  template <class U>
  void await_suspend(coroutine_handle<U> h) const {
    p->ctx->work_queue.push_back(p);
    p->wake_queue.push_back(&h.promise());
    h.promise().wait_cnt++;
  }
  coroutine_handle<promise<T>> handle() {
    return coroutine_handle<promise<T>>::from_promise(*p);
  }
  void detached() { p = nullptr; }
  ~awaitable_base() {
    if (p)
      handle().destroy();
  }
};
template <class T>
struct awaitable : public awaitable_base<T> {
  using awaitable_base<T>::p;
  using awaitable_base<T>::get_promise;
  using awaitable_base<T>::handle;
  using awaitable_base<T>::await_suspend;
  // true for not done
  // false for done
  optional<T> get_return() {
    if (!p)
      return {};
    auto ret = ::move(p->value_);
    if (p->done) {
      handle().destroy();
      p = nullptr;
    }
    return ret;
  }
};
template <>
struct awaitable<void> : public awaitable_base<void> {
  void get_return();
};

struct timeout_await : public await{
  time_t t;
  timeout_await(time_t t);
  bool operator<(const timeout_await& ta) const { return t<ta.t;}
  operator bool() const { return time(NULL)<=t; }
  template <class U>
  void await_suspend(coroutine_handle<U> h){
    wake_queue.push_back(&h.promise());
    h.promise().wait_cnt++;
    ctx->sleep_queue.insert({t,this});
  }
  void get_return();
};

struct io_await_read : public io_await {
  int fd;
  char* c;
  size_t n;
  u64 offset;
  io_await_read(int fd, char* c, size_t n, u64 offset);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_write : public io_await {
  int fd;
  const char* c;
  size_t n;
  u64 offset;
  io_await_write(int fd, const char* c, size_t n, u64 offset);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_recv : public io_await {
  int fd;
  char* c;
  size_t n;
  int flags;
  io_await_recv(int fd, char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_send : public io_await {
  int fd;
  const char* c;
  size_t n;
  int flags;
  io_await_send(int fd, const char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct io_await_send_zc : public io_await {
  int fd;
  const char* c;
  size_t n;
  int flags;
  io_await_send_zc(int fd, const char* c, size_t n, int flags);
  virtual void prepare(io_uring_sqe* sqe);
};

struct connection {
  int fd{-1};
  int status{0};
  void close();
  operator bool() { return fd != -1 && status == 0; }
  io_await_write async_write(const char* c, size_t n);
  io_await_read async_read(char* c, size_t n);
  io_await_recv async_recv(char *c,size_t n,int flags=0);
  io_await_send async_send(const char *c,size_t n,int flags=0);
  io_await_send_zc async_send_zc(const char *c,size_t n,int flags=0);
};

struct io_await_accept : public io_await {
  int fd;
  io_await_accept(int fd);
  virtual void prepare(io_uring_sqe* sqe);
  connection get_return();
};

template <types::Address Address, types::Protocol proto>
struct acceptor {
  int fd;
  ~acceptor() { close(fd); }
  acceptor(const Address& addr) {
    fd = socket(Address::AF, proto::SOCK, proto::PROTO);
    if (::bind(fd, (const sockaddr*)&addr, addr.length()) < 0)
      throw exception();
    ::listen(fd, 16);
  }
  io_await_accept async_accept() { return {fd}; }
};

struct io_await_connect : public io_await {
  int fd;
  sockaddr* addr;
  socklen_t len;
  io_await_connect(int fd, sockaddr* addr, socklen_t len);
  virtual void prepare(io_uring_sqe* sqe);
  connection get_return();
};

template <types::Address Address, types::Protocol proto>
io_await_connect async_connect(Address&& addr) {
  int fd = socket(decay_t<Address>::AF, proto::SOCK, proto::PROTO);
  return {fd, (sockaddr*)&addr, addr.length()};
}

template <types::Address Address, types::Protocol proto>
connection async_open(Address&& addr) {
  int fd = socket(decay_t<Address>::AF, proto::SOCK, proto::PROTO);
  int status=::bind(fd, (const sockaddr*)&addr, addr.length());
  if (status<0) status=errno;
  return {fd,status};
}

template<class Rep,class Period>
timeout_await async_timeout(const std::chrono::duration<Rep, Period>& sleep_duration){
  return {time(NULL) + std::chrono::duration_cast<std::chrono::duration<time_t>>(sleep_duration).count()};
}
io_await_read async_read(int fd, char* c, size_t n, u64 offset = 0);
io_await_write async_write(int fd, const char* c, size_t n, u64 offset = 0);
io_await_recv async_recv(int fd,char *c,size_t n,int flags);
io_await_send async_send(int fd,const char *c,size_t n,int flags);
io_await_send_zc async_send_zc(int fd,const char *c,size_t n,int flags);
}  // namespace zio