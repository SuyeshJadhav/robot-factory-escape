#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

#include <algorithm>
#include <utility>

void Game::restart(engine::Scene &scene) {
  factory::restartPlayer(scene,robot_,grounded_,progress_,!clientSession_ && !peerSession_);
  sprites_.reset(scene,robot_,world_.drone);
  renderedWon_.store(false);
  setStatusText(scene,"STATUS: AVOID THE PATROL DRONE");
  engine::log::info("Restarted. Reach the exit!");
}

void Game::handleInput(engine::Scene &scene, const engine::KeyboardState &keyboard) {

  const bool jumpKeyIsPressed = keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_SPACE);
  const bool jumpPressed = jumpKeyIsPressed && !jumpKeyWasPressed_;
  jumpKeyWasPressed_ = jumpKeyIsPressed;
  if (progress_.won)
    return;

  const bool moveLeft = keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_A) ||
                        keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_LEFT);
  const bool moveRight = keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_D) ||
                         keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_RIGHT);
  const float direction = static_cast<float>(moveRight) - static_cast<float>(moveLeft);
  // Input sets speed; physics applies elapsed time and changes position.
  scene.getRigidBody(robot_)->velocity.x = direction * gameSettings::robotSpeed;
  // A server platform may have moved away during pause; do not allow a stale
  // grounded flag to grant a midair jump on the first resumed tick.
  grounded_ = playerIsSupported(scene, robot_, world_.solids);
  jumpIfGrounded(*scene.getRigidBody(robot_), grounded_, jumpPressed);
}

void Game::update(engine::Scene &scene, float deltaSeconds) {
  if (progress_.won) return;
  const auto result = factory::tickPlayer(scene,physics_,world_,robot_,grounded_,progress_,
                                          deltaSeconds,!clientSession_ && !peerSession_);
  switch (result) {
  case factory::TickResult::DroneHit:
    sprites_.reset(scene,robot_,world_.drone);
    setStatusText(scene,"STATUS: DRONE HIT - TRY AGAIN");
    break;
  case factory::TickResult::DeathZone:
    sprites_.reset(scene,robot_,world_.drone);
    setStatusText(scene,"STATUS: DEATH ZONE - BACK TO SPAWN");
    break;
  case factory::TickResult::Won:
    renderedWon_.store(true);
    setStatusText(scene,"ESCAPED! PRESS R TO PLAY AGAIN");
    break;
  case factory::TickResult::None:
    break;
  }
  sprites_.update(scene,robot_,grounded_,progress_.won,deltaSeconds);
}

void Game::setStatusText(engine::Scene &scene, std::string message) {
  if (auto *text = scene.getText(statusText_)) {
    text->val = std::move(message);
  }
}
