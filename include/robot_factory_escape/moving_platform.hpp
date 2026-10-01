#pragma once

#include <engine/engine.hpp>

#include <cmath>

struct MovingPlatformPatrol {
  static constexpr float left = 330.f;
  static constexpr float right = 450.f;
  static constexpr float height = 750.f;
  static constexpr float speed = 60.f;
  float distance = 0.f;

  void advance(engine::Scene &scene, engine::EntityId platform, float dt) {
    constexpr float length = right - left;
    distance = std::fmod(distance + speed * dt, 2.f * length);
    const float offset = distance <= length ? distance : 2.f * length - distance;
    scene.transform(platform).position = {left + offset, height};
  }
};
