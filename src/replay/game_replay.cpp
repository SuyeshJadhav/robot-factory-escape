#include <robot_factory_escape/replay/game_replay.hpp>
#include <robot_factory_escape/graphics/camera.hpp>
#include <robot_factory_escape/gameplay/game_events.hpp>
#include <engine/events/commonEvents.hpp>

#include <vector>

namespace factory {

bool isReplayFrame(const engine::Scene& scene) {
  return !scene.all<ReplayView>().empty();
}

bool renderReplayFrame(engine::Renderer& renderer, const engine::Scene& frame,
                         engine::TextureId backdrop) {
  if (!isReplayFrame(frame))
    return false;
  renderer.setCamera({0.f, 0.f});
  renderer.clear({0, 0, 0, 255});
  renderer.drawTexture(backdrop, {0.f, 0.f}, levelLayout::levelSize);
  renderer.drawEntities(frame);
  renderer.present();
  return true;
}

engine::ScalingMode scalingForFrame(ReplayPresentation& state, bool replayFrame,
                                     bool pPressed, engine::ScalingMode current) {
  if (replayFrame) {
    if (!state.savedLiveScaling)
      state.savedLiveScaling = current;
    return engine::ScalingMode::Proportional;
  }
  auto mode = state.savedLiveScaling.value_or(current);
  state.savedLiveScaling.reset();
  if (pPressed)
    mode = mode == engine::ScalingMode::Proportional ? engine::ScalingMode::Constant
                                                    : engine::ScalingMode::Proportional;
  return mode;
}

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
    banner_ = "REPLAY - ESC TO RETURN";
    recorder_.startPlayback(e.fromTick);
  });
  events_.subscribe<engine::events::replayPlayStop>([this](const auto&, engine::Scene&) {
    recorder_.stopPlayback();
  });
}

void ReplaySession::syncGameplay(GameEvents& gameplay) const noexcept {
  gameplay.setGameplayBlocked(playing());
}

std::optional<std::string> ReplaySession::feedback(std::string message) {
  banner_ = message;
  if (marker_)
    if (auto* text = presentation_.getText(*marker_))
      text->val = message;
  return message;
}

std::optional<std::string> ReplaySession::input(const engine::events::inputEvent& event,
                                                bool alive, bool paused) {
  if (!event.pressed)
    return std::nullopt;
  using namespace engine::SC;
  if (playing() && (event.key == SDL_SCANCODE_T || event.key == SDL_SCANCODE_1 ||
                    event.key == SDL_SCANCODE_2 || event.key == SDL_SCANCODE_3))
    return feedback("STATUS: TIME CONTROLS LOCKED DURING REPLAY");
  if (event.key == SDL_SCANCODE_F5) {
    if (playing())
      return feedback("STATUS: STOP REPLAY BEFORE RECORDING");
    if (recording_)
      events_.raise(engine::events::replayRecordStop{}, events_.now(), -40);
    else
      events_.raise(engine::events::replayRecordStart{}, events_.now(), -40);
    return recording_ ? "STATUS: RECORDING STOPPED - F6 TO REPLAY" : "STATUS: RECORDING - F5 TO STOP";
  }
  if (event.key == SDL_SCANCODE_F6) {
    if (playing()) {
      events_.raise(engine::events::replayPlayStop{}, events_.now(), -40);
      return "STATUS: LIVE GAMEPLAY";
    }
    if (!alive)
      return "STATUS: RESPAWN BEFORE REPLAYING";
    if (paused)
      return "STATUS: UNPAUSE BEFORE REPLAYING";
    if (recorder_.empty())
      return "STATUS: NO RECORDING - F5 TO RECORD";
    events_.raise(engine::events::replayPlayStart{}, events_.now(), -40);
    return "STATUS: REPLAY - ESC TO RETURN";
  }
  if ((event.key == SDL_SCANCODE_ESCAPE || event.key == SDL_SCANCODE_R) && playing()) {
    events_.raise(engine::events::replayPlayStop{}, events_.now(), -40);
    if (event.key == SDL_SCANCODE_ESCAPE)
      return "STATUS: LIVE GAMEPLAY";
  }
  return std::nullopt;
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
    presentation_.addText(*marker_, {.val = banner_, .font = hudFont_,
                                     .color = {80, 255, 220, 255}});
  }
  return &presentation_;
}

} // namespace factory
