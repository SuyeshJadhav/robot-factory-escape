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
    EventRig delayed(2);
    test::require(delayed.rules.alive() && delayed.rules.revision() == 1 &&
                      delayed.initialNotices.size() == 1 &&
                      delayed.initialNotices[0].kind == robotNet::LifeKind::Spawn &&
                      delayed.initialNotices[0].position == glm::vec2{360.f, 300.f},
                  "initial_spawn_is_one_event_at_assigned_owner_spawn");
    delayed.scene.addSpriteAnimation(delayed.robot,
        engine::SpriteAnimation{0, {{0, 0.2f}, {1, 0.2f}}, true, 1, 0.05f});
    delayed.events.raise(engine::events::collisionEvent{{}, delayed.robot, delayed.world.drone},
                          10, -20);
    delayed.events.run(delayed.scene, 10);
    delayed.rules.takeLifeEvents();
    delayed.events.run(delayed.scene, 69);
    test::require(!delayed.rules.alive() && delayed.rules.revision() == 2,
                  "dead robot waits through tick 69");
    delayed.events.run(delayed.scene, 70);
    const auto spawned = delayed.rules.takeLifeEvents();
    test::require(delayed.rules.alive() && delayed.rules.revision() == 3 &&
                      delayed.scene.transform(delayed.robot).position == glm::vec2{360.f, 300.f} &&
                      delayed.scene.getRigidBody(delayed.robot)->velocity == glm::vec2{0.f, 0.f} &&
                      delayed.scene.getShape(delayed.robot) &&
                      delayed.scene.getSpriteAnimation(delayed.robot) &&
                      delayed.scene.getSpriteAnimation(delayed.robot)->currentFrame == 1 &&
                      spawned.size() == 1 && spawned[0].kind == robotNet::LifeKind::Spawn,
                  "delayed spawn restores visuals and assigned pose exactly at tick 70");
    delayed.tick = 70;
    delayed.step();
    delayed.step();
    test::require(delayed.rules.takeLifeEvents().empty(), "spawn is not repeated on later ticks");

    EventRig deadTap;
    deadTap.stand();
    const auto supportedSpawn = deadTap.scene.transform(deadTap.robot).position;
    deadTap.scene.transform(deadTap.scene.get<factory::RespawnTarget>(deadTap.robot)->spawn).position =
        supportedSpawn;
    deadTap.events.raise(engine::events::deathEvent{{}, deadTap.robot, deadTap.world.drone}, 10);
    deadTap.events.run(deadTap.scene, 10);
    deadTap.tick = 20;
    deadTap.key(engine::SC::SDL_SCANCODE_SPACE, true);
    deadTap.key(engine::SC::SDL_SCANCODE_SPACE, false);
    deadTap.events.run(deadTap.scene, 70);
    deadTap.tick = 70;
    test::require(deadTap.rules.alive() &&
                      deadTap.scene.transform(deadTap.robot).position == supportedSpawn,
                  "dead-input fixture respawns on its stable supported surface");
    deadTap.step();
    test::require(deadTap.scene.getRigidBody(deadTap.robot)->velocity.y == 0.f,
                  "space_tap_while_dead_does_not_jump_on_respawn");
    std::cout << "event lifecycle checks passed\n";
  });
}
