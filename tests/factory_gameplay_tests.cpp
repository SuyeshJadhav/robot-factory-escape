#include "test_support.hpp"
#include <robot_factory_escape/gameplay/factory_gameplay.hpp>

namespace {
void offlineTickAdvancesAndCarries() {
  engine::Scene scene;
  const auto world = factory::createWorld(scene);
  const auto robot = factory::createPlayer(scene,world);
  engine::PhysicsSystem physics(1800.f);
  GameProgress progress;
  bool grounded = true;
  const auto oldPlatform = scene.transform(world.movingPlatform).position;
  const auto oldDrone = scene.transform(world.drone).position;
  const auto oldRobot = oldPlatform + glm::vec2{30.f,-128.f};
  scene.transform(robot).position = oldRobot;
  scene.getRigidBody(robot)->velocity = {0.f,0.f};
  constexpr float dt = 1.f / 60.f;
  const auto result = factory::tickPlayer(scene,physics,world,robot,grounded,progress,dt,true);
  const auto platform = scene.transform(world.movingPlatform).position;
  const auto drone = scene.transform(world.drone).position;
  test::require(result == factory::TickResult::None &&
                test::near(platform.x,oldPlatform.x + 60.f * dt) &&
                platform.y == oldPlatform.y, "offline production tick advances platform");
  test::require(drone.y > oldDrone.y && drone.x < oldDrone.x &&
                scene.get<factory::Orbit>(world.drone)->angle > 0.f,
                "offline production tick advances circular drone");
  test::require(test::near(scene.transform(robot).position.x,oldRobot.x + 60.f * dt) &&
                test::near(scene.transform(robot).position.y,oldRobot.y) && grounded,
                "offline production tick carries a supported robot");
  test::require(scene.getRigidBody(robot)->velocity == glm::vec2{0.f,0.f},
                "carry does not become voluntary player velocity");
}

void clientTickLeavesSharedMotionUntouched() {
  engine::Scene scene;
  const auto world = factory::createWorld(scene);
  const auto robot = factory::createPlayer(scene,world);
  // Non-initial state makes accidental resets visible as well as advancement.
  factory::advanceWorld(scene,0.25f);
  const auto platform = scene.transform(world.movingPlatform).position;
  const auto drone = scene.transform(world.drone).position;
  const auto distance = scene.get<MovingPlatformPatrol>(world.movingPlatform)->distance;
  const auto angle = scene.get<factory::Orbit>(world.drone)->angle;
  const auto oldRobot = platform + glm::vec2{30.f,-128.f};
  scene.transform(robot).position = oldRobot;
  scene.getRigidBody(robot)->velocity = {300.f,0.f};
  engine::PhysicsSystem physics(1800.f);
  GameProgress progress;
  bool grounded = true;
  constexpr float dt = 1.f / 60.f;
  const auto result = factory::tickPlayer(scene,physics,world,robot,grounded,progress,dt,false);
  test::require(result == factory::TickResult::None &&
                test::near(scene.transform(robot).position.x,oldRobot.x + 300.f * dt) && grounded,
                "client production tick moves its own robot without false hazard contact");
  test::require(scene.transform(world.movingPlatform).position == platform &&
                scene.transform(world.drone).position == drone &&
                scene.get<MovingPlatformPatrol>(world.movingPlatform)->distance == distance &&
                scene.get<factory::Orbit>(world.drone)->angle == angle,
                "client tick cannot advance or reset server-owned motion");
}
}

int main() {
  return test::run([] {
    offlineTickAdvancesAndCarries();
    clientTickLeavesSharedMotionUntouched();
    engine::Scene scene;
    const auto w = factory::createWorld(scene);
    const auto robot = factory::createPlayer(scene,w);
    engine::PhysicsSystem physics(1800.f);
    GameProgress progress;
    bool grounded = true;
    factory::assignSpawn(scene,w,robot,2);
    scene.transform(robot).position = {2100.f,1000.f};
    scene.getRigidBody(robot)->velocity = {100.f,100.f};
    auto result = factory::tickPlayer(scene,physics,w,robot,grounded,progress,1.f/120.f,false);
    test::require(result == factory::TickResult::DeathZone &&
                  scene.transform(robot).position == glm::vec2{360.f,300.f} &&
                  scene.getRigidBody(robot)->velocity == glm::vec2{0.f,0.f} && !grounded,
                  "production tick respawns on trigger, not on a Y-only fallback");
    scene.transform(robot).position = scene.transform(w.drone).position;
    result = factory::tickPlayer(scene,physics,w,robot,grounded,progress,1.f/120.f,false);
    test::require(result == factory::TickResult::DroneHit &&
                  scene.transform(robot).position.x == 360.f, "drone uses assigned spawn");
    scene.transform(robot).position = {3000.f,410.f};
    result = factory::tickPlayer(scene,physics,w,robot,grounded,progress,1.f/120.f,false);
    test::require(result == factory::TickResult::Won && progress.won, "new exit is functional");
    const auto wonPosition = scene.transform(robot).position;
    result = factory::tickPlayer(scene,physics,w,robot,grounded,progress,0.1f,false);
    test::require(result == factory::TickResult::None &&
                  scene.transform(robot).position == wonPosition, "win stays frozen");
    factory::advanceWorld(scene,1.f);
    const auto platform = scene.transform(w.movingPlatform).position;
    const auto drone = scene.transform(w.drone).position;
    scene.getRigidBody(robot)->velocity = {100.f,150.f};
    grounded = true;
    factory::restartPlayer(scene,robot,grounded,progress,false);
    test::require(!progress.won && !grounded &&
                  scene.getRigidBody(robot)->velocity == glm::vec2{0.f,0.f} &&
                  scene.transform(robot).position == glm::vec2{360.f,300.f},
                  "restart clears completion, velocity, support and uses assigned spawn");
    test::require(scene.transform(w.movingPlatform).position == platform &&
                  scene.transform(w.drone).position == drone, "network restart leaves world alone");
    factory::restartPlayer(scene,robot,grounded,progress,true);
    test::require(scene.transform(w.movingPlatform).position.x == 330.f &&
                  test::near(scene.transform(w.drone).position.x,1260.f), "offline restart rewinds");
    scene.transform(robot).position = {3000.f,410.f};
    result = factory::tickPlayer(scene,physics,w,robot,grounded,progress,1.f/120.f,false);
    test::require(result == factory::TickResult::Won && progress.won &&
                  scene.getRigidBody(robot)->velocity == glm::vec2{0.f,0.f},
                  "production path can win again after restart");
    factory::restartPlayer(scene,robot,grounded,progress,false);
    const auto zeroPosition = scene.transform(robot).position;
    factory::tickPlayer(scene,physics,w,robot,grounded,progress,0.f,true);
    test::require(scene.transform(robot).position == zeroPosition, "zero delta does not move");
    // The real level's floor gap must be crossable using current physics.
    scene.transform(robot).position = {2000.f,812.f};
    scene.getRigidBody(robot)->velocity = {300.f,-880.f};
    grounded = false;
    for (int i = 0; i < 145; ++i)
      factory::tickPlayer(scene,physics,w,robot,grounded,progress,1.f/120.f,false);
    test::require(scene.transform(robot).position.x > 2320.f && grounded,
                  "factory pit is traversable, not an unavoidable death");
  });
}
