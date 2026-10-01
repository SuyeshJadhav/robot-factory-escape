#pragma once

#include <string>

inline std::string speedLabel(float speed) {
  if (speed == 0.5f)
    return "0.5X";
  if (speed == 1.f)
    return "1X";
  if (speed == 2.f)
    return "2X";
  return std::to_string(speed) + "X";
}
