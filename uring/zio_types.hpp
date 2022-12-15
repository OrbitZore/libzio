#pragma once
#include <netinet/in.h>
#include <type_traits>
namespace zio::types {
template <class T>
concept Address = requires(std::remove_cvref_t<T> a) {
                    a.AF;
                    a.length();
                    a.set_port(0);
                  };
template <class T>
concept Protocol = requires(std::remove_cvref_t<T> a) {
                     a.SOCK;
                     a.PROTO;
                   };
}  // namespace zio::types