#include "uringtest.h"
#include <liburing.h>
#include <unistd.h>
namespace zio {
void await::wake_others() {
  for (auto wake_promise : wake_queue)
    if (--wake_promise->wait_cnt == 0) {
      ctx->work_queue.push_back(wake_promise);
    }
  wake_queue.clear();
}
coroutine_handle<promise_base> promise_base::handle() {
  return coroutine_handle<promise_base>::from_promise(*this);
}
void awaitable<void>::get_return() {
  if (!p)
    return;
  if (p->done) {
    handle().destroy();
    p = nullptr;
  }
  return;
}

suspend_always promise<void>::initial_suspend() {
  return {};
}
suspend_always promise<void>::final_suspend() noexcept {
  return {};
}
void promise<void>::unhandled_exception() {
  terminate();
}

awaitable<void> promise<void>::get_return_object() {
  return {this};
}
void promise<void>::return_void() {
  done = true;
}
suspend_always promise<void>::yield_void() {
  return {};
}

io_context::io_context() {
  io_uring_queue_init(128, &ring, 0);
}
void io_context::reg(promise_base* p) {
  p->from_ctx=true;
  work_queue.push_back(p);
}
void io_context::run() {
  int uring_submit_cnt=0;
  while (true) {
    bool is_advanced=false;
    while (work_queue.size()) {
      is_advanced=true;
      work_queue.front()->handle()();
      work_queue.front()->wake_others();
      if (work_queue.front()->done&&work_queue.front()->from_ctx) {
        work_queue.front()->~promise_base();
#ifdef DEBUG
        cerr << "Destory!" << endl;
#endif
      }else{
#ifdef DEBUG
        cerr << "No!" << work_queue.front()->from_ctx << endl;
#endif  
      }
      work_queue.pop_front();
    }
    uring_submit_cnt+=io_uring_submit(&ring);
    io_uring_cqe* cqe;
    while (!io_uring_peek_cqe(&ring, &cqe)) {
      is_advanced=true;
      io_await* c = (io_await*)io_uring_cqe_get_data(cqe);
      c->complete(cqe->res);
      c->wake_others();
      c->done=true;
      io_uring_cqe_seen(&ring, cqe);
      uring_submit_cnt--;
    }
    while (sleep_queue.size() && time(NULL) >= sleep_queue.begin()->first) {
      is_advanced=true;
      sleep_queue.begin()->second->wake_others();
      sleep_queue.erase(sleep_queue.begin());
    }
    if (!(is_advanced||sleep_queue.size()||work_queue.size()||uring_submit_cnt)){
#ifdef DEBUG
      cerr<<"done!"<<endl;
#endif
      return ;
    }
  }
}

timeout_await::timeout_await(time_t t) : t(t) {}

void timeout_await::get_return() {
  return;
}

void await::set_context(io_context& _ctx) {
  ctx = &_ctx;
}
void io_await::complete(int _ret) {
  ret = _ret;
  done = true;
}
int io_await::get_return() {
  return ret;
}

io_await_write::io_await_write(int fd,
                               const char* c,
                               unsigned int n,
                               u64 offset)
    : fd(fd), c(c), n(n), offset(offset) {}
void io_await_write::prepare(io_uring_sqe* sqe) {
  io_uring_prep_write(sqe, fd, c, n, offset);
}

void connection::close() {
  if (fd >= 0)
    ::close(fd);
}

io_await_write connection::async_write(const char* c, int n) {
  return zio::async_write(fd, c, n);
}

io_await_read connection::async_read(char* c, int n) {
  return zio::async_read(fd, c, n);
}
io_await_read::io_await_read(int fd, char* c, unsigned int n, u64 offset)
    : fd(fd), c(c), n(n), offset(offset) {}
void io_await_read::prepare(io_uring_sqe* sqe) {
  io_uring_prep_read(sqe, fd, c, n, offset);
}

io_await_accept::io_await_accept(int fd) : fd(fd) {}
void io_await_accept::prepare(io_uring_sqe* sqe) {
  io_uring_prep_accept(sqe, fd, NULL, NULL, 0);
}
connection io_await_accept::get_return() {
  return {ret};
}

io_await_connect::io_await_connect(int fd, sockaddr* addr, socklen_t len)
    : fd(fd), addr(addr), len(len) {}
void io_await_connect::prepare(io_uring_sqe* sqe) {
  io_uring_prep_connect(sqe, fd, addr, len);
}
connection io_await_connect::get_return() {
  return {fd, ret};
}

io_await_read async_read(int fd, char* c, unsigned int n, u64 offset) {
  return io_await_read{fd, c, n, offset};
}
io_await_write async_write(int fd, const char* c, unsigned int n, u64 offset) {
  return io_await_write{fd, c, n, offset};
}
}  // namespace zio
