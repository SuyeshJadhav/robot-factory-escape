#include "test_support.hpp"
#include <robot_factory_escape/gameplay/factory_world.hpp>
#include <algorithm>
#include <tuple>
#include <set>

int main() {
  return test::run([] {
    engine::Scene scene;
    const auto world = factory::createWorld(scene);
    test::require(scene.all<factory::SpawnPoint>().size() == 2, "two spawn objects");
    test::require(scene.all<factory::DeathZone>().size() == 4, "pit and perimeter death objects");
    for (const auto spawn : world.spawns) {
      test::require(scene.has<factory::SpawnPoint>(spawn), "tagged spawn");
      test::require(!scene.getShape(spawn) && !scene.getText(spawn) &&
                    !scene.getSpriteAnimation(spawn) && !scene.getCollider(spawn),
                    "spawn is hidden and non-solid");
    }
    for (const auto &[zone, tag] : scene.all<factory::DeathZone>()) {
      test::require(scene.getCollider(zone), "death zone has collision bounds");
      test::require(!scene.getShape(zone) && !scene.getText(zone) &&
                    !scene.getSpriteAnimation(zone), "death zone is hidden");
      test::require(std::find(world.solids.begin(), world.solids.end(), zone) ==
                    world.solids.end(), "triggers are not solid surfaces");
    }
    std::set<std::tuple<float,float,float,float>> staticSpecs;
    for (const auto platform : world.platforms) {
      if (platform == world.movingPlatform) continue;
      const auto p = scene.transform(platform).position;
      const auto size = scene.getShape(platform)->size;
      staticSpecs.emplace(p.x,p.y,size.x,size.y);
      test::require(scene.has<factory::Solid>(platform), "static platform is solid");
    }
    test::require(staticSpecs.size() >= 3, "three distinct static platform combinations");
    test::require(scene.transform(world.exit).position.x > 1920.f, "goal requires scrolling");
    test::require(scene.rigidBodies().empty(), "world movers do not receive gravity");
    const auto robot = factory::createPlayer(scene, world);
    test::require(scene.getRigidBody(robot) && scene.getCollider(robot), "playable robot");
    test::require(scene.get<factory::RespawnTarget>(robot)->spawn == world.spawns[0],
                  "offline player designated spawn");
    test::require(scene.transform(robot).position.x == 120.f &&
                  scene.transform(robot).position.y == 300.f, "offline starting position");
    const auto copy = scene;
    test::require(copy.has<factory::SpawnPoint>(world.spawns[1]) &&
                  copy.get<factory::RespawnTarget>(robot)->spawn == world.spawns[0],
                  "render/simulation copies retain custom components");
  });
}
