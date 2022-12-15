#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string_view>
#include "uringtest.h"
#include "zio_ip.hpp"
#include "zio_types.hpp"
using namespace zio;
using namespace zio::ip;
using namespace zio::debug;
using namespace std;
template <types::Address Address>
awaitable<void> client(io_context& ctx,Address addr) {
    connection<Address,tcp> con=co_await tcp::async_connect(addr);
    if (!con){
        perrno(con.status);
        co_return ;
    }
    char c1[1024],c2[1024];
    io_await_recv iorc=con.async_recv(c1, 1024);
    io_await_read iord=async_read(STDIN_FILENO, c2, 1024);
    while (1){
        co_await wait_any(iorc,iord);
        if (!iorc){
            int n=iorc.get_return();
            if (n<=0){
                perrno(n);
                exit(0);
            }
            ctx.reg(async_write(STDOUT_FILENO, c1, n));
            iorc=con.async_recv(c1, 1024);
        }
        if (!iord){
            int n=iord.get_return();
            if (n<=0){
                // cerr<<"iord:";
                perrno(n);
                exit(0);
            }
            ctx.reg(con.async_send(c2, n));
            iord=async_read(STDIN_FILENO, c2, 1024);
        }
    }
    con.close();
}
int main(int argc, char* argv[]) {
  assert(argc >= 2);
  if (auto ip = ipv4::from_url(argv[1])) {
    cerr<<ip->to_pret()<<endl;
    io_context ctx;
    ctx.reg(client(ctx,*ip));
    ctx.run();
  } else {
    cerr << "url prase error!" << endl;
  }
}