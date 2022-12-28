#include <fcntl.h>
#include <cstring>
#include <filesystem>
#ifdef __clang__
constexpr auto COMPILER_NAME = "clang " __clang_version__;
#elif __GNUC__
#define _TO(arg) #arg
#define __TOO(arg) _TO(arg)
constexpr auto COMPILER_NAME = "GCC " __TOO(__GNUC__);
#elif
constexpr auto COMPILER_NAME = "Unknown Compiler";
#endif

#include <bits/stdc++.h>
#include "uringtest.h"
#include "zio_http.hpp"
#include "zio_ip.hpp"
#include "zio_types.hpp"

using namespace zio;
using namespace std;
constexpr auto html_template =
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
    "\"http://www.w3.org/TR/html4/strict.dtd\">"
    "\r\n"
    "<html>"
    "\r\n"
    "<head>"
    "\r\n"
    "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
    "\r\n"
    "<title>Directory listing for {}</title>"
    "\r\n"
    "</head>"
    "\r\n"
    "<body>"
    "\r\n"
    "<h1>Directory listing for {}</h1>"
    "\r\n"
    "<hr>"
    "\r\n"
    "<ul>"
    "\r\n"
    "{<li><a href=\"{}/\">{}</a></li>"
    "\r\n}"
    "{<li><a href=\"{}\">{}</a></li>"
    "\r\n}"
    "</ul>"
    "</hr>"
    "</body>"
    "</html>";
io_context ctx;
awaitable<void> write_html(connection<ip::ipv4, ip::tcp>& con,
                           string content,
                           int status_code = 200) {
  auto header =
      http::make_http_header(status_code, content.size(), http::MIME::html);
  int n = co_await con.async_write(header.data(), header.size());
  if (n < header.size())
    co_return;
  n = co_await con.async_write(content.data(), content.size());
  if (n < content.size())
    co_return;
}

awaitable<void> write_file(connection<ip::ipv4, ip::tcp>& con,
                           filesystem::path filename,
                           int status_code = 200) {
  size_t filesize = file_size(filename), available = 0;
  auto fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0)
    status_code = 404;
  auto header = http::make_http_header(status_code, filesize, http::MIME::file);
  int n = co_await con.async_write(header.data(), header.size());
  if (n < header.size())
    co_return;
  if (fd < 0)
    co_return;
  array<char, 16 * 1024> data;  // buffer size 16k
  while ((filesize -= available) &&
         (available = co_await async_read(fd, &data[0], 16 * 1024))) {
    int c = 0;
    while (c < available) {
      int n = co_await con.async_write(&data[c], available-c);
      if (n < 0) {
        cerr << format("Errorno [ {} ]: {}\n", n, strerror(-n));
        co_return;
      }
      c += n;
    }
  }
  close(fd);
}
awaitable<void> http_server(connection<ip::ipv4, ip::tcp> con,
                            filesystem::path path) {
  buffer::zio_istream is(std::move(con));
  while (is.con) {
    try {
      auto req = co_await http::http_request::read_request(is);
      if (!req)
        co_return;
      if (req.url.front() == '/')
        req.url.remove_prefix(1);
      auto dirpath = path / http::url::decode(req.url);
      int status_code;
      if (is_directory(dirpath)) {
        vec<pair<string, string>> folders = {{".", "."}, {"..", ".."}}, files;
        for (auto i : filesystem::directory_iterator(dirpath)) {
          string filename = i.path().filename();
          (i.is_directory() ? folders : files)
              .push_back({http::url::encode(filename), filename});
        }

        co_await write_html(
            is.con, format(html_template, req.url, req.url, folders, files),
            status_code = 200);
      } else if (is_regular_file(dirpath)) {
        co_await write_file(is.con, dirpath, status_code = 200);
      } else {
        co_await write_html(is.con, "", status_code = 404);
      }

      cerr << format("[{}] \"{} /{} {} {}\"\n", tools::format_now("%c"),
                     req.method, req.url, req.version, status_code);
    } catch (const exception& e) {
      cerr << e.what() << endl;
      co_return;
    }
  }
}

awaitable<void> server(ip::ipv4 ip, filesystem::path path) {
  auto acceptor = ip::tcp::acceptor<ip::ipv4>(ip);
  while (true) {
    auto x = co_await acceptor.async_accept();
    if (x) {
      timeval timeout;
      timeout.tv_sec = 3;
      timeout.tv_usec = 0;
      if (x.setopt(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0 ||
          x.setopt(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0) {
        perror("setsocketopt error!");
      }

      ctx.reg(http_server(std::move(x), path));
    }
  }
}
int main(int argc, char* argv[]) {
  if (argc != 4) {
    cerr << "Usage: simple http file server\n";
    cerr << " <listen_address> <listen_port>\n";
    cerr << " <shared_directory>\n";
    cerr << "Version: 1.0\n";
    cerr << "Compiler: " << COMPILER_NAME << "\n";
    return 1;
  }
  signal(SIGPIPE, SIG_IGN);
  ctx.reg(
      server(ip::ipv4::from_pret(argv[1], *tools::to_int(argv[2])), argv[3]));
  ctx.run();
}