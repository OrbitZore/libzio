#include "uringtest.h"
#include "zio_ip.hpp"
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
    auto x = co_await G1;
    auto y = co_await G2;
    auto z = co_await G3;
    // cerr<<x<<"\n"<<y<<"\n"<<z<<endl;
    // cerr<<(bool)G1<<endl;
    // cerr<<(bool)G2<<endl;
    // cerr<<(bool)G3<<endl;
    if (!x && !y && !z)
      break;
    if (x)
      cout << *x << endl;
    if (y)
      cout << *y << endl;
    if (z)
      cout << *z << endl;
  }
  co_return ;
}
awaitable<void> echo_server(connection con){
  // const char *c="hello!";
  // co_await con.async_write(c, strlen(c));
  char c[1024];
  while (1){
    int n1=co_await con.async_read(c, 1024);
    cerr<<"recv:"<<n1<<endl;
    if (n1<=0) break;
    int n2=co_await con.async_write(c, n1);
    cerr<<"send:"<<n2<<endl;
    if (n2<=0) break;
  }
  con.close();
  co_return ;
}
awaitable<void> server(io_context& ctx) {

  tcp::acceptor<ipv4> acceptor(ipv4::from_pret("0.0.0.0",4000));
  while (1){
    connection con=co_await acceptor.async_accept();
    if (con){
      ctx.reg(echo_server(std::move(con)));
    }
  }
  co_return ;
}

awaitable<void> echo_client(connection con){
  char c[1024];
  while (1){
    int n1=co_await con.async_read(c, 1024);
    cerr<<"recv:"<<n1<<endl;
    if (n1<=0) break;
    int n2=co_await con.async_write(c, n1);
    cerr<<"send:"<<n2<<endl;
    if (n2<=0) break;
  }
  con.close();
  co_return ;
}
awaitable<void> request_uuid_client(connection con) {
  {
    static const char* uuid_request=
  "GET /uuid HTTP/1.1\r\n"
  "Host: httpbin.org\r\n"
  "User-Agent: curl/7.86.0\r\n"
  "accept: */*\r\n\r\n";
    static const int len=strlen(uuid_request);
    const char* c=uuid_request;
    while (c!=uuid_request+len){
      int n=co_await con.async_write(c,len-(c-uuid_request));
      if (n<=0) break;
      c+=n;
    }
    cerr<<"write::"<<(c-uuid_request)<<endl;
  }
  char c[1024];
  string res;
  while (1){
    int n1=co_await con.async_read(c, 1024);
    if (n1<=0) break;
    res+=string_view(c,c+n1);
    // cerr<<"recv:"<<string_view(c,c+n1)<<endl<<(int)res.back()<<" "<<(int)res.end()[-2]<<endl;
    if (res.ends_with("\r")||res.ends_with("\n"))
      break;
  }
  con.close();
  cerr<<res<<endl;
  co_return ;
}
awaitable<void> client(io_context& ctx) {
  const auto ip =ipv4::from_pret("127.0.0.1",1244);
  while (true){
    connection con=co_await tcp::async_connect(ip);
    if (con){
      ctx.reg(echo_server(std::move(con)));
    }else{
      cerr<<"connect fail"<<endl;
    }
  }
  co_return ;
}
awaitable<void> echo_client_await(io_context& ctx,ipv4 addr){
  while (1){
    auto con=tcp::async_connect(addr);
    auto tm=async_timeout(3s);
    co_await wait_any(con,tm);
    if (!con){
      auto connection=con.get_return();
      if (connection){
        cout<<"connect success!"<<con.get_return().fd<<","<<con.get_return().status<<endl;
        ctx.reg(request_uuid_client(::move(connection)));
      }else{
        cout<<"connect fail!"<<con.get_return().status<<endl;
      }
    }else{
      cout<<"connect timeout:"<<con.get_return().status<<endl;
    }
    co_await wait_all(tm,con);
  }
}
int main() {
  io_context ctx;
  // const auto ip =ipv4::from_pret("127.0.0.1",1244);
  const auto ip =ipv4::from_pret("198.18.0.217", 80);
  // ctx.reg(echo_client_await(ctx,ip));
  auto af=f();
  ctx.reg(af);
  // ctx.reg(server(ctx));
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