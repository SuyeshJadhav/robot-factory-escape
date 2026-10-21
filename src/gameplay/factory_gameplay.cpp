#include <algorithm>
#include <robot_factory_escape/gameplay/factory_gameplay.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

namespace factory {
TickResult tickPlayer(engine::Scene &scene, engine::PhysicsSystem &physics, const World &world,
                      engine::EntityId robot, bool &grounded, GameProgress &progress, float dt,
                      bool ownsWorld) {
  if (progress.won)
    return TickResult::None;
  float remaining = std::clamp(dt, 0.f, 0.1f);
  while (remaining > 0.f) {
    const float step = std::min(remaining, 1.f / 120.f);
    remaining -= step;
    if (ownsWorld) {
      const auto previous = scene.transform(world.movingPlatform).position;
      advanceWorld(scene, step);
      carryPlayerWithPlatform(scene, robot, world.movingPlatform, previous, world.solids, grounded);
    }
    movePlayer(scene, physics, robot, world.solids, step, grounded);
    if (physics.isCollision(scene, robot, world.drone)) {
      respawnPlayer(scene, robot, grounded);
      return TickResult::DroneHit;
    }
    if (inDeathZone(scene, physics, robot)) {
      respawnPlayer(scene, robot, grounded);
      return TickResult::DeathZone;
    }
    if (progress.checkExit(scene, physics, robot, world.exit))
      return TickResult::Won;
  }
  return TickResult::None;
}
void restartPlayer(engine::Scene &scene, engine::EntityId robot, bool &grounded,
                   GameProgress &progress, bool ownsWorld) {
  respawnPlayer(scene, robot, grounded);
  progress.won = false;
  if (ownsWorld)
    resetWorld(scene);
}
} // namespace factory
