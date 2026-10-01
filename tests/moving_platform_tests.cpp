#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/gameplay/moving_platform.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < 0.02f; }
struct Fixture {
  engine::Scene scene;
  engine::PhysicsSystem physics{gameSettings::gravity};
  MovingPlatformPatrol patrol;
  engine::EntityId platform = box({330.f, 750.f}, {300.f, 24.f});
  engine::EntityId player = box({400.f, 730.f}, {20.f, 20.f});
  bool grounded = true;
  Fixture() { scene.addRigidBody(player); }
  engine::EntityId box(glm::vec2 position, glm::vec2 size) {
    const auto id = scene.createEntity();
    scene.transform(id).position = position;
    scene.addCollider(id, {.size = size});
    return id;
  }
  glm::vec2 &position() { return scene.transform(player).position; }
  void carryTo(float x) {
    const auto previous = scene.transform(platform).position;
    scene.transform(platform).position.x = x;
    carryPlayerWithPlatform(scene, player, platform, previous, std::array{platform}, grounded);
  }
};
} // namespace

int main() {
  try {
    {
      Fixture f;
      // Several full patrol reversals; an idle rider must retain its offset.
      for (int i = 0; i < 2400; ++i) {
        const auto previous = f.scene.transform(f.platform).position;
        f.patrol.advance(f.scene, f.platform, 1.f / 120.f);
        carryPlayerWithPlatform(f.scene, f.player, f.platform, previous, std::array{f.platform},
                                f.grounded);
        movePlayer(f.scene, f.physics, f.player, std::array{f.platform}, 1.f / 120.f, f.grounded);
        require(near(f.position().x - f.scene.transform(f.platform).position.x, 70.f),
                "idle rider drifted at patrol reversal");
        require(f.grounded && near(f.position().y, 730.f), "rider lost deck contact");
      }
      require(near(f.scene.getRigidBody(f.player)->velocity.x, 0.f),
              "carry changed voluntary horizontal velocity");
      require(jumpIfGrounded(*f.scene.getRigidBody(f.player), f.grounded, true),
              "cannot jump from moving platform");
      const float jumpX = f.position().x;
      f.carryTo(440.f);
      require(near(f.position().x, jumpX), "jumping player stuck to platform");
    }
    {
      Fixture f;
      f.scene.getRigidBody(f.player)->velocity.x = 300.f;
      f.carryTo(331.f);
      movePlayer(f.scene, f.physics, f.player, std::array{f.platform}, 1.f / 60.f, f.grounded);
      require(near(f.position().x, 406.f), "walking and carry did not add correctly");
      // Walking off releases support, even though the platform keeps moving.
      f.position().x = 630.f;
      movePlayer(f.scene, f.physics, f.player, std::array{f.platform}, 1.f / 60.f, f.grounded);
      require(!f.grounded, "walking off kept grounded state");
      const float x = f.position().x;
      f.carryTo(350.f);
      require(near(f.position().x, x), "airborne player carried");
    }
    {
      Fixture f;
      // Snapshot gaps and repeated polls must apply each delta only once.
      for (float x : {360.f, 420.f, 420.f, 450.f, 400.f, 330.f})
        f.carryTo(x);
      require(near(f.position().x, 400.f), "snapshot displacement accumulated incorrectly");
      // Paused updates change the world, not the player. No catch-up on resume.
      f.position().x = 340.f;
      f.scene.transform(f.platform).position.x = 450.f;
      f.grounded = playerIsSupported(f.scene, f.player, std::array{f.platform});
      require(!f.grounded, "stale support survived paused platform movement");
      require(!jumpIfGrounded(*f.scene.getRigidBody(f.player), f.grounded, true),
              "resume allowed a midair jump");
      f.carryTo(449.f);
      require(near(f.position().x, 340.f), "resume snapped robot to departed platform");
    }
    {
      Fixture f;
      // A large network displacement is swept against a wall, not teleported.
      const auto wall = f.box({440.f, 690.f}, {20.f, 60.f});
      const auto previous = f.scene.transform(f.platform).position;
      f.scene.transform(f.platform).position.x = 450.f;
      carryPlayerWithPlatform(f.scene, f.player, f.platform, previous, std::array{f.platform, wall},
                              f.grounded);
      require(near(f.position().x, 420.f), "rightward carry passed through wall");
      require(!f.grounded, "robot remained grounded after platform moved beyond wall");
    }
    {
      Fixture f;
      f.position().x = 500.f;
      f.scene.transform(f.platform).position.x = 450.f;
      const auto wall = f.box({460.f, 690.f}, {20.f, 60.f});
      const auto previous = f.scene.transform(f.platform).position;
      f.scene.transform(f.platform).position.x = 330.f;
      carryPlayerWithPlatform(f.scene, f.player, f.platform, previous, std::array{f.platform, wall},
                              f.grounded);
      require(near(f.position().x, 480.f), "leftward carry passed through wall");
    }
    for (glm::vec2 position : {glm::vec2{310.f, 740.f}, glm::vec2{400.f, 774.f}}) {
      Fixture f;
      f.position() = position;
      f.carryTo(340.f);
      require(near(f.position().x, position.x), "side or underside contact carried player");
    }
    std::cout << "Platform riding, detachment, pause, and snapshot collision "
                 "checks passed.\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
