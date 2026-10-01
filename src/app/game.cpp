#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/graphics/hud.hpp>

#include <stdexcept>

Game::Game(std::optional<NetworkOptions> network, float initialSpeed)
    : window_("Robot Factory Escape | A/D: move | Space: jump | P: scaling | "
              "R: restart | T: pause",
              1280, 720),
      renderer_(window_) {
  renderer_.setScalingMode(engine::ScalingMode::Proportional);
  localSpawn_ = levelLayout::robotSpawn;
  createLevel();
  gameTime_.setSpeedMultiplier(initialSpeed);
  scene_.getText(timeText_)->val = "TIME: RUNNING " + speedLabel(initialSpeed);
  if (network)
    connectNetwork(*network);
  engine::log::info("Factory ready. Cyborg: player; red drone: hazard; cyan door: exit.");
  engine::log::info("A/D: move; Space: jump; P: scaling. Falling off-screen "
                    "resets the robot.");
  engine::log::info("Avoid the red patrol drone: contact returns you to the start.");
  engine::log::info("Reach the cyan factory exit to win. Press R to restart at any time.");
}
