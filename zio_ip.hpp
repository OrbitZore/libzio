#pragma once
#include <arpa/inet.h>
#include <asm-generic/errno.h>
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
#include <bits/stdc++.h>
using namespace std;
#ifndef ZIO_IP
#define ZIO_IP
namespace zio::ip {
struct ipv4 : public sockaddr_in {
  static constexpr auto AF = AF_INET;
  static constexpr size_t length() { return sizeof(ipv4); }
  static ipv4 from_pret(const char* c, uint16_t port) {
    ipv4 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF;
    inet_pton(AF, c, &addr.sin_addr);
    addr.sin_port = htons(port);
    return addr;
  }
  string to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin_addr, &str[0], INET_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin_port));
    return a;
  }
};
struct ipv6 : public sockaddr_in6 {
  static constexpr auto AF = AF_INET6;
  static constexpr size_t length() { return sizeof(ipv6); }
  static ipv6 from_pret(const char* c, uint16_t port) {
    ipv6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF;
    inet_pton(AF, c, &addr.sin6_addr);
    addr.sin6_port = htons(port);
    return addr;
  }
  string to_pret() const {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF, &this->sin6_addr, &str[0], INET6_ADDRSTRLEN);
    string a = str;
    a += ":" + to_string(ntohs(sin6_port));
    return a;
  }
};
struct tcp {
  static constexpr auto SOCK = SOCK_STREAM;
  static constexpr auto PROTO = IPPROTO_TCP;
};
struct udp {
  static constexpr auto SOCK = SOCK_DGRAM;
  static constexpr auto PROTO = IPPROTO_UDP;
};
}
#endif
