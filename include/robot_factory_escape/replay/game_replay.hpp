#pragma once

#include <engine/renderer.hpp>
#include <engine/replay/replayRecorder.hpp>
#include <engine/events/eventManager.hpp>
#include <engine/events/commonEvents.hpp>
#include <robot_factory_escape/gameplay/factory_world.hpp>

#include <optional>
#include <string>

namespace factory {

struct ReplayView {};
struct ReplayPresentation {
  std::optional<engine::ScalingMode> savedLiveScaling;
};

engine::Scene makeReplayView(const engine::Scene&, const World&, engine::EntityId robot, bool won);

class ReplaySession {
public:
  ReplaySession(engine::events::eventManager&, engine::FontId hudFont);
  ReplaySession(const ReplaySession&) = delete;
  ReplaySession& operator=(const ReplaySession&) = delete;
  bool recording() const noexcept { return recording_; }
  bool playing() const noexcept { return recorder_.playing(); }
  std::optional<std::string> input(const engine::events::inputEvent&, bool alive, bool paused);
  const engine::Scene* update(const engine::Scene&, const World&, engine::EntityId robot,
                              bool won, std::int64_t tick);

private:
  engine::events::eventManager& events_;
  engine::FontId hudFont_;
  engine::replay::replayRecorder recorder_;
  bool recording_ = false;
  engine::Scene presentation_;
  std::optional<engine::EntityId> marker_;
};

} // namespace factory
