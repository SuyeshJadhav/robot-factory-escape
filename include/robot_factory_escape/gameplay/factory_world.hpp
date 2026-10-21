#pragma once
#include <engine/engine.hpp>
#include <robot_factory_escape/gameplay/moving_platform.hpp>
#include <array>
#include <vector>

namespace factory {
struct SpawnPoint {};
struct DeathZone {};
struct Solid {};
struct RespawnTarget { engine::EntityId spawn{}; };
struct Orbit {
  glm::vec2 center{1100.f, 440.f};
  float radius = 160.f;
  float angle = 0.f;
  float radiansPerSecond = 1.f;
};
struct World {
  std::array<engine::EntityId, 2> spawns{};
  std::vector<engine::EntityId> solids;
  std::vector<engine::EntityId> platforms;
  engine::EntityId drone{}, movingPlatform{}, exit{};
};
World createWorld(engine::Scene &scene);
engine::EntityId createPlayer(engine::Scene &scene, const World &world);
void advanceWorld(engine::Scene &scene, float dt);
void resetWorld(engine::Scene &scene);
}
