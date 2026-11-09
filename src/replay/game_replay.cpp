#include <robot_factory_escape/replay/game_replay.hpp>
#include <robot_factory_escape/graphics/camera.hpp>
#include <engine/events/commonEvents.hpp>

#include <vector>

namespace factory {

engine::Scene makeReplayView(const engine::Scene& live, const World& world,
                             engine::EntityId robot, bool won) {
  const auto camera = cameraForPlayer(live, robot, levelLayout::levelSize.x);
  engine::Scene view = live;
  view.addShape(world.exit, {.size = {100.f, 140.f},
      .color = won ? engine::Color{80, 255, 140, 255} : engine::Color{50, 220, 235, 255}});
  std::vector<engine::EntityId> hidden;
  for (const auto& [id, transform] : view.transforms()) {
    if (view.getText(id) || (!view.getShape(id) && !view.getSpriteAnimation(id)))
      hidden.push_back(id);
    else
      view.transform(id).position -= camera;
  }
  for (const auto id : hidden)
    view.destroyEntity(id);
  return view;
}

ReplaySession::ReplaySession(engine::events::eventManager& events, engine::FontId hudFont)
    : events_(events), hudFont_(hudFont) {
  events_.subscribe<engine::events::replayRecordStart>([this](const auto&, engine::Scene&) {
    recorder_.clear();
    recording_ = true;
  });
  events_.subscribe<engine::events::replayRecordStop>([this](const auto&, engine::Scene&) {
    recording_ = false;
  });
  events_.subscribe<engine::events::replayPlayStart>([this](const auto& e, engine::Scene&) {
    recording_ = false;
    presentation_ = engine::Scene{};
    marker_.reset();
    recorder_.startPlayback(e.fromTick);
  });
  events_.subscribe<engine::events::replayPlayStop>([this](const auto&, engine::Scene&) {
    recorder_.stopPlayback();
  });
}

const engine::Scene* ReplaySession::update(const engine::Scene& live, const World& world,
                                           engine::EntityId robot, bool won, std::int64_t tick) {
  if (recording_)
    recorder_.record(makeReplayView(live, world, robot, won), tick);
  if (!recorder_.playing() || !recorder_.step(presentation_))
    return nullptr;
  if (!marker_ || !presentation_.hasEntity(*marker_)) {
    marker_ = presentation_.createEntity();
    presentation_.add<ReplayView>(*marker_);
    presentation_.transform(*marker_).position = {32.f, 32.f};
    presentation_.addText(*marker_, {.val = "REPLAY - ESC TO RETURN", .font = hudFont_,
                                     .color = {80, 255, 220, 255}});
  }
  return &presentation_;
}

} // namespace factory
