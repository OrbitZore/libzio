#pragma once
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include "uringtest.h"
using namespace std;
namespace zio::ip {
struct ipv4 : public sockaddr_in {
  static inline constexpr auto AF = AF_INET;
  static inline constexpr size_t length() { return sizeof(ipv4); }
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
  template <class ipT>
  using acceptor = zio::acceptor<ipT, zio::ip::tcp>;

  template <class T>
  static io_await_connect async_connect(T&& addr) {
    return zio::async_connect<T, tcp>(forward<T>(addr));
  }
};

struct udp {
  static constexpr auto SOCK = SOCK_DGRAM;
  static constexpr auto PROTO = IPPROTO_UDP;
  template <class ipT>
  using acceptor = zio::acceptor<ipT, zio::ip::udp>;
  template <class T>
  static io_await_connect async_connect(T&& addr) {
    return zio::async_connect<T, udp>(forward<T>(addr));
  }
};
}  // namespace zio::ip