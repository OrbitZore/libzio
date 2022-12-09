#include <type_traits>
#include <utility>
#include "cardinal.hpp"
#include "liburing.h"
namespace zio {
template <class T>
struct awaitble;
template <class T>
struct promise;
}  // namespace zio

template <class T, class... Args>
struct coroutine_traits<zio::awaitble<T>, Args...> {
  using promise_type = zio::promise<T>;
};
namespace zio {
template <class T>
struct promise {
  promise() : done(false) {}
  auto initial_suspend() { return suspend_always{}; }
  auto final_suspend() noexcept { return suspend_always{}; }
  void unhandled_exception() { terminate(); }
  // template <class awaitableT>
  // auto await_transform(awaitableT&& a) {
  //   struct awaitble {
  //     awaitableT& a;
  //     bool await_ready() { return a; }
  //     void await_suspend(coroutine_handle<>) {}
  //     auto await_resume() { return a(); }
  //   };
  //   return awaitble{a};
  // }
  awaitble<T> get_return_object() { return {this}; }
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
  bool done;
  T value_;
};
template<class T>
concept waitable=requires(T a,const T ca){
  (bool)ca;
  a.get_return();
  a();
};
template <class T>
struct awaitble {
  promise<T>* f;
  ~awaitble() {
    if (f)
      handle().destroy();
  }
  coroutine_handle<promise<T>> handle() {
    return coroutine_handle<promise<T>>::from_promise(*f);
  }
  operator bool() const { return f; }
  bool await_ready() { return (*this); }
  void await_suspend(coroutine_handle<>) {}
  auto await_resume() { return (*this)(); }
  T* get_return() {
    if (f&&f->done) {
      handle().destroy();
      f = nullptr;
    }
    if (!f)
      return nullptr;
    return &f->value_;
  }
  T* operator()() {
    if (f&&f->done) {
      handle().destroy();
      f = nullptr;
    }
    if (!f)
      return nullptr;
    handle()();
    return &f->value_;
  }
};
template <waitable ...awaitables>
struct awaitbleList {
  tuple<awaitables&&...> t;
  int cnt;
  using return_type =
      decltype(apply([](auto&&... v) { return tuple(v()...); }, t));
  bool await_ready() { return (*this); }
  void await_suspend(coroutine_handle<>) {}
  auto await_resume() { return (*this)(); }
  awaitbleList(awaitables&&... args)
      : t(forward<awaitables>(args)...), cnt(0)
       {
    cnt = (((bool)args) + ...);
  }
  operator bool() const { return cnt; }
  return_type get_return() const {
    return apply([](auto&&... v) { return tuple(v.get_return()...); }, t);
  }
  return_type operator()() {
    return apply([](auto&&... v) { return tuple(v()...); }, t);
  }
};
template<waitable T1,waitable T2>
auto operator&&(T1&& a,T2&& b){
  return awaitbleList<T1,T2>(forward<T1>(a),forward<T2>(b));
}
template<waitable ...Args,waitable T2>
auto operator&&(awaitbleList<Args...>&& a,T2&& b){
  return apply([&](auto&& ...args){return awaitbleList<decltype(args)&&...,T2&&>(forward<decltype(args)>(args)...,forward<T2>(b));},a.t);
}
struct io_context {
  io_uring ring;
  io_uring_sqe* sqe;
  io_context() {
    io_uring_queue_init(128, &ring, 0);
    sqe = io_uring_get_sqe(&ring);
  }
  void run() {}
};
}  // namespace zio
using namespace zio;
awaitble<int> g1() {
  co_yield 10;
  co_return 11;
}
awaitble<int> g2() {
  co_return 100;
}
awaitble<int> g3() {
  co_yield 1000;
  co_yield 1001;
  co_return 1002;
}
awaitble<int> f() {
  auto G1 = g1();
  auto G2 = g2();
  auto G3 = g3();
  while (true) {
    auto [x,y,z] = co_await (G1&&G2&&G3);
    if (!x&&!y&&!z) break;
    if (x) cout << *x << endl;
    if (y) cout << *y << endl;
    if (z) cout << *z << endl;
  }
  co_return 2;
}
int main() {
  auto x = f();
  cout << *x() << endl;
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