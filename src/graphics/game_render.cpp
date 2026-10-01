#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>

void Game::render() {
  renderer_.clear({0, 0, 0, 255});
  // Draw the backdrop first, outside the unordered entity collection.
  renderer_.drawTexture(backdropTexture_, {0.f, 0.f}, levelLayout::levelSize);
  // A quiet backing keeps the controls legible against the bright factory art.
  renderer_.fillRect({16.f, 12.f}, {810.f, 212.f}, {7, 24, 31, 255});
  renderer_.fillRect({16.f, 12.f}, {4.f, 212.f}, {80, 255, 220, 255});
  drawExit();
  const auto platformPosition = scene_.transform(movingPlatform_).position;
  // Highlight the actual deck without filling the sprite's transparent margins.
  const engine::Color deckLight{80, 255, 220, 255};
  renderer_.fillRect(platformPosition + glm::vec2{0.f, -3.f}, {levelLayout::platformSize.x, 3.f},
                     deckLight);
  renderer_.fillRect(platformPosition, {4.f, 12.f}, deckLight);
  renderer_.fillRect(platformPosition + glm::vec2{levelLayout::platformSize.x - 4.f, 0.f},
                     {4.f, 12.f}, deckLight);
  // Gold markers distinguish other players despite the renderer using local
  // sprite art.
  for (const auto &[id, shape] : scene_.shapes()) {
    if (id == robot_ || id == drone_ || !scene_.getSpriteAnimation(id))
      continue;
    if (shape.color.r == 255 && shape.color.g == 210 && shape.color.b == 60) {
      const auto position = scene_.transform(id).position;
      renderer_.fillRect(position + glm::vec2{-3.f, -3.f}, {102.f, 134.f}, {255, 210, 60, 255});
    }
  }
  renderer_.drawEntities(scene_);
  renderer_.present();
}

void Game::drawExit() {
  const auto position = scene_.transform(exit_).position;
  const engine::Color frame =
      renderedWon_.load() ? engine::Color{80, 255, 140, 255} : engine::Color{50, 220, 235, 255};
  const engine::Color indicator =
      renderedWon_.load() ? engine::Color{80, 255, 100, 255} : engine::Color{255, 170, 35, 255};

  // A compact cyberpunk factory door built from engine rectangles.
  renderer_.fillRect(position + glm::vec2{-18.f, -20.f}, {136.f, 160.f}, {7, 18, 27, 255});
  renderer_.fillRect(position + glm::vec2{-12.f, -14.f}, {124.f, 154.f}, frame);
  renderer_.fillRect(position + glm::vec2{-5.f, -7.f}, {110.f, 147.f}, {12, 29, 39, 255});
  renderer_.fillRect(position + glm::vec2{3.f, 2.f}, {94.f, 138.f}, {23, 46, 57, 255});
  renderer_.fillRect(position + glm::vec2{48.f, 2.f}, {4.f, 138.f}, frame);

  // Hazard rails and a status panel tie the door to the orange/cyan factory
  // palette.
  renderer_.fillRect(position + glm::vec2{-12.f, 8.f}, {7.f, 22.f}, {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{-12.f, 42.f}, {7.f, 22.f}, {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{105.f, 8.f}, {7.f, 22.f}, {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{105.f, 42.f}, {7.f, 22.f}, {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{27.f, 14.f}, {46.f, 25.f}, {5, 15, 22, 255});
  renderer_.fillRect(position + glm::vec2{34.f, 20.f}, {32.f, 13.f}, indicator);
  renderer_.fillRect(position + glm::vec2{76.f, 76.f}, {8.f, 22.f}, indicator);
}
