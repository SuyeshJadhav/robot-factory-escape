#pragma once

#include <engine/engine.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/gameplay/drone_patrol.hpp>
#include <robot_factory_escape/gameplay/game_progress.hpp>
#include <robot_factory_escape/gameplay/moving_platform.hpp>
#include <robot_factory_escape/graphics/game_sprites.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

struct NetworkOptions {
  std::string serverHost = "127.0.0.1";
  std::uint16_t serverPort = 5555;
  std::string advertisedHost = "127.0.0.1";
  bool peerToPeer = false;
};

class Game {
public:
  explicit Game(std::optional<NetworkOptions> network = std::nullopt, float initialSpeed = 1.f);
  void run();

private:
  void createLevel();
  void restart(engine::Scene &scene);
  void connectNetwork(const NetworkOptions &network);
  engine::EntityId createBox(glm::vec2 position, glm::vec2 size, engine::Color color);
  void handleInput(engine::Scene &scene, const engine::KeyboardState &keyboard);
  void update(engine::Scene &scene, float deltaSeconds);
  void drawExit();
  void render();
  void pumpNetwork(engine::Scene &scene, bool paused);
  void publishRobot(engine::Scene &scene, std::int64_t tick);
  void setStatusText(engine::Scene &scene, std::string message);

  // Window must outlive its renderer; members are destroyed in reverse order.
  engine::Window window_;
  engine::Renderer renderer_;
  engine::Scene scene_;
  engine::InputHandler input_;
  engine::PhysicsSystem physics_{gameSettings::gravity};
  engine::Timeline realTime_;
  engine::Timeline gameTime_{realTime_, 60};
  std::unique_ptr<engine::SimulationThread> simulation_;

  engine::EntityId robot_{};
  engine::EntityId floor_{};
  engine::EntityId drone_{};
  engine::EntityId movingPlatform_{};
  engine::EntityId exit_{};
  engine::EntityId statusText_{};
  engine::EntityId networkText_{};
  engine::EntityId timeText_{};
  engine::EntityId fpsText_{};
  std::vector<engine::EntityId> solids_;
  engine::TextureId backdropTexture_{};
  GameSprites sprites_;

  bool jumpKeyWasPressed_ = false;
  bool grounded_ = false;
  DronePatrol dronePatrol_;
  MovingPlatformPatrol platformPatrol_;
  GameProgress progress_;
  std::atomic<bool> renderedWon_{false};
  std::unique_ptr<engine::networking::sessionClient> clientSession_;
  std::unique_ptr<engine::networking::peerSession> peerSession_;
  engine::ClientId localClientId_ = 0;
  glm::vec2 localSpawn_{120.f, 300.f};
  std::unordered_set<engine::ClientId> remoteIds_;
  bool peerToPeer_ = false;
  bool connected_ = true;
};
