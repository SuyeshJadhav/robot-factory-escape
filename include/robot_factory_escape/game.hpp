#pragma once

#include <engine/engine.hpp>
#include <robot_factory_escape/drone_patrol.hpp>
#include <robot_factory_escape/game_progress.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/game_sprites.hpp>

#include <string>
#include <vector>

class Game {
public:
    Game();
    void run();

private:
    void createLevel();
    engine::EntityId createBox(glm::vec2 position, glm::vec2 size, engine::Color color);
    void handleInput();
    void update(float deltaSeconds);
    void drawExit();
    void render();
    void setStatusText(std::string message);

    // Window must outlive its renderer; members are destroyed in reverse order.
    engine::Window window_;
    engine::Renderer renderer_;
    engine::Scene scene_;
    engine::InputHandler input_;
    engine::PhysicsSystem physics_{gameSettings::gravity};
    engine::Timeline realTime_;
    engine::Timeline gameTime_{realTime_, 60};
    engine::Stepper simulation_{gameTime_};

    engine::EntityId robot_{};
    engine::EntityId floor_{};
    engine::EntityId drone_{};
    engine::EntityId exit_{};
    engine::EntityId statusText_{};
    std::vector<engine::EntityId> solids_;
    engine::TextureId backdropTexture_{};
    GameSprites sprites_;

    bool scalingKeyWasPressed_ = false;
    bool jumpKeyWasPressed_ = false;
    bool grounded_ = false;
    DronePatrol dronePatrol_;
    GameProgress progress_;
    bool restartKeyWasPressed_ = false;
};
