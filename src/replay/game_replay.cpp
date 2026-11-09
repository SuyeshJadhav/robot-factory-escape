#include <robot_factory_escape/replay/game_replay.hpp>
#include <robot_factory_escape/graphics/camera.hpp>

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

} // namespace factory
