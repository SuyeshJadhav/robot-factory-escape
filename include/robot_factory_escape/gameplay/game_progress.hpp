#pragma once

#include <engine/engine.hpp>

struct GameProgress {
  bool won = false;

  bool checkExit(engine::Scene &scene, engine::PhysicsSystem &physics, engine::EntityId robot,
                 engine::EntityId exit) {
    if (won || !physics.isCollision(scene, robot, exit))
      return false;
    won = true;
    scene.getRigidBody(robot)->velocity = {0.f, 0.f};
    return true;
  }

};
