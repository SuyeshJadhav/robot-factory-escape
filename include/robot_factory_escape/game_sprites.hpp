#pragma once

#include <array>
#include <engine/engine.hpp>
#include <robot_factory_escape/network_state.hpp>
#include <string>

class GameSprites {
public:
  void load(engine::Renderer &renderer, const std::string &directory);
  void reset(engine::Scene &scene, engine::EntityId robot,
             engine::EntityId drone);
  void update(engine::Scene &scene, engine::EntityId robot, bool grounded,
              bool won, float dt);
  robotNet::AnimationState state(const engine::Scene &scene,
                                 engine::EntityId robot) const;
  bool applyRemote(engine::Scene &scene, engine::EntityId robot,
                   robotNet::AnimationState state) const;

private:
  enum Clip { IdleRight, IdleLeft, WalkRight, WalkLeft, JumpRight, JumpLeft };
  std::array<engine::SpriteAnimation, 6> robotClips_;
  engine::SpriteAnimation droneClip_;
  Clip current_ = IdleRight;
  bool facingLeft_ = false;
};
