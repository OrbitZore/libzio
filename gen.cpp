#include <coroutine>
#include <iostream>
#include <optional>
#include <tuple>

using namespace std;

template <class T>
struct promise;

template <class T>
struct generator;

namespace std {
template <class T, class... Args>
struct coroutine_traits<generator<T>, Args...> {
  using promise_type = promise<T>;
};
}

template <class T>
struct promise {
  promise():done(false){}
  auto initial_suspend() { return suspend_always{}; }
  auto final_suspend() noexcept { return suspend_always{}; }
  void unhandled_exception() { terminate(); }

  generator<T> get_return_object() { return {this}; }
  void return_void() {
    done = true;
  }
  template <class U>
  auto yield_value(U&& value) {
    value_ = move(forward<U>(value));
    return suspend_always{};
  }
  bool done;
  T value_;
};

template <class T>
struct generator {
  promise<T>* f;
  ~generator() { 
    if (f)
      handle().destroy(); 
  }
  coroutine_handle<promise<T>> handle() {
    return coroutine_handle<promise<T>>::from_promise(*f);
  }
  operator bool() { return f; }
  T* operator()() {
    if (!f) return nullptr;
    handle()();
    if (f->done){
      handle().destroy();
      f = nullptr;
      return nullptr;
    }
    return &f->value_;
  }
};

generator<int> fibonacci(int a = 1) {
  int i = 0, j = 1;
  while (j<=100) {
    co_yield j;
    tie(i, j) = make_pair(j, i + j);
  }
}

int main(int argc, char** argv) {
  auto x = fibonacci();
  while (auto y=x()) {
    cout << *y << endl;
  }
  return 0;
}
