#include "test_support.hpp"
#include <robot_factory_escape/gameplay/factory_world.hpp>
#include <numbers>

int main() {
  return test::run([] {
    engine::Scene scene;
    const auto w = factory::createWorld(scene);
    factory::advanceWorld(scene, 1.f);
    test::require(test::near(scene.transform(w.movingPlatform).position.x,390.f),
                  "horizontal platform advances at 60 pixels per second");
    test::require(scene.transform(w.movingPlatform).position.y == 750.f,
                  "horizontal patrol preserves height");
    factory::resetWorld(scene);
    factory::advanceWorld(scene, std::numbers::pi_v<float> / 2.f);
    test::require(test::near(scene.transform(w.drone).position.x,1100.f) &&
                  test::near(scene.transform(w.drone).position.y,600.f), "quarter circle");
    factory::advanceWorld(scene, std::numbers::pi_v<float> / 2.f);
    test::require(test::near(scene.transform(w.drone).position.x,940.f) &&
                  test::near(scene.transform(w.drone).position.y,440.f), "half circle");
    const auto before = scene.transform(w.drone).position;
    factory::advanceWorld(scene, 0.f);
    factory::advanceWorld(scene, -1.f);
    test::require(scene.transform(w.drone).position == before, "nonpositive dt is inert");
    factory::resetWorld(scene);
    factory::advanceWorld(scene, 3.f);
    test::require(test::near(scene.transform(w.movingPlatform).position.x,390.f),
                  "platform reverses at endpoint");
    factory::resetWorld(scene);
    test::require(scene.transform(w.movingPlatform).position.x == 330.f &&
                  test::near(scene.transform(w.drone).position.x,1260.f), "offline rewind");
    factory::resetWorld(scene);
    factory::advanceWorld(scene,2.f * std::numbers::pi_v<float>);
    test::require(test::near(scene.transform(w.drone).position.x,1260.f) &&
                  test::near(scene.transform(w.drone).position.y,440.f), "full orbit returns to start");
    for (int i = 0; i < 10000; ++i) {
      factory::advanceWorld(scene,1.f / 120.f);
      const auto p = scene.transform(w.drone).position;
      test::require(test::near(std::hypot(p.x - 1100.f,p.y - 440.f),160.f),
                    "circular drone stays on its configured radius over repeated ticks");
      const auto platformX = scene.transform(w.movingPlatform).position.x;
      test::require(platformX >= MovingPlatformPatrol::left &&
                    platformX <= MovingPlatformPatrol::right, "horizontal mover stays in bounds");
    }
    test::require(scene.rigidBodies().empty(), "patterned movers remain kinematic");
  });
}
