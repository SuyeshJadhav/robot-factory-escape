#pragma once

#include <engine/events/commonEvents.hpp>
#include <engine/events/eventManager.hpp>
#include <robot_factory_escape/gameplay/factory_gameplay.hpp>
#include <robot_factory_escape/networking/life_events.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace factory {

struct RespawnDue : engine::events::simpleEvent {
  std::uint64_t generation = 0;
};
struct RestartEvent : engine::events::simpleEvent {};

class GameEvents {
public:
  using Controls = std::function<void(const engine::events::inputEvent&)>;
  GameEvents(engine::events::eventManager&, const World&, engine::EntityId robot,
             engine::ClientId owner, bool ownsWorld, bool& grounded,
             GameProgress&, Controls controls = {});
  GameEvents(const GameEvents&) = delete;
  GameEvents& operator=(const GameEvents&) = delete;

  void start(engine::Scene&, std::int64_t tick);
  void input(engine::Scene&, const engine::KeyboardState& now,
             const engine::KeyboardState& previous, std::int64_t tick);
  TickResult step(engine::Scene&, engine::PhysicsSystem&, std::int64_t tick, float dt);
  void setGameplayBlocked(bool blocked) noexcept;
  bool alive() const noexcept { return alive_; }
  std::uint32_t revision() const noexcept { return revision_; }
  std::vector<robotNet::LifeEvent> takeLifeEvents();

private:
  void spawn(engine::Scene&);
  void die(engine::Scene&);

  engine::events::eventManager& events_;
  World world_;
  engine::EntityId robot_;
  engine::ClientId owner_;
  bool ownsWorld_;
  bool& grounded_;
  GameProgress& progress_;
  Controls controls_;
  engine::KeyboardState held_;
  bool alive_ = false;
  bool blocked_ = false;
  bool jumpRequested_ = false;
  std::uint32_t revision_ = 0;
  std::uint64_t generation_ = 0;
  std::optional<engine::Shape> shape_;
  std::optional<engine::SpriteAnimation> animation_;
  std::vector<robotNet::LifeEvent> outbox_;
};

} // namespace factory
