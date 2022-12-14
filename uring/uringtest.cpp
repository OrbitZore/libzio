#include "uringtest.h"
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/socket.h>
#include <unistd.h>
namespace zio {
void await::wake_others() {
  if (waiter && --waiter->wait_cnt == 0) {
    ctx->work_queue.push_back(waiter);
  }
  waiter = NULL;
}
coroutine_handle<promise_base> promise_base::handle() {
  return coroutine_handle<promise_base>::from_promise(*this);
}
promise_base::~promise_base() {}
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
void io_context::run() {
  while (true) {
    bool is_advanced = false;
    while (work_queue.size()) {
      is_advanced = true;
      auto p = work_queue.front();
      work_queue.pop_front();
      p->handle()();
      p->wake_others();
      if (p->done && p->from_ctx) {
        p->handle().destroy();
#ifdef DEBUG
        cerr << "Destory:" << p << endl;
#endif
      } else {
#ifdef DEBUG
        cerr << "No!" << p->from_ctx << endl;
#endif
      }
    }
    uring_submit_cnt += io_uring_submit(&ring);
    io_uring_cqe* cqe;
    __kernel_timespec t{};
    t.tv_nsec = 1e-2 / 1e-9;
    while (!io_uring_wait_cqe_timeout(&ring, &cqe, &t)) {
      is_advanced = true;
      if (io_await* c = (io_await*)io_uring_cqe_get_data(cqe)) {
        c->complete(cqe->res);
        c->wake_others();
        c->done = true;
      }
      io_uring_cqe_seen(&ring, cqe);
      uring_submit_cnt--;
    }
    while (sleep_queue.size() && time(NULL) >= sleep_queue.begin()->first) {
      is_advanced = true;
      sleep_queue.begin()->second->wake_others();
      sleep_queue.erase(sleep_queue.begin());
    }
    if (!(is_advanced || sleep_queue.size() || work_queue.size() ||
          uring_submit_cnt)) {
#ifdef DEBUG
      cerr << "done!" << endl;
#endif
      return;
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

io_await_write::io_await_write(int fd, const char* c, size_t n, u64 offset)
    : fd(fd), c(c), n(n), offset(offset) {}
void io_await_write::prepare(io_uring_sqe* sqe) {
  io_uring_prep_write(sqe, fd, c, n, offset);
}

void message_header::set_address(void* addr, int addrlen) {
  msg_name = addr;
  msg_namelen = addrlen;
}

void message_header::set_io_vector(io_vector& v) {
  msg_iov = &v[0];
  msg_iovlen = v.size();
}

io_await_read::io_await_read(int fd, char* c, size_t n, u64 offset)
    : fd(fd), c(c), n(n), offset(offset) {}

io_await_recv::io_await_recv(int fd, char* c, size_t n, int flags)
    : fd(fd), c(c), n(n), flags(flags) {}
io_await_send::io_await_send(int fd, const char* c, size_t n, int flags)
    : fd(fd), c(c), n(n), flags(flags) {}
io_await_send_zc::io_await_send_zc(int fd, const char* c, size_t n, int flags)
    : fd(fd), c(c), n(n), flags(flags) {}
io_await_recvmsg::io_await_recvmsg(int fd, message_header* msg, int flags)
    : fd(fd), msg(msg), flags(flags) {}
io_await_sendmsg::io_await_sendmsg(int fd, message_header* msg, int flags)
    : fd(fd), msg(msg), flags(flags) {}

void io_await_recv::prepare(io_uring_sqe* sqe) {
  io_uring_prep_recv(sqe, fd, c, n, flags);
}
void io_await_send::prepare(io_uring_sqe* sqe) {
  io_uring_prep_send(sqe, fd, c, n, flags);
}
void io_await_send_zc::prepare(io_uring_sqe* sqe) {
  io_uring_prep_send_zc(sqe, fd, c, n, flags, 0);
}
void io_await_read::prepare(io_uring_sqe* sqe) {
  io_uring_prep_read(sqe, fd, c, n, offset);
}
void io_await_recvmsg::prepare(io_uring_sqe* sqe) {
  io_uring_prep_recvmsg(sqe, fd, msg, flags);
}
void io_await_sendmsg::prepare(io_uring_sqe* sqe) {
  io_uring_prep_sendmsg(sqe, fd, msg, flags);
}

io_await_read async_read(int fd, char* c, size_t n, u64 offset) {
  return io_await_read{fd, c, n, offset};
}
io_await_write async_write(int fd, const char* c, size_t n, u64 offset) {
  return io_await_write{fd, c, n, offset};
}
io_await_recv async_recv(int fd, char* c, size_t n, int flags) {
  return {fd, c, n, flags};
}
io_await_send async_send(int fd, const char* c, size_t n, int flags) {
  return {fd, c, n, flags};
}
io_await_send_zc async_send_zc(int fd, const char* c, size_t n, int flags) {
  return {fd, c, n, flags};
}
}  // namespace zio
