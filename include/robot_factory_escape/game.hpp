#pragma once

#include <engine/engine.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/gameplay/factory_gameplay.hpp>
#include <robot_factory_escape/gameplay/game_events.hpp>
#include <robot_factory_escape/gameplay/game_progress.hpp>
#include <robot_factory_escape/graphics/game_sprites.hpp>
#include <robot_factory_escape/networking/network_state.hpp>
#include <robot_factory_escape/replay/game_replay.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
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

  void connectNetwork(const NetworkOptions &network);
  void handleControls(engine::Scene&, const engine::events::inputEvent&);
  void update(engine::Scene &scene, float deltaSeconds);
  void drawExit();
  void render();
  void pumpNetwork(engine::Scene &scene, bool paused);
  void publishRobot(engine::Scene &scene, std::int64_t tick);
  void publishLifeEvents(std::int64_t tick);
  void setStatusText(engine::Scene &scene, std::string message);

  // Window must outlive its renderer; members are destroyed in reverse order.
  engine::Window window_;
  engine::Renderer renderer_;
  factory::ReplayPresentation replayPresentation_;
  engine::Scene scene_;
  engine::InputHandler input_;
  engine::PhysicsSystem physics_{gameSettings::gravity};
  engine::Timeline realTime_;
  engine::Timeline gameTime_{realTime_, 60};

  engine::EntityId robot_{};
  factory::World world_;
  engine::EntityId statusText_{};
  engine::EntityId networkText_{};
  engine::EntityId timeText_{};
  engine::EntityId fpsText_{};
  engine::TextureId backdropTexture_{};
  engine::FontId hudFont_{};
  GameSprites sprites_;

  bool grounded_ = false;
  GameProgress progress_;
  engine::events::eventManager events_;
  std::unique_ptr<factory::GameEvents> rules_;
  std::unique_ptr<factory::ReplaySession> replays_;
  std::int64_t lastSimulationTick_ = 0;
  std::atomic<bool> renderedWon_{false};
  std::unique_ptr<engine::networking::sessionClient> clientSession_;
  std::unique_ptr<engine::networking::peerSession> peerSession_;
  engine::ClientId localClientId_ = 0;
  std::unordered_set<engine::ClientId> remoteIds_;
  std::unordered_set<engine::ClientId> departedIds_;
  robotNet::RemoteLives remoteLives_;
  robotNet::LifeNotices lifeNotices_;
  std::unordered_map<engine::NetId, robotNet::AnimationState> remoteAnimation_;
  bool peerToPeer_ = false;
  bool connected_ = true;
  // Stop the worker before destroying anything its callbacks reference.
  std::unique_ptr<engine::SimulationThread> simulation_;
};
