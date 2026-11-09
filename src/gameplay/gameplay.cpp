#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

#include <algorithm>
#include <utility>


void Game::update(engine::Scene &scene, float deltaSeconds) {
  const bool wasAlive = rules_->alive();
  const auto result = rules_->step(scene, physics_, lastSimulationTick_, deltaSeconds);
  if (!wasAlive && rules_->alive())
    sprites_.reset(scene, robot_, world_.drone);
  switch (result) {
  case factory::TickResult::DroneHit:
    setStatusText(scene,"STATUS: DRONE HIT - TRY AGAIN");
    break;
  case factory::TickResult::DeathZone:
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
  rules_->takeLifeEvents();
}

void Game::setStatusText(engine::Scene &scene, std::string message) {
  if (auto *text = scene.getText(statusText_)) {
    text->val = std::move(message);
  }
}
