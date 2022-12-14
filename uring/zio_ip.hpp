#pragma once
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include "uringtest.h"
#include "zio_types.hpp"
using namespace std;
namespace zio::ip {
struct ipv4;
struct ipv6;
template <class T>
concept IP_Address = types::Address<T> && (is_same_v<remove_cvref_t<T>, ipv4> ||
                                           is_same_v<remove_cvref_t<T>, ipv6>);

template <IP_Address Address>
inline optional<Address> from_url(const char* a) {
  addrinfo* ai;
  int i = 0, port = 0, di = 0, j = 0, k = 0;
  char domain[128];
  while (a[i] && a[i++] != ':')
    ;
  k = j = i;
  while (a[j] && a[j++] != ':')
    ;
  if (a[i] == '/' && a[i + 1] == '/') {
    if (a[i + 2] == 'h' && a[i + 3] == 't' && a[i + 4] == 't' &&
        a[i + 5] == 'p') {
      if (a[i + 6] == 's')
        port = 443;
      else
        port = 80;
      k = j;
    }
  }
  {
    int ii = port ? i + 2 : 0;
    while (a[ii] && a[ii] != '/' && a[ii] != ':')
      domain[di++] = a[ii++];
  }
  if (isdigit(a[k])) {
    port = 0;
    do {
      port = port * 10 + a[k] - '0';
    } while (isdigit(a[++k]));
  }
  domain[di] = '\0';
  addrinfo hint{};
#ifdef DEBUG
  cerr << domain << endl;
#endif
  hint.ai_family = Address::AF;
  if (getaddrinfo(domain, NULL, &hint, &ai))
    return {};
  if (ai) {
    Address addr{};
    memcpy(&addr, ai->ai_addr, ai->ai_addrlen);
    addr.set_port(port);
    freeaddrinfo(ai);
    return addr;
  }
  return {};
}
struct ipv4 : public sockaddr_in {
  static inline constexpr auto AF = AF_INET;
  static inline constexpr size_t length() { return sizeof(ipv4); }
  inline void set_port(uint16_t hport) { sin_port = htons(hport); }
  static inline optional<ipv4> from_url(const char* a) {
    return ip::from_url<ipv4>(a);
  }
  static inline ipv4 from_pret(const char* c, uint16_t port) {
    ipv4 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF;
    inet_pton(AF, c, &addr.sin_addr);
    addr.sin_port = htons(port);
    return addr;
  }
  string inline to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin_addr, &str[0], INET_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin_port));
    return a;
  }
};
struct ipv6 : public sockaddr_in6 {
  static inline constexpr auto AF = AF_INET6;
  static inline constexpr size_t length() { return sizeof(ipv6); }
  inline void set_port(uint16_t hport) { sin6_port = htons(hport); }
  static inline optional<ipv6> from_url(const char* a) {
    return ip::from_url<ipv6>(a);
  }
  static inline ipv6 from_pret(const char* c, uint16_t port) {
    ipv6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF;
    inet_pton(AF, c, &addr.sin6_addr);
    addr.sin6_port = htons(port);
    return addr;
  }
  string inline to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin6_addr, &str[0], INET6_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin6_port));
    return a;
  }
};
struct tcp {
  static inline constexpr auto SOCK = SOCK_STREAM;
  static inline constexpr auto PROTO = IPPROTO_TCP;
  template <IP_Address T>
  using acceptor = zio::acceptor<T, zio::ip::tcp>;
  template <IP_Address T>
  static auto async_connect(T&& addr) {
    return zio::async_connect<T, tcp>(forward<T>(addr));
  }
};

struct udp {
  static constexpr auto SOCK = SOCK_DGRAM;
  static constexpr auto PROTO = IPPROTO_UDP;
  template <IP_Address T>
  using acceptor = zio::acceptor<T, zio::ip::udp>;
  template <IP_Address T>
  static auto async_connect(T&& addr) {
    return zio::async_connect<T, udp>(forward<T>(addr));
  }
  template <IP_Address T>
  static auto open(T&& addr) {
    return zio::open<T, udp>(forward<T>(addr));
  }
};
}  // namespace zio::ip