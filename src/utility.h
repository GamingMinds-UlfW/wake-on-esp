#pragma once
#ifndef _WAKE_ON_ESP_UTILITY_H_
#define _WAKE_ON_ESP_UTILITY_H_

namespace Utility {
namespace Serial {
template <typename T>
static size_t printLn(T &&arg) {
  return ::Serial.println(arg);
}

template <typename T, typename... T2>
static size_t printLn(T &&arg, T2 &&...args) {
  return ::Serial.print(arg) + ::Utility::Serial::printLn(std::forward<T2>(args)...);
}
}; // namespace Serial

} // namespace Utility

#endif // _WAKE_ON_ESP_UTILITY_H_