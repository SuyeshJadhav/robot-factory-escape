#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/player_motion.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
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
    glm::vec2{MovingPlatformPatrol::left, MovingPlatformPatrol::height},
    glm::vec2{650.f, 560.f},  glm::vec2{980.f, 740.f},
    glm::vec2{1280.f, 550.f}, glm::vec2{1560.f, 360.f},
};
std::string speedLabel(float speed) {
  if (speed == 0.5f)
    return "0.5X";
  if (speed == 1.f)
    return "1X";
  if (speed == 2.f)
    return "2X";
  return std::to_string(speed) + "X";
}
} // namespace

Game::Game(std::optional<NetworkOptions> network, float initialSpeed)
    : window_("Robot Factory Escape | A/D: move | Space: jump | P: scaling | "
              "R: restart | T: pause",
              1280, 720),
      renderer_(window_) {
  renderer_.setScalingMode(engine::ScalingMode::Proportional);
  localSpawn_ = robotSpawn;
  createLevel();
  gameTime_.setSpeedMultiplier(initialSpeed);
  scene_.getText(timeText_)->val =
      "TIME: RUNNING " + speedLabel(initialSpeed);
  if (network) {
    peerToPeer_ = network->peerToPeer;
    const std::string endpoint = "tcp://" + network->serverHost + ":" +
                                 std::to_string(network->serverPort);
    if (peerToPeer_) {
      peerSession_ = std::make_unique<engine::networking::peerSession>(
          endpoint, network->advertisedHost);
      if (!peerSession_->join())
        throw std::runtime_error("could not join peer coordinator at " +
                                 endpoint);
      localClientId_ = peerSession_->id();
      peerSession_->replicator().bind(robotNet::droneId, drone_);
      peerSession_->replicator().bind(robotNet::movingPlatformId,
                                      movingPlatform_);
      if (peerSession_->replicator().track(robot_) !=
          robotNet::robotId(localClientId_))
        throw std::runtime_error("unexpected robot network ID");
      peerSession_->start();
    } else {
      clientSession_ =
          std::make_unique<engine::networking::sessionClient>(endpoint);
      if (!clientSession_->join())
        throw std::runtime_error("could not join server at " + endpoint);
      localClientId_ = clientSession_->id();
      clientSession_->replicator().bind(robotNet::droneId, drone_);
      clientSession_->replicator().bind(robotNet::movingPlatformId,
                                        movingPlatform_);
      if (clientSession_->replicator().track(robot_) !=
          robotNet::robotId(localClientId_))
        throw std::runtime_error("unexpected robot network ID");
      clientSession_->setHeartbeat(100);
      clientSession_->start();
    }
    resetRobot(scene_, robot_, localSpawn_, grounded_);
    scene_.getText(networkText_)->val =
        std::string("NETWORK: ") + (peerToPeer_ ? "PEER" : "SERVER") +
        "  PLAYER " + std::to_string(localClientId_) + "  CONNECTED";
    engine::log::info("Joined as client {} ({})", localClientId_,
                      peerToPeer_ ? "peer-to-peer" : "client-server");
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
  createText({32.f, 62.f},
             "A/D MOVE  SPACE JUMP  R RESTART  T PAUSE  1/2/3 SPEED  P SCALE",
             hudFont, {235, 240, 255, 255});
  createText({32.f, 94.f}, "WATCH THE MOVING PLATFORM  ->  REACH THE EXIT", hudFont,
             {255, 190, 55, 255});
  statusText_ = createText({32.f, 126.f}, "STATUS: AVOID THE PATROL DRONE",
                           hudFont, {255, 110, 120, 255});
  networkText_ = createText({32.f, 158.f}, "NETWORK: OFFLINE", hudFont,
                            {80, 255, 220, 255});
  timeText_ = createText({32.f, 190.f}, "TIME: RUNNING 1X", hudFont,
                         {255, 190, 55, 255});

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
  for (std::size_t i = 0; i < platformPositions.size(); ++i) {
    const auto position = platformPositions[i];
    const auto platform = scene_.createEntity();
    if (i == 0)
      movingPlatform_ = platform;
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

void Game::publishRobot(engine::Scene &scene, std::int64_t tick) {
  auto &replicator =
      peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  replicator.setExtra(robotNet::robotId(localClientId_),
                      robotNet::encode(sprites_.state(scene, robot_)));
  if (peerToPeer_)
    peerSession_->publishScene(scene, tick);
  else
    clientSession_->submitScene(scene, tick);
}

void Game::pumpNetwork(engine::Scene &scene) {
  if (peerToPeer_) {
    peerSession_->applyUpdates(scene);
    remoteIds_.clear();
    for (const auto &client : peerSession_->roster())
      if (client.id != localClientId_)
        remoteIds_.insert(client.id);
  } else {
    clientSession_->applySnapshot(scene);
    for (const auto &event : clientSession_->drainRosterEvents()) {
      if (event.client.id == localClientId_)
        continue;
      if (event.change == engine::networking::RosterChange::Joined)
        remoteIds_.insert(event.client.id);
      else
        remoteIds_.erase(event.client.id);
    }
  }
  auto &replicator =
      peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  // A snapshot can arrive before its roster event. Exclude every foreign
  // robot from physics immediately, even if its visual setup waits one poll.
  std::vector<engine::EntityId> foreignBodies;
  for (const auto &[id, body] : scene.rigidBodies()) {
    if (id == robot_)
      continue;
    if (const auto netId = replicator.netIdOf(id);
        netId && engine::ownerOf(*netId) != engine::kServerId)
      foreignBodies.push_back(id);
  }
  for (const auto id : foreignBodies)
    robotNet::makeRemoteVisual(scene, id);
  for (auto id : remoteIds_) {
    const auto remote = replicator.entityOf(robotNet::robotId(id));
    if (!remote || !scene.hasEntity(*remote))
      continue;
    robotNet::makeRemoteVisual(scene, *remote);
    // The renderer's texture handles are local, so restore the local sprite
    // clip.
    if (const auto *bytes = replicator.extraOf(robotNet::robotId(id))) {
      if (const auto state = robotNet::decode(*bytes))
        sprites_.applyRemote(scene, *remote, *state);
    }
    if (auto *shape = scene.getShape(*remote))
      shape->color = {255, 210, 60, 255};
  }
}

void Game::run() {
  simulation_ = std::make_unique<engine::SimulationThread>(
      scene_, gameTime_, [this](const engine::TickContext &ctx) {
        handleInput(ctx.scene, ctx.keyboard);
        update(ctx.scene, ctx.dt);
        if (clientSession_ || peerSession_)
          publishRobot(ctx.scene, ctx.tick);
      });
  simulation_->start();
  engine::KeyboardState previous;
  engine::SimStatus lastStatus;
  try {
    while (!window_.shouldClose()) {
      window_.pollEvents();
      const auto keyboard = engine::KeyboardState::capture(input_);
      simulation_->submitKeyboard(keyboard);
      using namespace engine::SC;
      if (keyboard.justPressed(SDL_SCANCODE_T, previous)) {
        simulation_->post([this](engine::Scene &scene, engine::Timeline &time) {
          if (time.paused())
            time.unpause();
          else
            time.pause();
          scene.getText(timeText_)->val =
              std::string("TIME: ") + (time.paused() ? "PAUSED " : "RUNNING ") +
              speedLabel(time.speedMultiplier());
          engine::log::info("Local simulation {}",
                            time.paused() ? "paused" : "resumed");
        });
      }
      const float requestedSpeed =
          keyboard.justPressed(SDL_SCANCODE_1, previous)   ? 0.5f
          : keyboard.justPressed(SDL_SCANCODE_2, previous) ? 1.f
          : keyboard.justPressed(SDL_SCANCODE_3, previous) ? 2.f
                                                           : 0.f;
      if (requestedSpeed > 0.f) {
        simulation_->post([this, requestedSpeed](engine::Scene &scene,
                                                 engine::Timeline &time) {
          time.setSpeedMultiplier(requestedSpeed);
          scene.getText(timeText_)->val =
              std::string("TIME: ") + (time.paused() ? "PAUSED " : "RUNNING ") +
              speedLabel(requestedSpeed);
        });
      }
      if (keyboard.justPressed(SDL_SCANCODE_P, previous))
        renderer_.toggleScalingMode();
      previous = keyboard;

      if (clientSession_ || peerSession_) {
        simulation_->post([this](engine::Scene &scene, engine::Timeline &) {
          pumpNetwork(scene);
        });
        const bool connectedNow = peerToPeer_ ? peerSession_->connected()
                                              : clientSession_->connected();
        if (connectedNow != connected_) {
          connected_ = connectedNow;
          simulation_->post([this, connectedNow](engine::Scene &scene,
                                                 engine::Timeline &) {
            scene.getText(networkText_)->val =
                std::string("NETWORK: ") + (peerToPeer_ ? "PEER" : "SERVER") +
                "  PLAYER " + std::to_string(localClientId_) +
                (connectedNow ? "  CONNECTED" : "  DISCONNECTED");
          });
        }
        // A paused peer still advertises its frozen robot to a late joiner.
        if (peerToPeer_ && lastStatus.paused &&
            std::chrono::steady_clock::now() - lastPausedPublish_ >
                std::chrono::milliseconds(250)) {
          lastPausedPublish_ = std::chrono::steady_clock::now();
          const auto tick = lastStatus.tick;
          simulation_->post(
              [this, tick](engine::Scene &scene, engine::Timeline &) {
                publishRobot(scene, tick);
              });
        }
      }
      if (auto frame = simulation_->takeRenderFrame()) {
        scene_ = std::move(frame->scene);
        lastStatus = frame->status;
      }
      if (auto failure = simulation_->failure())
        std::rethrow_exception(failure);
      render();
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  } catch (...) {
    simulation_->stop();
    if (peerSession_)
      peerSession_->leave();
    if (clientSession_)
      clientSession_->leave();
    throw;
  }
  simulation_->stop();
  if (peerSession_)
    peerSession_->leave();
  if (clientSession_)
    clientSession_->leave();
}

void Game::handleInput(engine::Scene &scene,
                       const engine::KeyboardState &keyboard) {

  const bool restartKeyIsPressed =
      keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_R);
  const bool restartPressed = restartKeyIsPressed && !restartKeyWasPressed_;
  restartKeyWasPressed_ = restartKeyIsPressed;
  const bool jumpKeyIsPressed =
      keyboard.isKeyPressed(engine::SC::SDL_SCANCODE_SPACE);
  const bool jumpPressed = jumpKeyIsPressed && !jumpKeyWasPressed_;
  jumpKeyWasPressed_ = jumpKeyIsPressed;
  if (restartPressed) {
    if (clientSession_ || peerSession_) {
      // The coordinator keeps the shared drone clock running across local
      // restarts.
      resetRobot(scene, robot_, localSpawn_, grounded_);
      progress_.won = false;
    } else {
      progress_.restart(scene, robot_, drone_, localSpawn_, grounded_,
                        dronePatrol_);
      platformPatrol_ = MovingPlatformPatrol{};
      platformPatrol_.advance(scene, movingPlatform_, 0.f);
    }
    sprites_.reset(scene, robot_, drone_);
    renderedWon_.store(false);
    setStatusText(scene, "STATUS: AVOID THE PATROL DRONE");
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
  scene.getRigidBody(robot_)->velocity.x = direction * robotSpeed;
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
      platformPatrol_.advance(scene, movingPlatform_, step);
    }
    movePlayer(scene, physics_, robot_, solids_, step, grounded_);
    if (resetOnDroneContact(scene, physics_, robot_, drone_, localSpawn_,
                            grounded_)) {
      sprites_.reset(scene, robot_, drone_);
      setStatusText(scene, "STATUS: DRONE HIT - TRY AGAIN");
      engine::log::info("Drone contact! Back to the start.");
      break;
    }
    if (scene.transform(robot_).position.y > levelSize.y) {
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

void Game::render() {
  renderer_.clear({0, 0, 0, 255});
  // Draw the backdrop first, outside the unordered entity collection.
  renderer_.drawTexture(backdropTexture_, {0.f, 0.f}, levelSize);
  drawExit();
  const auto platformPosition = scene_.transform(movingPlatform_).position;
  renderer_.fillRect(platformPosition + glm::vec2{-5.f, -5.f},
                     {platformSize.x + 10.f, platformColliderSize.y + 10.f},
                     {80, 255, 220, 255});
  // Gold markers distinguish other players despite the renderer using local
  // sprite art.
  for (const auto &[id, shape] : scene_.shapes()) {
    if (id == robot_ || id == drone_ || !scene_.getSpriteAnimation(id))
      continue;
    if (shape.color.r == 255 && shape.color.g == 210 && shape.color.b == 60) {
      const auto position = scene_.transform(id).position;
      renderer_.fillRect(position + glm::vec2{-3.f, -3.f}, {102.f, 134.f},
                         {255, 210, 60, 255});
    }
  }
  renderer_.drawEntities(scene_);
  renderer_.present();
}

void Game::drawExit() {
  const auto position = scene_.transform(exit_).position;
  const engine::Color frame = renderedWon_.load()
                                  ? engine::Color{80, 255, 140, 255}
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
