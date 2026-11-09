#pragma once

#include <engine/renderer.hpp>
#include <engine/replay/replayRecorder.hpp>
#include <engine/events/eventManager.hpp>
#include <engine/events/commonEvents.hpp>
#include <robot_factory_escape/gameplay/factory_world.hpp>

#include <optional>
#include <string>
#include <utility>

namespace factory {

class GameEvents;

struct ReplayView {};
struct ReplayPresentation {
  std::optional<engine::ScalingMode> savedLiveScaling;
};
bool isReplayFrame(const engine::Scene&);
bool renderReplayFrame(engine::Renderer&, const engine::Scene&, engine::TextureId backdrop);
engine::ScalingMode scalingForFrame(ReplayPresentation&, bool replayFrame, bool pPressed,
                                     engine::ScalingMode current);

engine::Scene makeReplayView(const engine::Scene&, const World&, engine::EntityId robot, bool won);

class ReplaySession {
public:
  ReplaySession(engine::events::eventManager&, engine::FontId hudFont);
  ReplaySession(const ReplaySession&) = delete;
  ReplaySession& operator=(const ReplaySession&) = delete;
  bool recording() const noexcept { return recording_; }
  bool playing() const noexcept { return recorder_.playing(); }
  std::optional<std::string> input(const engine::events::inputEvent&, bool alive, bool paused);
  void syncGameplay(GameEvents&) const noexcept;
  /// Synchronize posted input even when the paused clock produces no physics tick.
  void syncGameplay(GameEvents&, engine::SimulationThread&, const engine::Timeline&) const noexcept;
  /// True once after playback runs out by itself, so the live HUD can leave the replay text.
  bool takeNaturalEnd() noexcept { return std::exchange(naturalEnd_, false); }
  const engine::Scene* update(const engine::Scene&, const World&, engine::EntityId robot,
                              bool won, std::int64_t tick);

private:
  std::optional<std::string> feedback(std::string);
  engine::events::eventManager& events_;
  engine::FontId hudFont_;
  engine::replay::replayRecorder recorder_{60, 15}; // Rolling ~15 seconds at the game's 60 Hz clock.
  bool recording_ = false;
  bool naturalEnd_ = false;
  engine::Scene presentation_;
  std::optional<engine::EntityId> marker_;
  std::string banner_ = "REPLAY - ESC TO RETURN";
};

} // namespace factory
