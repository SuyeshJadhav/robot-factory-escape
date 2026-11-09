#include "test_support.hpp"
#include <robot_factory_escape/replay/game_replay.hpp>
#include <robot_factory_escape/graphics/camera.hpp>

void replayScalingPolicy() {
  using engine::ScalingMode;
  for (const auto initial : {ScalingMode::Constant, ScalingMode::Proportional}) {
    factory::ReplayPresentation state;
    test::require(factory::scalingForFrame(state, true, false, initial) == ScalingMode::Proportional,
                  "replay always fits its fixed logical viewport");
    test::require(factory::scalingForFrame(state, true, true, ScalingMode::Proportional) ==
                      ScalingMode::Proportional && state.savedLiveScaling == initial,
                  "P during replay changes neither projection nor saved live preference");
    test::require(factory::scalingForFrame(state, false, false, ScalingMode::Proportional) == initial &&
                      !state.savedLiveScaling, "first live frame restores original scaling");
    const auto toggled = initial == ScalingMode::Constant ? ScalingMode::Proportional : ScalingMode::Constant;
    test::require(factory::scalingForFrame(state, false, true, initial) == toggled,
                  "subsequent live P toggles normally");
    factory::scalingForFrame(state, true, false, initial);
    test::require(factory::scalingForFrame(state, false, true, ScalingMode::Proportional) == toggled,
                  "P on first returning live frame restores first, then toggles");
  }
  engine::Scene live, display;
  display.createEntity();
  display.add<factory::ReplayView>(display.createEntity());
  const auto copied = display;
  test::require(!factory::isReplayFrame(live) && factory::isReplayFrame(copied),
                "copied marker, not original IDs, determines presentation mode");
}

int main() {
  return test::run([] {
    replayScalingPolicy();
    for (int padding : {0, 3, 11}) {
      engine::Scene live;
      for (int i = 0; i < padding; ++i)
        live.createEntity();
      const auto world = factory::createWorld(live);
      const auto robot = factory::createPlayer(live, world);
      live.transform(robot).position = {1800.f, 300.f};
      const auto hud = live.createEntity();
      live.addText(hud, {.val = "HUD must not be recorded"});
      const auto original = live.transform(robot).position;
      const auto count = live.transforms().size();
      const auto camera = factory::cameraForPlayer(live, robot, levelLayout::levelSize.x);
      auto view = factory::makeReplayView(live, world, robot, false);
      test::require(view.texts().empty() && !view.hasEntity(hud), "recorded view excludes HUD Text");
      test::require(view.getShape(robot) && view.transform(robot).position == original - camera,
                    "recorded geometry uses the actual robot and fixed logical camera");
      test::require(view.getShape(world.exit) &&
                        view.transform(world.exit).position == live.transform(world.exit).position - camera &&
                        view.getShape(world.exit)->color.g == 220,
                    "custom exit has a simple projected cyan replay marker");
      test::require(live.transform(robot).position == original && live.getText(hud) &&
                        live.transforms().size() == count && !live.getShape(world.exit),
                    "preparing a view never mutates live transforms or components");
      for (const auto& [id, transform] : view.transforms())
        test::require(view.getShape(id) || view.getSpriteAnimation(id),
                      "recorded view contains only visible geometry entities");
      live.removeShape(robot);
      live.removeSpriteAnimation(robot);
      view = factory::makeReplayView(live, world, robot, true);
      test::require(!view.getShape(robot) && view.getShape(world.exit)->color.g == 255,
                    "dead robot stays hidden and won exit marker is green");
      test::require(live.hasEntity(robot) && live.getCollider(robot),
                    "filtering hidden replay geometry preserves live camera bounds");
    }
  });
}
