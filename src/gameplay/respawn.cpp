#include <robot_factory_escape/gameplay/player_motion.hpp>
#include <robot_factory_escape/gameplay/respawn.hpp>
#include <stdexcept>

namespace factory {
void assignSpawn(engine::Scene &scene, const World &world, engine::EntityId robot,
                 engine::ClientId client) {
  const std::size_t slot = client == 0 ? 0 : (client - 1) % world.spawns.size();
  const auto spawn = world.spawns.at(slot);
  if (!scene.has<SpawnPoint>(spawn))
    throw std::logic_error("invalid spawn assignment");
  scene.add<RespawnTarget>(robot, {spawn});
}
void respawnPlayer(engine::Scene &scene, engine::EntityId robot, bool &grounded) {
  const auto *target = scene.get<RespawnTarget>(robot);
  if (!target || !scene.hasEntity(target->spawn) || !scene.has<SpawnPoint>(target->spawn) ||
      !scene.getRigidBody(robot))
    throw std::logic_error("invalid respawn target");
  resetRobot(scene, robot, scene.transform(target->spawn).position, grounded);
}
bool inDeathZone(engine::Scene &scene, const engine::PhysicsSystem &physics,
                 engine::EntityId robot) {
  for (const auto &[zone, tag] : scene.all<DeathZone>())
    if (physics.isCollision(scene, robot, zone))
      return true;
  return false;
}
} // namespace factory
