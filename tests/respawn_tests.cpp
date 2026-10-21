#include "test_support.hpp"
#include <robot_factory_escape/gameplay/respawn.hpp>

int main() {
  return test::run([] {
    engine::Scene scene;
    const auto w = factory::createWorld(scene);
    const auto robot = factory::createPlayer(scene,w);
    engine::PhysicsSystem physics(1800.f);
    bool grounded = true;
    const float expectedX[] = {120.f,120.f,360.f,120.f,360.f};
    for (engine::ClientId client = 0; client < 5; ++client) {
      factory::assignSpawn(scene,w,robot,client);
      scene.getRigidBody(robot)->velocity = {300.f,900.f};
      grounded = true;
      factory::respawnPlayer(scene,robot,grounded);
      test::require(scene.transform(robot).position.x == expectedX[client] &&
                    scene.transform(robot).position.y == 300.f, "explicit spawn assignment");
      test::require(scene.getRigidBody(robot)->velocity == glm::vec2{0.f,0.f} && !grounded,
                    "respawn clears velocity and support");
    }
    test::require(!factory::inDeathZone(scene,physics,robot), "spawn is safe");
    scene.transform(robot).position = {2100.f,990.f};
    test::require(factory::inDeathZone(scene,physics,robot), "AABB enters pit death zone");
    scene.transform(robot).position = {2050.f,850.f};
    test::require(!factory::inDeathZone(scene,physics,robot), "above pit is not a death");
    scene.transform(robot).position = {-1.f,300.f};
    test::require(factory::inDeathZone(scene,physics,robot), "left perimeter");
    scene.transform(robot).position = {3190.f,300.f};
    test::require(factory::inDeathZone(scene,physics,robot), "right perimeter");
    scene.transform(robot).position = {500.f,1100.f};
    test::require(factory::inDeathZone(scene,physics,robot), "bottom perimeter");
    scene.destroyEntity(scene.get<factory::RespawnTarget>(robot)->spawn);
    const auto before = scene.transform(robot).position;
    bool threw = false;
    try { factory::respawnPlayer(scene,robot,grounded); }
    catch (const std::logic_error &) { threw = true; }
    test::require(threw && scene.transform(robot).position == before,
                  "invalid target fails before changing player state");
  });
}
