#pragma once

#include <engine/engine.hpp>
#include "drone_patrol.hpp"

class Game {
public:
    Game();
    void run();

private:
    void createLevel();
    engine::EntityId createBox(glm::vec2 position, glm::vec2 size, engine::Color color);
    void handleInput();
    void update(float deltaSeconds);
    void render();

    // Window must outlive its renderer; members are destroyed in reverse order.
    engine::Window window_;
    engine::Renderer renderer_;
    engine::Scene scene_;
    engine::InputHandler input_;
    engine::PhysicsSystem physics_{980.f};
    engine::Clock clock_;

    engine::EntityId robot_{};
    engine::EntityId floor_{};
    engine::EntityId crate_{};
    engine::EntityId drone_{};
    engine::EntityId exit_{};

    bool scalingKeyWasPressed_ = false;
    bool jumpKeyWasPressed_ = false;
    bool grounded_ = false;
    DronePatrol dronePatrol_;
};
