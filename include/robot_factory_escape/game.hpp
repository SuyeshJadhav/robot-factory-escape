#pragma once

#include <engine/engine.hpp>
#include <robot_factory_escape/drone_patrol.hpp>
#include <robot_factory_escape/game_progress.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/game_sprites.hpp>
#include <robot_factory_escape/network_state.hpp>

#include <memory>
#include <atomic>
#include <optional>
#include <string>
#include <vector>

struct NetworkOptions {
  std::string serverHost = "127.0.0.1";
  std::uint16_t serverPort = 5555;
  std::string advertisedHost = "127.0.0.1";
  bool peerToPeer = true;
};

class Game {
public:
  explicit Game(std::optional<NetworkOptions> network = std::nullopt);
  void run();

private:
  void createLevel();
  engine::EntityId createBox(glm::vec2 position, glm::vec2 size,
                             engine::Color color);
  void handleInput(const engine::KeyboardState& keyboard);
  void update(float deltaSeconds);
  void drawExit();
  void render();
  void pumpNetwork(const std::vector<engine::networking::ServerEvent>& events);
  void setStatusText(std::string message);
  engine::Scene& world() { return simulationScene_ ? *simulationScene_ : scene_; }

  // Window must outlive its renderer; members are destroyed in reverse order.
  engine::Window window_;
  engine::Renderer renderer_;
  engine::Scene scene_;
  engine::InputHandler input_;
  engine::PhysicsSystem physics_{gameSettings::gravity};
  engine::Timeline realTime_;
  engine::Timeline gameTime_{realTime_, 60};
  std::unique_ptr<engine::SimulationThread> simulation_;
  engine::Scene* simulationScene_ = nullptr; // simulation thread only

  engine::EntityId robot_{};
  engine::EntityId floor_{};
  engine::EntityId drone_{};
  engine::EntityId exit_{};
  engine::EntityId statusText_{};
  std::vector<engine::EntityId> solids_;
  engine::TextureId backdropTexture_{};
  GameSprites sprites_;

  bool scalingKeyWasPressed_ = false;
  bool jumpKeyWasPressed_ = false;
  bool grounded_ = false;
  DronePatrol dronePatrol_;
  GameProgress progress_;
  std::atomic<bool> renderedWon_{false};
  bool restartKeyWasPressed_ = false;
  std::unique_ptr<engine::networking::Client> network_;
  engine::ClientId localClientId_ = 0;
  glm::vec2 localSpawn_{120.f, 300.f};
  std::unique_ptr<robotNet::RemoteRobots> remotePlayers_;
  robotNet::DroneState droneState_;
  std::int64_t lastLoggedDroneTick_ = -1;
};
