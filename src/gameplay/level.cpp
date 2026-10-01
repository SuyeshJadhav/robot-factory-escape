#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>

#include <string>
#include <utility>

engine::EntityId Game::createBox(glm::vec2 position, glm::vec2 size, engine::Color color) {
  const auto id = scene_.createEntity();
  scene_.transform(id).position = position;
  scene_.addShape(id, {.size = size, .color = color, .texture = std::nullopt});
  scene_.addCollider(id, {.size = size});
  return id;
}

void Game::createLevel() {
  const std::string assetDir = GAME_ASSET_DIR;
  backdropTexture_ =
      renderer_.loadTexture(assetDir + "background/cyberpunk_background_backdropa_idle.png");
  const auto platformTexture =
      renderer_.loadTexture(assetDir + "platform/cyberpunk_platform_antigravcart_idle.png");

  const auto hudFont = renderer_.loadFont(GAME_FONT_PATH, 24.f);
  const auto titleFont = renderer_.loadFont(GAME_FONT_PATH, 34.f);
  const auto createText = [this](glm::vec2 position, std::string value, engine::FontId font,
                                 engine::Color color) {
    const auto id = scene_.createEntity();
    scene_.transform(id).position = position;
    scene_.addText(id, {.val = std::move(value), .font = font, .color = color});
    return id;
  };
  createText({32.f, 18.f}, "ROBOT FACTORY ESCAPE", titleFont, {80, 255, 220, 255});
  createText({32.f, 62.f},
             "A/D OR ARROWS MOVE  SPACE JUMP  R RESTART  T PAUSE  1/2/3 SPEED  "
             "P SCALE",
             hudFont, {235, 240, 255, 255});
  createText({32.f, 94.f}, "WATCH THE MOVING PLATFORM  ->  REACH THE EXIT", hudFont,
             {255, 190, 55, 255});
  statusText_ =
      createText({32.f, 126.f}, "STATUS: AVOID THE PATROL DRONE", hudFont, {255, 110, 120, 255});
  networkText_ = createText({32.f, 158.f}, "NETWORK: OFFLINE", hudFont, {80, 255, 220, 255});
  timeText_ = createText({32.f, 190.f}, "TIME: RUNNING 1X", hudFont, {255, 190, 55, 255});
  fpsText_ = createText({630.f, 190.f}, "FPS: --", hudFont, {180, 210, 220, 255});

  // Most boxes have matching visual and collision bounds, with scale left at 1.
  floor_ = createBox({0.f, 940.f}, {1920.f, 140.f}, {70, 78, 91, 255});
  solids_.push_back(floor_);
  robot_ = createBox(levelLayout::robotSpawn, levelLayout::robotSize, {72, 205, 230, 255});
  // The cyborg is narrower than its animation cell; keep the full visual height
  // while tightening horizontal collision to the character's body.
  scene_.addCollider(robot_, {.size = levelLayout::robotColliderSize});

  // Reuse one platform sheet for a staircase of reachable Mario-like jumps.
  // The collider covers only the solid deck, not the transparent machinery
  // below it.
  engine::SpriteSheetLayout platformLayout;
  platformLayout.frames.push_back({{0.f, 0.f}, {1024.f, 224.f}});
  const auto platformSheet =
      renderer_.createSpriteSheet(platformTexture, std::move(platformLayout));
  for (std::size_t i = 0; i < levelLayout::platformPositions.size(); ++i) {
    const auto position = levelLayout::platformPositions[i];
    const auto platform = scene_.createEntity();
    if (i == 0)
      movingPlatform_ = platform;
    scene_.transform(platform).position = position;
    scene_.addShape(platform, {.size = levelLayout::platformSize,
                               .color = {255, 255, 255, 255},
                               .texture = std::nullopt});
    scene_.addCollider(platform, {.size = levelLayout::platformColliderSize});
    scene_.addSpriteAnimation(platform, engine::SpriteAnimation::uniform(platformSheet, {0}, 1.f));
    solids_.push_back(platform);
  }

  drone_ = createBox({0.f, 0.f}, levelLayout::droneSize, {235, 85, 93, 255});
  dronePatrol_.advance(scene_, drone_, 0.f);
  // The trigger remains a simple collider; drawExit provides the themed
  // artwork.
  exit_ = scene_.createEntity();
  // The exit sits on the final platform, so walking across the floor is not
  // enough.
  scene_.transform(exit_).position = {1740.f, levelLayout::platformPositions.back().y - 140.f};
  scene_.addCollider(exit_, {.size = {100.f, 140.f}});

  // Only the robot participates in gravity. The drone follows a manual path.
  scene_.addRigidBody(robot_);
  sprites_.load(renderer_, assetDir);
  sprites_.reset(scene_, robot_, drone_);
}
