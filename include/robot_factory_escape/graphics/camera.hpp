#pragma once
#include <engine/engine.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <algorithm>

namespace factory {
inline glm::vec2 cameraForPlayer(const engine::Scene &scene, engine::EntityId robot,
                                float viewportWidth) {
  const auto *collider = scene.getCollider(robot);
  const float width = collider ? collider->size.x : 0.f;
  const float center = scene.transform(robot).position.x + width / 2.f;
  const float viewport = std::max(1.f,viewportWidth);
  return {std::clamp(center - viewport / 2.f,0.f,
                     std::max(0.f,levelLayout::worldSize.x - viewport)),0.f};
}
inline engine::Scene cameraRenderScene(const engine::Scene &source, glm::vec2 camera) {
  engine::Scene view = source;
  // All current Text entities are HUD. Never call this on live simulation state.
  for (const auto &[id,text] : source.texts()) view.transform(id).position += camera;
  return view;
}
}
