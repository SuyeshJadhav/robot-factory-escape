#include "event_test_support.hpp"

#include <iostream>

int main() {
  return test::run([] {
    EventRig drone;
    drone.scene.transform(drone.robot).position = drone.scene.transform(drone.world.drone).position;
    drone.scene.getRigidBody(drone.robot)->velocity = {100.f, 100.f};
    test::require(drone.step() == factory::TickResult::DroneHit,
                  "drone_contact_returns_drone_hit");
    test::require(!drone.rules.alive() && !drone.scene.getShape(drone.robot) &&
                      !drone.scene.getSpriteAnimation(drone.robot),
                  "death event hides robot artwork");
    test::require(drone.scene.getCollider(drone.robot) &&
                      drone.scene.getRigidBody(drone.robot)->velocity == glm::vec2{0.f, 0.f} &&
                      !drone.grounded,
                  "death preserves camera bounds and stops local motion");
    const auto deadPosition = drone.scene.transform(drone.robot).position;
    drone.key(engine::SC::SDL_SCANCODE_D, true);
    test::require(drone.step() == factory::TickResult::None &&
                      drone.scene.transform(drone.robot).position == deadPosition,
                  "dead robot ignores movement and physics");
    auto notices = drone.rules.takeLifeEvents();
    test::require(notices.size() == 1 && notices[0].kind == robotNet::LifeKind::Death &&
                      notices[0].revision == 2,
                  "one death creates one owner lifecycle notice");

    EventRig pit;
    pit.scene.transform(pit.robot).position = {2100.f, 1000.f};
    test::require(pit.step() == factory::TickResult::DeathZone && !pit.rules.alive(),
                  "pit_contact_returns_death_zone");

    EventRig repeated;
    const engine::events::collisionEvent hit{{}, repeated.robot, repeated.world.drone};
    repeated.events.raise(hit, 10, -20);
    repeated.events.raise(hit, 10, -20);
    repeated.events.run(repeated.scene, 10);
    test::require(!repeated.rules.alive() && repeated.rules.revision() == 2 &&
                      repeated.rules.takeLifeEvents().size() == 1,
                  "duplicate same-tick collisions cause only one death");

    EventRig safe;
    safe.events.raise(engine::events::collisionEvent{{}, safe.robot, safe.world.solids.front()}, 0);
    safe.events.raise(engine::events::collisionEvent{{}, safe.world.drone, safe.world.solids.front()}, 0);
    safe.events.raise(engine::events::deathEvent{{}, safe.world.drone, safe.robot}, 0);
    safe.events.run(safe.scene, 0);
    test::require(safe.rules.alive() && safe.rules.revision() == 1 &&
                      safe.rules.takeLifeEvents().empty(),
                  "unrelated and nonhazard contacts cannot kill the local robot");
    std::cout << "event lifecycle checks passed\n";
  });
}
