#include "test_support.hpp"
#include <robot_factory_escape/gameplay/game_progress.hpp>

int main() {
  return test::run([] {
    engine::Scene scene;
    engine::PhysicsSystem physics{980.f};
    const auto robot = scene.createEntity();
    scene.addRigidBody(robot, {.velocity = {300.f, 100.f}});
    scene.addCollider(robot, {.size = {64.f, 80.f}});
    const auto exit = scene.createEntity();
    scene.transform(exit).position = {1740.f, 800.f};
    scene.addCollider(exit, {.size = {100.f, 140.f}});
    GameProgress progress;
    test::require(!progress.checkExit(scene, physics, robot, exit) && !progress.won,
                  "no win away from exit");
    scene.transform(robot).position = {1730.f, 860.f};
    test::require(progress.checkExit(scene, physics, robot, exit) && progress.won,
                  "exit contact wins");
    test::require(scene.getRigidBody(robot)->velocity == glm::vec2{0.f, 0.f},
                  "win stops robot velocity");
    test::require(!progress.checkExit(scene, physics, robot, exit), "win fires only once");
  });
}
