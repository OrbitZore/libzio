#pragma once
#include <netinet/in.h>
#include <functional>
#include <string>
#include <optional>
#include <type_traits>
namespace zio::types {
template <class T>
concept Address = requires(std::remove_cvref_t<T> a) {
                    a.AF();
                    a.length();
                    a.set_port(0);
                  };
template <class T>
concept Protocol = requires(std::remove_cvref_t<T> a) {
                     a.SOCK;
                     a.PROTO;
                   };
//return {iter,bool} means readed [beg,iter) and true for done
template <class T>
concept MatchFunction =
    requires(T a) { std::function<std::pair<char*,bool>(char*,char*)>(a); };
template <class T>
concept IS_AWAITABLE = requires(std::remove_cvref_t<T> a){
  a.is_awaitble();
};
}  // namespace zio::types