#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr float contactTolerance = 0.001f;

bool onTop(glm::vec2 playerPosition, glm::vec2 playerSize, glm::vec2 surfacePosition,
           glm::vec2 surfaceSize) {
  return playerPosition.x < surfacePosition.x + surfaceSize.x &&
         playerPosition.x + playerSize.x > surfacePosition.x &&
         std::abs(playerPosition.y + playerSize.y - surfacePosition.y) < contactTolerance;
}
} // namespace

void resetRobot(engine::Scene &scene, engine::EntityId robot, glm::vec2 spawn, bool &grounded) {
  scene.transform(robot).position = spawn;
  scene.getRigidBody(robot)->velocity = {0.f, 0.f};
  grounded = false;
}

bool playerIsSupported(engine::Scene &scene, engine::EntityId player,
                       std::span<const engine::EntityId> solids) {
  if (scene.getRigidBody(player)->velocity.y < 0.f)
    return false;
  for (const auto solid : solids) {
    if (onTop(scene.transform(player).position, scene.getCollider(player)->size,
              scene.transform(solid).position, scene.getCollider(solid)->size))
      return true;
  }
  return false;
}

void carryPlayerWithPlatform(engine::Scene &scene, engine::EntityId player,
                             engine::EntityId platform, glm::vec2 previousPosition,
                             std::span<const engine::EntityId> solids, bool &grounded) {
  auto &position = scene.transform(player).position;
  const auto size = scene.getCollider(player)->size;
  if (!grounded || scene.getRigidBody(player)->velocity.y < 0.f ||
      !onTop(position, size, previousPosition, scene.getCollider(platform)->size))
    return;

  float displacement = scene.transform(platform).position.x - previousPosition.x;
  // Sweep the carry against the other solids so a delayed snapshot cannot
  // transport the robot through a wall. Keep voluntary velocity independent.
  for (const auto solid : solids) {
    if (solid == platform)
      continue;
    const auto obstacle = scene.transform(solid).position;
    const auto obstacleSize = scene.getCollider(solid)->size;
    if (position.y >= obstacle.y + obstacleSize.y || position.y + size.y <= obstacle.y)
      continue;
    if (displacement > 0.f && position.x + size.x <= obstacle.x)
      displacement = std::min(displacement, obstacle.x - position.x - size.x);
    else if (displacement < 0.f && position.x >= obstacle.x + obstacleSize.x)
      displacement = std::max(displacement, obstacle.x + obstacleSize.x - position.x);
  }
  position.x += displacement;
  grounded = playerIsSupported(scene, player, solids);
}

bool jumpIfGrounded(engine::RigidBody &body, bool &grounded, bool newPress) {
  if (!newPress || !grounded) {
    return false;
  }
  body.velocity.y = -gameSettings::jumpSpeed;
  grounded = false;
  return true;
}

void movePlayer(engine::Scene &scene, engine::PhysicsSystem &physics, engine::EntityId player,
                std::span<const engine::EntityId> solids, float deltaSeconds, bool &grounded) {
  auto &position = scene.transform(player).position;
  auto &velocity = scene.getRigidBody(player)->velocity;
  const auto size = scene.getCollider(player)->size;

  // Limit catch-up after a pause; short steps keep normal gameplay movement
  // smaller than the obstacles. This is not a general swept collision solver.
  float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
  while (remaining > 0.f) {
    const float step = std::min(remaining, 1.f / 120.f);
    remaining -= step;
    const auto previous = position;
    physics.step(scene, step); // The robot is the scene's only rigid body.
    const float nextY = position.y;

    // Resolve X at the old height, then resolve Y at the corrected X.
    position.y = previous.y;
    for (const auto solid : solids) {
      if (!physics.isCollision(scene, player, solid)) {
        continue;
      }
      const auto overlap = physics.GetCollisionOverlap(scene, player, solid);
      if (overlap.size.x <= 0.f || overlap.size.y <= 0.f) {
        continue; // Touching an edge is not penetration.
      }
      const auto solidPosition = scene.transform(solid).position;
      const auto solidSize = scene.getCollider(solid)->size;
      if (velocity.x > 0.f) {
        position.x = solidPosition.x - size.x;
      } else if (velocity.x < 0.f) {
        position.x = solidPosition.x + solidSize.x;
      }
      velocity.x = 0.f;
    }

    position.y = nextY;
    grounded = false;
    for (const auto solid : solids) {
      if (!physics.isCollision(scene, player, solid)) {
        continue;
      }
      const auto overlap = physics.GetCollisionOverlap(scene, player, solid);
      if (overlap.size.x <= 0.f || overlap.size.y <= 0.f) {
        continue;
      }
      const auto solidPosition = scene.transform(solid).position;
      const auto solidSize = scene.getCollider(solid)->size;
      if (velocity.y > 0.f) {
        position.y = solidPosition.y - size.y;
        grounded = true;
      } else if (velocity.y < 0.f) {
        position.y = solidPosition.y + solidSize.y;
      }
      velocity.y = 0.f;
    }
    // At very high frame rates gravity's displacement can round to zero.
    // Exact surface contact still supports the robot even without overlap.
    if (velocity.y >= 0.f) {
      for (const auto solid : solids) {
        const auto solidPosition = scene.transform(solid).position;
        const auto solidSize = scene.getCollider(solid)->size;
        const bool overSurface =
            position.x < solidPosition.x + solidSize.x && position.x + size.x > solidPosition.x;
        if (overSurface && std::abs(position.y + size.y - solidPosition.y) < 0.001f) {
          position.y = solidPosition.y - size.y;
          velocity.y = 0.f;
          grounded = true;
        }
      }
    }
  }
}
