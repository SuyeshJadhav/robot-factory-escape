#include "event_test_support.hpp"

#include <iostream>

int main() {
  using namespace engine::SC;
  EventRig f;
  f.key(SDL_SCANCODE_D, true);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == gameSettings::robotSpeed,
                "D input event starts rightward movement");
  const auto moved = f.scene.transform(f.robot).position.x;
  f.step();
  test::require(f.scene.transform(f.robot).position.x > moved,
                "held event state persists without repeated press events");
  f.key(SDL_SCANCODE_D, false);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == 0.f,
                "release event stops movement");
  f.key(SDL_SCANCODE_A, true);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == -gameSettings::robotSpeed,
                "A input event moves left");
  f.key(SDL_SCANCODE_D, true);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == 0.f,
                "opposing held directions cancel");
  f.key(SDL_SCANCODE_A, false);
  f.key(SDL_SCANCODE_D, false);
  f.key(SDL_SCANCODE_LEFT, true);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == -gameSettings::robotSpeed,
                "left arrow aliases A");
  f.key(SDL_SCANCODE_LEFT, false);
  f.key(SDL_SCANCODE_RIGHT, true);
  f.step();
  test::require(f.scene.getRigidBody(f.robot)->velocity.x == gameSettings::robotSpeed,
                "right arrow aliases D");
  EventRig jump;
  jump.stand();
  jump.key(SDL_SCANCODE_SPACE, true);
  jump.step();
  test::require(jump.scene.getRigidBody(jump.robot)->velocity.y < 0.f,
                "fresh grounded Space event jumps");
  jump.stand();
  jump.step();
  test::require(jump.scene.getRigidBody(jump.robot)->velocity.y == 0.f,
                "holding Space cannot trigger a second grounded jump");
  jump.key(SDL_SCANCODE_SPACE, false);
  jump.key(SDL_SCANCODE_SPACE, true);
  jump.step();
  test::require(jump.scene.getRigidBody(jump.robot)->velocity.y < 0.f,
                "release and fresh press allow another supported jump");
  EventRig airborne;
  airborne.grounded = true; // Deliberately stale: actual support must be checked.
  airborne.key(SDL_SCANCODE_SPACE, true);
  airborne.step();
  test::require(airborne.scene.getRigidBody(airborne.robot)->velocity.y >= 0.f,
                "stale grounded state cannot grant an airborne jump");
  airborne.stand();
  airborne.step();
  test::require(airborne.scene.getRigidBody(airborne.robot)->velocity.y == 0.f,
                "failed airborne jump request is not deferred until landing");
  EventRig blocked;
  blocked.stand();
  blocked.rules.setGameplayBlocked(true);
  blocked.key(SDL_SCANCODE_SPACE, true);
  blocked.key(SDL_SCANCODE_SPACE, false);
  blocked.rules.setGameplayBlocked(false);
  blocked.stand();
  blocked.step();
  test::require(blocked.scene.getRigidBody(blocked.robot)->velocity.y == 0.f,
                "space_tap_while_blocked_does_not_jump_after_unblock");
  blocked.key(SDL_SCANCODE_SPACE, true);
  blocked.rules.setGameplayBlocked(true);
  blocked.rules.setGameplayBlocked(false);
  blocked.stand();
  blocked.step();
  test::require(blocked.scene.getRigidBody(blocked.robot)->velocity.y == 0.f,
                "pending jump is discarded when gameplay becomes blocked");
  blocked.key(SDL_SCANCODE_SPACE, false);
  blocked.key(SDL_SCANCODE_SPACE, true);
  blocked.step();
  test::require(blocked.scene.getRigidBody(blocked.robot)->velocity.y < 0.f,
                "fresh press after unblocking jumps normally");
  EventRig paused;
  paused.stand();
  paused.key(SDL_SCANCODE_D, true);
  test::require(paused.scene.getRigidBody(paused.robot)->velocity.x == 0.f &&
                    paused.scene.transform(paused.robot).position.x ==
                        paused.scene.transform(paused.world.solids.front()).position.x + 5.f,
                "queued input is processed without a physics tick");
  std::cout << "event input checks passed\n";
}
