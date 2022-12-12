#include "uringtest.h"
#include "zio_ip.hpp"
#include "zio_types.hpp"
using namespace zio;
using namespace zio::ip;
using namespace std;
awaitable<void> echo_server(connection con) {
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

template<types::Address Address>
awaitable<void> server(io_context& ctx,Address &addr) {
  tcp::acceptor<ipv4> acceptor(addr);
  char c;
  auto g=async_read(0,&c,1);
  while (1) {
    auto ac=acceptor.async_accept();
    co_await wait_any(ac,g);
    if (!ac) {
      auto con=ac.get_return();
      if (con)
        ctx.reg(echo_server(std::move(con)));
    }else exit(0);
  }
  co_return;
}
int main(int argc,char *argv[]){
  assert(argc>=2);
  if (auto ip=ipv4::from_url(argv[1])){
    io_context ctx;
    cerr<<ip->to_pret()<<endl;
    ctx.reg(server(ctx, *ip));
    ctx.run();
  }else{
    cerr<<"url prase error!"<<endl;
  }
}