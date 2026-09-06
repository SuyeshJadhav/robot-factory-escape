#pragma once

#include <engine/engine.hpp>
#include <array>
#include <string>

class GameSprites {
public:
    void load(engine::Renderer& renderer, const std::string& directory);
    void reset(engine::Scene& scene, engine::EntityId robot, engine::EntityId drone);
    void update(engine::Scene& scene, engine::EntityId robot, bool grounded,
                bool won, float dt);

private:
    enum Clip { IdleRight, IdleLeft, WalkRight, WalkLeft, JumpRight, JumpLeft };
    std::array<engine::SpriteAnimation, 6> robotClips_;
    engine::SpriteAnimation droneClip_;
    Clip current_ = IdleRight;
    bool facingLeft_ = false;
};
