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
  std::cout << "event input checks passed\n";
}
