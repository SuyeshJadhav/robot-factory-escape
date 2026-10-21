#include <algorithm>
#include <cmath>
#include <numbers>
#include <robot_factory_escape/gameplay/factory_world.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>

namespace factory {
namespace {
engine::EntityId box(engine::Scene &scene, glm::vec2 p, glm::vec2 visual, glm::vec2 collision,
                     engine::Color color, bool solid) {
  const auto id = scene.createEntity();
  scene.transform(id).position = p;
  scene.addShape(id, {.size = visual, .color = color});
  scene.addCollider(id, {.size = collision});
  if (solid)
    scene.add<Solid>(id);
  return id;
}
void deathZone(engine::Scene &scene, glm::vec2 p, glm::vec2 size) {
  const auto id = scene.createEntity();
  scene.transform(id).position = p;
  scene.addCollider(id, {.size = size});
  scene.add<DeathZone>(id);
}
} // namespace
World createWorld(engine::Scene &scene) {
  World world;
  const std::array<glm::vec2, 2> spawnPositions{{{120.f, 300.f}, {360.f, 300.f}}};
  for (std::size_t i = 0; i < world.spawns.size(); ++i) {
    world.spawns[i] = scene.createEntity();
    scene.transform(world.spawns[i]).position = spawnPositions[i];
    scene.add<SpawnPoint>(world.spawns[i]);
  }
  // Floor gap [2080,2320): 240 logical pixels, below the nominal jump range.
  box(scene, {0.f, 940.f}, {2080.f, 140.f}, {2080.f, 140.f}, {70, 78, 91, 255}, true);
  box(scene, {2320.f, 940.f}, {880.f, 140.f}, {880.f, 140.f}, {70, 78, 91, 255}, true);
  world.movingPlatform = box(scene, {330.f, 750.f}, levelLayout::platformSize,
                             levelLayout::platformColliderSize, {255, 255, 255, 255}, true);
  scene.add<MovingPlatformPatrol>(world.movingPlatform);
  world.platforms.push_back(world.movingPlatform);
  struct PlatformSpec {
    glm::vec2 p;
    float width;
  };
  const std::array<PlatformSpec, 8> specs{{{{650.f, 560.f}, 300.f},
                                           {{980.f, 740.f}, 260.f},
                                           {{1280.f, 550.f}, 340.f},
                                           {{1560.f, 360.f}, 300.f},
                                           {{1860.f, 550.f}, 300.f},
                                           {{2180.f, 550.f}, 300.f},
                                           {{2500.f, 740.f}, 300.f},
                                           {{2820.f, 550.f}, 300.f}}};
  for (const auto &spec : specs)
    world.platforms.push_back(
        box(scene, spec.p, {spec.width, 66.f}, {spec.width, 24.f}, {255, 255, 255, 255}, true));
  world.drone = box(scene, {1260.f, 440.f}, levelLayout::droneSize, levelLayout::droneSize,
                    {235, 85, 93, 255}, false);
  scene.add<Orbit>(world.drone);
  world.exit = scene.createEntity();
  scene.transform(world.exit).position = {3000.f, 410.f};
  scene.addCollider(world.exit, {.size = {100.f, 140.f}});
  deathZone(scene, {2080.f, 1000.f}, {240.f, 400.f});
  deathZone(scene, {-256.f, 1080.f}, {3712.f, 512.f});
  deathZone(scene, {-256.f, -512.f}, {256.f, 1592.f});
  deathZone(scene, {3200.f, -512.f}, {256.f, 1592.f});
  for (const auto &[id, tag] : scene.all<Solid>())
    world.solids.push_back(id);
  std::sort(world.solids.begin(), world.solids.end());
  return world;
}
engine::EntityId createPlayer(engine::Scene &scene, const World &world) {
  const auto id = box(scene, scene.transform(world.spawns[0]).position, levelLayout::robotSize,
                      levelLayout::robotColliderSize, {72, 205, 230, 255}, false);
  scene.addRigidBody(id);
  scene.add<RespawnTarget>(id, {world.spawns[0]});
  return id;
}

void advanceWorld(engine::Scene &scene, float dt) {
  const float elapsed = std::max(dt, 0.f);
  scene.each<MovingPlatformPatrol>([&](engine::EntityId id, MovingPlatformPatrol &motion) {
    motion.advance(scene, id, elapsed);
  });
  scene.each<Orbit>([&](engine::EntityId id, Orbit &motion) {
    motion.angle = std::fmod(motion.angle + elapsed * motion.radiansPerSecond,
                             2.f * std::numbers::pi_v<float>);
    scene.transform(id).position =
        motion.center + motion.radius * glm::vec2{std::cos(motion.angle), std::sin(motion.angle)};
  });
}
void resetWorld(engine::Scene &scene) {
  scene.each<MovingPlatformPatrol>(
      [](engine::EntityId, MovingPlatformPatrol &motion) { motion.distance = 0.f; });
  scene.each<Orbit>([](engine::EntityId, Orbit &motion) { motion.angle = 0.f; });
  advanceWorld(scene, 0.f);
}
} // namespace factory
