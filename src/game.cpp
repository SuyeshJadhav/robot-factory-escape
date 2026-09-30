#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/player_motion.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <chrono>
#include <thread>
#include <utility>

namespace {
constexpr glm::vec2 levelSize{1920.f, 1080.f};
constexpr glm::vec2 robotSpawn{120.f, 300.f};
constexpr float robotSpeed = 300.f; // Logical pixels per second.
constexpr glm::vec2 platformSize{300.f, 66.f};
constexpr glm::vec2 platformColliderSize{300.f, 24.f};
constexpr std::array platformPositions{
    // Uphill steps rise 190 pixels: close to the roughly 211-pixel jump limit.
    glm::vec2{330.f, 750.f},  glm::vec2{650.f, 560.f},  glm::vec2{980.f, 740.f},
    glm::vec2{1280.f, 550.f}, glm::vec2{1560.f, 360.f},
};
} // namespace

Game::Game(std::optional<NetworkOptions> network)
    : window_("Robot Factory Escape | A/D: move | Space: jump | P: scaling | "
              "R: restart | T: pause",
              1280, 720),
      renderer_(window_) {
  renderer_.setScalingMode(engine::ScalingMode::Proportional);
  createLevel();
  if (network) {
    engine::networking::ClientConfig config;
    config.serverHost = network->serverHost;
    config.joinPort = network->serverPort;
    config.advertisedHost = network->advertisedHost;
    config.peerToPeer = network->peerToPeer;
    config.heartbeat = std::chrono::milliseconds(16); // ~60 Hz world polling matching server timeline
    network_ = std::make_unique<engine::networking::Client>(config);
    network_->start();
    localClientId_ = network_->id();
    localSpawn_.x = robotSpawn.x + static_cast<float>(localClientId_ - 1) * 120.f;
    resetRobot(scene_, robot_, localSpawn_, grounded_);
    remotePlayers_ = std::make_unique<robotNet::RemoteRobots>(localClientId_);
    engine::log::info("Joined world as client {} ({})", localClientId_,
                      network->peerToPeer ? "peer-to-peer" : "client-server");
  }
  engine::log::info(
      "Factory ready. Cyborg: player; red drone: hazard; cyan door: exit.");
  engine::log::info("A/D: move; Space: jump; P: scaling. Falling off-screen "
                    "resets the robot.");
  engine::log::info(
      "Avoid the red patrol drone: contact returns you to the start.");
  engine::log::info(
      "Reach the cyan factory exit to win. Press R to restart at any time.");
}

engine::EntityId Game::createBox(glm::vec2 position, glm::vec2 size,
                                 engine::Color color) {
  const auto id = scene_.createEntity();
  scene_.transform(id).position = position;
  scene_.addShape(id, {.size = size, .color = color, .texture = std::nullopt});
  scene_.addCollider(id, {.size = size});
  return id;
}

void Game::createLevel() {
  const std::string assetDir = GAME_ASSET_DIR;
  backdropTexture_ = renderer_.loadTexture(
      assetDir + "background/cyberpunk_background_backdropa_idle.png");
  const auto platformTexture = renderer_.loadTexture(
      assetDir + "platform/cyberpunk_platform_antigravcart_idle.png");

  const auto hudFont = renderer_.loadFont(GAME_FONT_PATH, 24.f);
  const auto titleFont = renderer_.loadFont(GAME_FONT_PATH, 34.f);
  const auto createText = [this](glm::vec2 position, std::string value,
                                 engine::FontId font, engine::Color color) {
    const auto id = scene_.createEntity();
    scene_.transform(id).position = position;
    scene_.addText(id, {.val = std::move(value), .font = font, .color = color});
    return id;
  };
  createText({32.f, 18.f}, "ROBOT FACTORY ESCAPE", titleFont,
             {80, 255, 220, 255});
  createText({32.f, 62.f}, "A/D MOVE  SPACE JUMP  R RESTART  T PAUSE  1/2/3 SPEED  P SCALE",
             hudFont, {235, 240, 255, 255});
  createText({32.f, 94.f}, "CLIMB THE PLATFORMS  ->  REACH THE EXIT", hudFont,
             {255, 190, 55, 255});
  statusText_ = createText({32.f, 126.f}, "STATUS: AVOID THE PATROL DRONE",
                           hudFont, {255, 110, 120, 255});

  // Most boxes have matching visual and collision bounds, with scale left at 1.
  floor_ = createBox({0.f, 940.f}, {1920.f, 140.f}, {70, 78, 91, 255});
  solids_.push_back(floor_);
  robot_ = createBox(robotSpawn, {96.f, 128.f}, {72, 205, 230, 255});
  // The cyborg is narrower than its animation cell; keep the full visual height
  // while tightening horizontal collision to the character's body.
  scene_.addCollider(robot_, {.size = {80.f, 128.f}});

  // Reuse one platform sheet for a staircase of reachable Mario-like jumps.
  // The collider covers only the solid deck, not the transparent machinery
  // below it.
  engine::SpriteSheetLayout platformLayout;
  platformLayout.frames.push_back({{0.f, 0.f}, {1024.f, 224.f}});
  const auto platformSheet =
      renderer_.createSpriteSheet(platformTexture, std::move(platformLayout));
  for (const auto position : platformPositions) {
    const auto platform = scene_.createEntity();
    scene_.transform(platform).position = position;
    scene_.addShape(platform, {.size = platformSize,
                               .color = {255, 255, 255, 255},
                               .texture = std::nullopt});
    scene_.addCollider(platform, {.size = platformColliderSize});
    scene_.addSpriteAnimation(
        platform, engine::SpriteAnimation::uniform(platformSheet, {0}, 1.f));
    solids_.push_back(platform);
  }

  drone_ = createBox({0.f, 0.f}, {130.f, 80.f}, {235, 85, 93, 255});
  dronePatrol_.advance(scene_, drone_, 0.f);
  // The trigger remains a simple collider; drawExit provides the themed
  // artwork.
  exit_ = scene_.createEntity();
  // The exit sits on the final platform, so walking across the floor is not
  // enough.
  scene_.transform(exit_).position = {1740.f,
                                      platformPositions.back().y - 140.f};
  scene_.addCollider(exit_, {.size = {100.f, 140.f}});

  // Only the robot participates in gravity. The drone follows a manual path.
  scene_.addRigidBody(robot_);
  sprites_.load(renderer_, assetDir);
  sprites_.reset(scene_, robot_, drone_);
}

void Game::run() {
  simulation_ = std::make_unique<engine::SimulationThread>(
      scene_, gameTime_, [this](const engine::TickContext& ctx) {
        simulationScene_ = &ctx.scene;
        handleInput(ctx.keyboard);
        update(ctx.dt);
        if (network_) {
          const auto position = world().transform(robot_).position;
          network_->send(robotNet::encode({position.x, position.y}), ctx.tick);
        }
      });
  simulation_->start();
  engine::KeyboardState previous;
  while (!window_.shouldClose()) {
    window_.pollEvents();
    const auto keyboard = engine::KeyboardState::capture(input_);
    simulation_->submitKeyboard(keyboard);
    using namespace engine::SC;
    if (keyboard.justPressed(SDL_SCANCODE_T, previous)) {
      simulation_->post([](engine::Scene&, engine::Timeline& time) {
        if (time.paused()) time.unpause();
        else time.pause();
        engine::log::info("Local simulation {} at {} ms",
            time.paused() ? "paused" : "resumed",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
      });
    }
    if (keyboard.justPressed(SDL_SCANCODE_1, previous)) simulation_->setSpeed(0.5f);
    if (keyboard.justPressed(SDL_SCANCODE_2, previous)) simulation_->setSpeed(1.f);
    if (keyboard.justPressed(SDL_SCANCODE_3, previous)) simulation_->setSpeed(2.f);
    if (keyboard.justPressed(SDL_SCANCODE_P, previous)) {
      renderer_.toggleScalingMode();
      engine::log::info("Scaling: {}", renderer_.scalingMode() ==
          engine::ScalingMode::Proportional ? "proportional" : "constant");
    }
    previous = keyboard;
    if (network_) {
      if (auto events = network_->drain(); !events.empty()) {
        simulation_->post([this, events = std::move(events)](engine::Scene& scene,
                                                              engine::Timeline&) {
          simulationScene_ = &scene;
          pumpNetwork(events);
        });
      }
    }
    if (auto frame = simulation_->takeRenderFrame()) scene_ = std::move(frame->scene);
    if (auto failure = simulation_->failure()) {
      simulation_->stop();
      std::rethrow_exception(failure);
    }
    render();
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  simulation_->stop();
  if (network_) network_->stop();
}

void Game::pumpNetwork(const std::vector<engine::networking::ServerEvent>& events) {
  for (const auto& event : events) {
    using Kind = engine::networking::ServerEvent::Kind;
    if (event.kind == Kind::World) {
      if (droneState_.apply(world(), drone_, event)) {
        if (lastLoggedDroneTick_ < 0 || event.tick - lastLoggedDroneTick_ >= 30) {
          lastLoggedDroneTick_ = event.tick;
          const auto position = world().transform(drone_).position;
          engine::log::info("Client {} drone tick {} position ({}, {}) at {} ms",
                            localClientId_, event.tick, position.x, position.y,
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count());
        }
      }
    } else if (remotePlayers_ && remotePlayers_->apply(world(), event)) {
      if (event.kind == Kind::PlayerLeft)
        engine::log::info("Client {} removed remote robot {}", localClientId_, event.client);
      else if (event.tick % 30 == 0) {
        const auto position = world().transform(*remotePlayers_->entity(event.client)).position;
        engine::log::info("Client {} remote robot {} tick {} position ({}, {}) at {} ms",
                          localClientId_, event.client, event.tick, position.x, position.y,
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch()).count());
      }
    }
  }
}

void Game::handleInput(const engine::KeyboardState& keyboard) {

  const bool restartKeyIsPressed =
      keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_R);
  const bool restartPressed = restartKeyIsPressed && !restartKeyWasPressed_;
  restartKeyWasPressed_ = restartKeyIsPressed;
  const bool jumpKeyIsPressed =
      keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_SPACE);
  const bool jumpPressed = jumpKeyIsPressed && !jumpKeyWasPressed_;
  jumpKeyWasPressed_ = jumpKeyIsPressed;
  if (restartPressed) {
    if (network_) {
      // The coordinator keeps the shared drone clock running across local restarts.
      resetRobot(world(), robot_, localSpawn_, grounded_);
      progress_.won = false;
    } else {
      progress_.restart(world(), robot_, drone_, localSpawn_, grounded_,
                        dronePatrol_);
    }
    sprites_.reset(world(), robot_, drone_);
    renderedWon_.store(false);
    setStatusText("STATUS: AVOID THE PATROL DRONE");
    engine::log::info("Restarted. Reach the exit!");
    return;
  }
  if (progress_.won)
    return;

  const bool moveLeft = keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_A);
  const bool moveRight = keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_D);
  const float direction =
      static_cast<float>(moveRight) - static_cast<float>(moveLeft);
  // Input sets speed; physics applies elapsed time and changes position.
  world().getRigidBody(robot_)->velocity.x = direction * robotSpeed;
  jumpIfGrounded(*world().getRigidBody(robot_), grounded_, jumpPressed);
}

void Game::update(float deltaSeconds) {
  if (progress_.won)
    return;
  // Advance both movers on the same clock and check contact each small step.
  float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
  while (remaining > 0.f) {
    const float step = std::min(remaining, 1.f / 120.f);
    remaining -= step;
    if (!network_)
      dronePatrol_.advance(world(), drone_, step);
    movePlayer(world(), physics_, robot_, solids_, step, grounded_);
    if (resetOnDroneContact(world(), physics_, robot_, drone_, localSpawn_,
                            grounded_)) {
      sprites_.reset(world(), robot_, drone_);
      setStatusText("STATUS: DRONE HIT - TRY AGAIN");
      engine::log::info("Drone contact! Back to the start.");
      break;
    }
    if (world().transform(robot_).position.y > levelSize.y) {
      resetRobot(world(), robot_, localSpawn_, grounded_);
      sprites_.reset(world(), robot_, drone_);
      setStatusText("STATUS: MISSED A JUMP - BACK TO START");
      break;
    }
    if (progress_.checkExit(world(), physics_, robot_, exit_)) {
      renderedWon_.store(true);
      setStatusText("ESCAPED! PRESS R TO PLAY AGAIN");
      engine::log::info("You escaped! Press R to play again.");
      break;
    }
  }
  sprites_.update(world(), robot_, grounded_, progress_.won, deltaSeconds);
}

void Game::setStatusText(std::string message) {
  if (auto *text = world().getText(statusText_)) {
    text->val = std::move(message);
  }
}

void Game::render() {
  renderer_.clear({0, 0, 0, 255});
  // Draw the backdrop first, outside the unordered entity collection.
  renderer_.drawTexture(backdropTexture_, {0.f, 0.f}, levelSize);
  drawExit();
  renderer_.drawEntities(scene_);
  renderer_.present();
}

void Game::drawExit() {
  const auto position = scene_.transform(exit_).position;
  const engine::Color frame = renderedWon_.load() ? engine::Color{80, 255, 140, 255}
                                            : engine::Color{50, 220, 235, 255};
  const engine::Color indicator = renderedWon_.load()
                                      ? engine::Color{80, 255, 100, 255}
                                      : engine::Color{255, 170, 35, 255};

  // A compact cyberpunk factory door built from engine rectangles.
  renderer_.fillRect(position + glm::vec2{-18.f, -20.f}, {136.f, 160.f},
                     {7, 18, 27, 255});
  renderer_.fillRect(position + glm::vec2{-12.f, -14.f}, {124.f, 154.f}, frame);
  renderer_.fillRect(position + glm::vec2{-5.f, -7.f}, {110.f, 147.f},
                     {12, 29, 39, 255});
  renderer_.fillRect(position + glm::vec2{3.f, 2.f}, {94.f, 138.f},
                     {23, 46, 57, 255});
  renderer_.fillRect(position + glm::vec2{48.f, 2.f}, {4.f, 138.f}, frame);

  // Hazard rails and a status panel tie the door to the orange/cyan factory
  // palette.
  renderer_.fillRect(position + glm::vec2{-12.f, 8.f}, {7.f, 22.f},
                     {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{-12.f, 42.f}, {7.f, 22.f},
                     {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{105.f, 8.f}, {7.f, 22.f},
                     {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{105.f, 42.f}, {7.f, 22.f},
                     {255, 116, 24, 255});
  renderer_.fillRect(position + glm::vec2{27.f, 14.f}, {46.f, 25.f},
                     {5, 15, 22, 255});
  renderer_.fillRect(position + glm::vec2{34.f, 20.f}, {32.f, 13.f}, indicator);
  renderer_.fillRect(position + glm::vec2{76.f, 76.f}, {8.f, 22.f}, indicator);
}
