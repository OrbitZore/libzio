#pragma once
#include <cctype>
#include <optional>
#include "uringtest.h"
#include "zio_ip.hpp"
#include "zio_types.hpp"
using namespace zio;
namespace zio::buffer {
template <types::Address Address, types::Protocol Protocol>
struct zio_istream {
  connection<Address, Protocol> con;
  zio_istream(connection<Address, Protocol>&& con):con(std::move(con)){
  }
  char c[1024];
  int i{0}, n1{0};
  inline awaitable<optional<char>> get(){
    if (i >= n1) {
      n1 = co_await con.async_read(c, 1024);
      if (n1 <= 0) {
        co_return {};
      }
      i = 0;
    }
    co_return c[i++];
  }
  inline void seek_back(){
    i--;
  }
  template<class T>
  awaitable<bool> operator>>(T &a) requires integral<T>{
    char c;
    do{
      auto x=co_await this->get();
      if (!x) co_return false;
      c=*x;
    }while (isspace(c));
    a=0;
    if (!isdigit(c)) co_return false;
    do{
      a=a*10+c-'0';
      auto x=co_await this->get();
      if (!x) co_return false;
      c=*x;
    }while (isdigit(c));
    co_return true;
  }
  inline awaitable<bool> operator>>(std::string &a){
    char c;
    do{
      auto x=co_await this->get();
      if (!x) co_return false;
      c=*x;
    }while (isspace(c));
    a.clear();
    do{
      a+=c;
      auto x=co_await this->get();
      if (!x) co_return false;
      c=*x;
    }while (!isspace(c));
    co_return true;
  }
  inline awaitable<bool> read_until(std::string &a,char stop_char,size_t MAX=1024*1024){
    a.clear();
    optional<char> x;
    while ((x=co_await this->get())&&*x!=stop_char&&a.size()<MAX){
      a+=*x;
    }
    if (!x||a.size()>=MAX) co_return false;
    co_return true;
  }
  template<size_t N>
  inline awaitable<size_t> read_until(char (&a)[N],char stop_char){
    optional<char> x;
    size_t i=0;
    while ((x=co_await this->get())&&*x!=stop_char&&i<N){
      a[i++]=*x;
    }
    co_return i;
  }
  template<class FunctionType>
  awaitable<bool> read_until(std::string &a,FunctionType func,size_t MAX=1024*1024){
    a.clear();
    optional<char> x;
    while ((x=co_await this->get())&&a.size()<MAX){
      a+=*x;
      if (func(*x)) break;
    }
    if (!x||a.size()>=MAX) co_return false;
    co_return true;
  }
  template<size_t N,class FunctionType>
  awaitable<size_t> read_until(char (&a)[N],FunctionType func){
    optional<char> x;
    size_t i=0;
    while ((x=co_await this->get())&&i<N){
      a[i++]=*x;
      if (func(*x)) break;
    }
    co_return i;
  }
};
}  // namespace zio::buffer