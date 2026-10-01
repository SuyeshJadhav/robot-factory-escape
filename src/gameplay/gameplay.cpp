#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

#include <algorithm>
#include <utility>

void Game::restart(engine::Scene &scene) {
  if (clientSession_ || peerSession_) {
    // The coordinator keeps the shared drone clock running across local
    // restarts.
    resetRobot(scene, robot_, localSpawn_, grounded_);
    progress_.won = false;
  } else {
    progress_.restart(scene, robot_, drone_, localSpawn_, grounded_, dronePatrol_);
    platformPatrol_ = MovingPlatformPatrol{};
    platformPatrol_.advance(scene, movingPlatform_, 0.f);
  }
  sprites_.reset(scene, robot_, drone_);
  renderedWon_.store(false);
  setStatusText(scene, "STATUS: AVOID THE PATROL DRONE");
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
  grounded_ = playerIsSupported(scene, robot_, solids_);
  jumpIfGrounded(*scene.getRigidBody(robot_), grounded_, jumpPressed);
}

void Game::update(engine::Scene &scene, float deltaSeconds) {
  if (progress_.won)
    return;
  // Advance both movers on the same clock and check contact each small step.
  float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
  while (remaining > 0.f) {
    const float step = std::min(remaining, 1.f / 120.f);
    remaining -= step;
    if (!clientSession_ && !peerSession_) {
      dronePatrol_.advance(scene, drone_, step);
      const auto previousPlatform = scene.transform(movingPlatform_).position;
      platformPatrol_.advance(scene, movingPlatform_, step);
      carryPlayerWithPlatform(scene, robot_, movingPlatform_, previousPlatform, solids_, grounded_);
    }
    movePlayer(scene, physics_, robot_, solids_, step, grounded_);
    if (resetOnDroneContact(scene, physics_, robot_, drone_, localSpawn_, grounded_)) {
      sprites_.reset(scene, robot_, drone_);
      setStatusText(scene, "STATUS: DRONE HIT - TRY AGAIN");
      engine::log::info("Drone contact! Back to the start.");
      break;
    }
    if (scene.transform(robot_).position.y > levelLayout::levelSize.y) {
      resetRobot(scene, robot_, localSpawn_, grounded_);
      sprites_.reset(scene, robot_, drone_);
      setStatusText(scene, "STATUS: MISSED A JUMP - BACK TO START");
      break;
    }
    if (progress_.checkExit(scene, physics_, robot_, exit_)) {
      renderedWon_.store(true);
      setStatusText(scene, "ESCAPED! PRESS R TO PLAY AGAIN");
      engine::log::info("You escaped! Press R to play again.");
      break;
    }
  }
  sprites_.update(scene, robot_, grounded_, progress_.won, deltaSeconds);
}

void Game::setStatusText(engine::Scene &scene, std::string message) {
  if (auto *text = scene.getText(statusText_)) {
    text->val = std::move(message);
  }
}
