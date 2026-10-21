#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace test {
inline void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
inline bool near(float a, float b) { return std::abs(a - b) < 0.02f; }
template<class F> int run(F body) {
  try {
    body();
    std::cout << "checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
}
