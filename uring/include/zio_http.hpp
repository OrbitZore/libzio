#pragma once
#include <charconv>
#include <string_view>
#include <unordered_map>
#include "uringtest.h"
#include "zio_buffer.hpp"
#include "zio_ip.hpp"
#include "zio_types.hpp"

using namespace std;
namespace zio::http {
namespace url {
inline string encode(string_view a) {
  ostringstream os;
  os.fill('0');
  os << hex;
  for (auto c : a) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      os << c;
    } else {
      os << uppercase << '%' << setw(2) << int((unsigned char)c) << nouppercase;
    }
  }
  return os.str();
}

inline string decode(string_view a) {
  string os;
  for (auto i = begin(a); i != end(a);) {
    if (*i == '%' && end(a) - i >= 3) {
      os += tools::asciihex_to_uint(*next(i)) * 16 +
            tools::asciihex_to_uint(*next(i, 2));
      i += 3;
    } else {
      os += *i;
      i++;
    }
  }
  return os;
}
}  // namespace url
namespace STATUS_CODE {
inline map<int, string> status_code_to_name = {
    {200, "OK"},
    {404, "Not Found"},
};
}
namespace MIME {
enum data_type {
  html,
  file,
};

inline map<data_type, string> data_type_name = {
    {html, "text/html; charset=utf-8"},
    {file, "application/octet-stream"}};
}  // namespace MIME

struct http_request {
  string _data, content;
  string_view method, url, version;
  unordered_map<string_view, string_view> header;
  inline http_request() = default;
  inline http_request(http_request&&) = default;
  inline http_request(const http_request&) = delete;
  inline http_request& operator=(http_request&&) = default;
  inline http_request& operator=(http_request&) = delete;
  inline operator bool() { return _data.size(); }
  inline void prase() {
    const char* l = _data.data();
    bool xfirst = true;
    for (const char& c : _data) {
      if (c == '\r' || c == '\n') {
        if (&c - l >= 2) {
          if (xfirst) {
            const char* l1 = ranges::find(string_view(l, &c), ' ');
            if (l1 >= &c - 2) {
              _data.clear();
              return;
            }
            const char* l2 = ranges::find(string_view(l1 + 2, &c), ' ');
            if (l2 >= &c - 1) {
              _data.clear();
              return;
            }
            method = {l, l1};
            url = {l1 + 1, l2};
            version = {l2 + 1, &c};
            xfirst = false;
          } else {
            const char* l1 = ranges::find(string_view(l, &c), ':');
            if (l1 == &c) {
              _data.clear();
              return;
            }
            header[tools::strip(string_view(l, l1))] =
                tools::strip(string_view{l1 + 1, &c});
          }
        }
        l = &c;
      }
    }
  }
  inline awaitable<void> recv_content(
      zio::buffer::zio_istream<ip::ipv4, ip::tcp>& s) {
    if (auto cl = header.find("Content-Length"); cl != header.end()) {
      if (int length;
          from_chars(cl->second.begin(), cl->second.end(), length).ec ==
          errc{}) {
        auto x = tools::to_int(cl->second);
        if (x)
          if (int len = *x; len > 0) {
            for (int i = 0; i < len; i++)
              if (auto c = co_await s.get())
                content += *c;
              else
                break;
          }
      }
    }
  }
  inline static awaitable<http_request> read_request(
      zio::buffer::zio_istream<ip::ipv4, ip::tcp>& s) {
    string buffer;
    bool f = true;
    while (auto x = co_await s.get()) {
      buffer += *x;
      if (buffer.size() >= 1024 * 1024)
        break;
      if (buffer.ends_with("\r\n\r\n") || buffer.ends_with("\n\n") ||
          buffer.ends_with("\r\r")) {
        f = false;
        break;
      }
    }
    if (f)
      co_return {};
    http_request h;
    h._data = std::move(buffer);
    h.prase();
    co_return std::move(h);
  }
};

inline string make_http_header(int status_code,
                               size_t size,
                               MIME::data_type type,
                               vector<string> extra = {}) {
  string header;
  auto add_line = [&](string_view str) {
    header += str;
    header += "\r\n";
  };
  add_line("HTTP/1.1 " + to_string(status_code) + " " +
           STATUS_CODE::status_code_to_name[status_code]);
  add_line(
      "Server: "
      "SimpleHTTP/0.1 "
      "with libzio");
  add_line("Date: " + tools::format_now("%c %Z"));
  add_line("Content-Length: " + to_string(size));
  add_line("Content-type: " + MIME::data_type_name[type]);
  for (auto& i : extra)
    add_line(i);
  add_line("");
  return header;
}
}  // namespace zio::http