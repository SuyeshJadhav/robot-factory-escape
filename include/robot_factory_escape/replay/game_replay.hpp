#pragma once

#include <engine/renderer.hpp>
#include <robot_factory_escape/gameplay/factory_world.hpp>

#include <optional>

namespace factory {

struct ReplayView {};
struct ReplayPresentation {
  std::optional<engine::ScalingMode> savedLiveScaling;
};

engine::Scene makeReplayView(const engine::Scene&, const World&, engine::EntityId robot, bool won);

} // namespace factory
