#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>

#include <string>
#include <utility>



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

  world_ = factory::createWorld(scene_);
  robot_ = factory::createPlayer(scene_,world_);
  engine::SpriteSheetLayout platformLayout;
  platformLayout.frames.push_back({{0.f,0.f},{1024.f,224.f}});
  const auto platformSheet = renderer_.createSpriteSheet(platformTexture,std::move(platformLayout));
  for (const auto platform : world_.platforms)
    scene_.addSpriteAnimation(platform,engine::SpriteAnimation::uniform(platformSheet,{0},1.f));
  sprites_.load(renderer_,assetDir);
  sprites_.reset(scene_,robot_,world_.drone);
}
