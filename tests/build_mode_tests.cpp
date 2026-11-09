#include "test_support.hpp"

int main() {
  return test::run([] {
#ifdef NDEBUG
    constexpr bool assertionsEnabled = false;
#else
    constexpr bool assertionsEnabled = true;
#endif
    test::require(assertionsEnabled,
                  "Game regression checks must remain active in Release, including legacy assert setup");
  });
}
