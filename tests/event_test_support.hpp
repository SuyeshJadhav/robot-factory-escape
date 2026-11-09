#pragma once

#include <robot_factory_escape/gameplay/game_events.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include "test_support.hpp"

struct EventRig {
  engine::Scene scene;
  factory::World world = factory::createWorld(scene);
  engine::EntityId robot = factory::createPlayer(scene, world);
  bool grounded = false;
  GameProgress progress;
  engine::PhysicsSystem physics{gameSettings::gravity};
  engine::events::eventManager events;
  factory::GameEvents rules;
  engine::KeyboardState keyboard;
  std::int64_t tick = 0;
  std::vector<robotNet::LifeEvent> initialNotices;

  explicit EventRig(engine::ClientId owner = 1, bool ownsWorld = false)
      : rules(events, world, robot, owner, ownsWorld, grounded, progress) {
    factory::assignSpawn(scene, world, robot, owner);
    rules.start(scene, tick);
    initialNotices = rules.takeLifeEvents();
  }

  void key(engine::SC::SDL_Scancode code, bool pressed) {
    const auto previous = keyboard;
    keyboard.setKey(code, pressed);
    rules.input(scene, keyboard, previous, tick);
  }

  factory::TickResult step(float dt = 1.f / 60.f) {
    return rules.step(scene, physics, ++tick, dt);
  }

  void stand() {
    const auto surface = world.solids.front();
    scene.transform(robot).position = scene.transform(surface).position +
        glm::vec2{5.f, -scene.getCollider(robot)->size.y};
    scene.getRigidBody(robot)->velocity = {0.f, 0.f};
    grounded = true;
  }
};
