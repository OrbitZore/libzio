#include <bits/stdc++.h>
#include <memory>
#include <string>
#include <string_view>
#include "uringtest.h"
#include "zio_ip.hpp"
#include "zio_types.hpp"
using namespace zio;
using namespace zio::ip;
using namespace zio::debug;
using namespace std;
constexpr auto welcome_string =
    "System > welcome to z3475's chatroom!\n"
    "System > enter any words to chat!\n";
constexpr auto help_string =
    "System > command list:\n"
    "System > /nick <nickname> - change your nickname(up to 6 chars)\n"
    "System > /help - get this command list\n"
    "System > /online - get online user\n"
    "System > /exit - close connection\n";
struct session;
list<session*> sessions;
io_context ctx;
string_view strip(string_view a) {
  while (a.size() && isspace(a.front()))
    a.remove_prefix(1);
  while (a.size() && isspace(a.back()))
    a.remove_suffix(1);
  return a;
}
string get_date() {
  time_t now = time(NULL);
  char buffer[16];
  strftime(buffer, sizeof buffer, "[%H:%M:%S] ", localtime(&now));
  return buffer;
}
int s = 0;
shared_ptr<string> system_name = make_shared<string>("System > ");
void broadcast(shared_ptr<string> name, shared_ptr<string> message);
shared_ptr<string> merge_message(shared_ptr<string> name,
                                 shared_ptr<string> message) {
  return make_shared<string>(get_date() + *name + *message);
}
string merge_message(const string& name, const string& message) {
  return get_date() + name + message;
}
struct session {
  shared_ptr<string> name;
  connection<ipv4, tcp> con;
  list<session*>::iterator i;
  session(connection<ipv4, tcp> con) : con(std::move(con)) {
    name.reset(new string("id_" + to_string(++s)));
    name->resize(6, ' ');
    *name += " > ";
    i = sessions.insert(sessions.end(), this);
  }
  ~session() {
    con.close();
    sessions.erase(i);
    broadcast(system_name,
              make_shared<string>(name->substr(0, name->size() - 3) +
                                  " leave chatroom!\n"));
  }
  awaitable<void> welcome() {
    co_await con.async_send(welcome_string, strlen(welcome_string));
    co_await con.async_send(help_string, strlen(help_string));
  }
  awaitable<void> run() {
    {
      co_await welcome();
      broadcast(system_name,
                make_shared<string>(name->substr(0, name->size() - 3) +
                                    " come chatroom!\n"));
    }

    auto message = make_unique<string>();
    char buffer[1024];
    co_await [&]() -> awaitable<void> {
      while (1) {
        int n1 = co_await con.async_recv(buffer, 1024);
        if (n1 <= 0)
          co_return;
        for (int i = 0; i < n1; i++) {
          *message += buffer[i];
          if (buffer[i] == '\n') {
            if (co_await handle(
                    std::exchange(message, make_unique<string>()))) {
              co_return;
            }
            continue;
          }
        }
      }
    }();
    delete this;
  }
  awaitable<bool> handle(shared_ptr<string> message) {
    if (message->starts_with("/help")) {
      co_await con.async_send(help_string, strlen(help_string));
    } else if (message->starts_with("/nick")) {
      auto name_view = strip(string_view(*message).substr(6, 6));
      if (name_view.size()) {
        name = unique_ptr<string>(new string(name_view));
        name->resize(6, ' ');
        *name += " > ";
      }
      auto message = get_date() + *system_name + "changed name to " +
                     name->substr(0, name->size() - 3) + "\n";
      co_await send(std::move(message));
    } else if (message->starts_with("/online")) {
      vector<shared_ptr<string>> names;
      for (auto i : sessions)
        names.emplace_back(i->name);
      co_await send(
          merge_message(system_name, make_shared<string>("Online User:\n")));
      for (auto name : names) {
        co_await send(merge_message(
            system_name,
            make_shared<string>(name->substr(0, name->size() - 3) + "\n")));
      }
    } else if (message->starts_with("/exit")) {
      co_await send(merge_message(*system_name, "Have a good day!\n"));
      co_return true;
    } else
      broadcast(name, message);
    co_return false;
  }
  awaitable<void> send(string line) {
    co_await con.async_send(line.data(), line.size());
  }
  awaitable<void> send(shared_ptr<string> line) {
    co_await con.async_send(line->data(), line->size());
  }
};

void broadcast(shared_ptr<string> name, shared_ptr<string> message) {
  auto line = merge_message(name, message);
  for (auto session : sessions)
    ctx.reg(session->send(line));
}

awaitable<void> server(ipv4 addr) {
  tcp::acceptor<ipv4> ac(addr);
  while (auto con = co_await ac.async_accept()) {
    ctx.reg((new session(std::move(con)))->run());
  }
}

int main(int argc, char* argv[]) {
  assert(argc >= 2);
  if (auto ip = ipv4::from_url(argv[1])) {
    cerr << ip->to_pret() << endl;
    ctx.reg(server(*ip));
    ctx.run();
  } else {
    cerr << "url prase error!" << endl;
  }
}