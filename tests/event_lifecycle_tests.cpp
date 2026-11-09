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
    EventRig restarted(2);
    const auto serverPlatform = restarted.scene.transform(restarted.world.movingPlatform).position;
    restarted.events.raise(engine::events::deathEvent{{}, restarted.robot, restarted.world.drone}, 10);
    restarted.events.run(restarted.scene, 10);
    restarted.rules.takeLifeEvents();
    restarted.progress.won = true;
    restarted.tick = 20;
    restarted.key(engine::SC::SDL_SCANCODE_R, true);
    test::require(restarted.rules.alive() && !restarted.progress.won &&
                      restarted.scene.transform(restarted.robot).position == glm::vec2{360.f, 300.f},
                  "restart through input works during dead interval without a physics tick");
    const auto resetNotice = restarted.rules.takeLifeEvents();
    test::require(resetNotice.size() == 1 && resetNotice[0].revision == 3 &&
                      resetNotice[0].kind == robotNet::LifeKind::Spawn &&
                      restarted.scene.transform(restarted.world.movingPlatform).position == serverPlatform,
                  "client restart publishes only spawn and preserves server-owned world");
    restarted.scene.transform(restarted.robot).position = {700.f, 500.f};
    restarted.events.run(restarted.scene, 70);
    test::require(restarted.scene.transform(restarted.robot).position == glm::vec2{700.f, 500.f} &&
                      restarted.rules.takeLifeEvents().empty(),
                  "old delayed respawn cannot teleport restarted robot");
    EventRig owned(0, true);
    const auto initialPlatform = owned.scene.transform(owned.world.movingPlatform).position;
    const auto initialDrone = owned.scene.transform(owned.world.drone).position;
    factory::advanceWorld(owned.scene, 0.5f);
    test::require(owned.scene.transform(owned.world.movingPlatform).position != initialPlatform,
                  "offline restart fixture advances the owned world");
    owned.progress.won = true;
    owned.key(engine::SC::SDL_SCANCODE_R, true);
    test::require(!owned.progress.won && owned.rules.alive() &&
                      owned.scene.transform(owned.world.movingPlatform).position == initialPlatform &&
                      owned.scene.transform(owned.world.drone).position == initialDrone,
                  "offline restart resets its owned world");
    std::cout << "event lifecycle checks passed\n";
  });
}
