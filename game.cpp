#include "game.hpp"
#include "player_motion.hpp"

#include <array>
#include <algorithm>

namespace {
constexpr glm::vec2 levelSize{1920.f, 1080.f};
constexpr glm::vec2 robotSpawn{120.f, 300.f};
constexpr float robotSpeed = 300.f; // Logical pixels per second.
constexpr engine::Color background{22, 27, 36, 255};
} // namespace

Game::Game()
    : window_("Robot Factory Escape | A/D: move | Space: jump | P: scaling", 1280, 720),
      renderer_(window_) {
    renderer_.setScalingMode(engine::ScalingMode::Proportional);
    createLevel();
    engine::log::info("Scaffold ready. Cyan: robot; orange: crate; red: drone; green: exit.");
    engine::log::info("A/D: move; Space: jump; P: scaling. Falling off-screen resets the robot.");
    engine::log::info("Avoid the red patrol drone: contact returns you to the start.");
}

engine::EntityId Game::createBox(glm::vec2 position, glm::vec2 size, engine::Color color) {
    const auto id = scene_.createEntity();
    scene_.transform(id).position = position;
    scene_.addShape(id, {.size = size, .color = color, .texture = std::nullopt});
    scene_.addCollider(id, {.size = size});
    return id;
}

void Game::createLevel() {
    // Every box has matching visual and collision bounds, with scale left at 1.
    floor_ = createBox({0.f, 940.f}, {1920.f, 140.f}, {70, 78, 91, 255});
    robot_ = createBox(robotSpawn, {64.f, 80.f}, {72, 205, 230, 255});
    crate_ = createBox({600.f, 780.f}, {180.f, 160.f}, {211, 145, 65, 255});
    drone_ = createBox({1100.f, 840.f}, {90.f, 50.f}, {235, 85, 93, 255});
    exit_ = createBox({1740.f, 800.f}, {100.f, 140.f}, {83, 187, 123, 255});

    // Only the robot participates in gravity. The drone follows a manual path.
    scene_.addRigidBody(robot_);
}

void Game::run() {
    // Exclude construction time from the first gameplay frame.
    clock_.tick();
    while (!window_.shouldClose()) {
        window_.pollEvents();
        if (window_.shouldClose()) {
            break;
        }
        clock_.tick();
        handleInput();
        update(clock_.deltaSeconds());
        render();
    }
}

void Game::handleInput() {
    const bool scalingKeyIsPressed = input_.isKeyPressed(engine::SC::SDL_SCANCODE_P);
    if (scalingKeyIsPressed && !scalingKeyWasPressed_) {
        renderer_.toggleScalingMode();
        engine::log::info("Scaling: {}", renderer_.scalingMode() == engine::ScalingMode::Proportional
                                               ? "proportional" : "constant");
    }
    scalingKeyWasPressed_ = scalingKeyIsPressed;

    const bool moveLeft = input_.isKeyPressed(engine::SC::SDL_SCANCODE_A);
    const bool moveRight = input_.isKeyPressed(engine::SC::SDL_SCANCODE_D);
    const float direction = static_cast<float>(moveRight) - static_cast<float>(moveLeft);
    // Input sets speed; physics applies elapsed time and changes position.
    scene_.getRigidBody(robot_)->velocity.x = direction * robotSpeed;
    const bool jumpKeyIsPressed = input_.isKeyPressed(engine::SC::SDL_SCANCODE_SPACE);
    jumpIfGrounded(*scene_.getRigidBody(robot_), grounded_,
                   jumpKeyIsPressed && !jumpKeyWasPressed_);
    jumpKeyWasPressed_ = jumpKeyIsPressed;
    // STEP 5: Read R to reset positions, velocity, and game state.
}

void Game::update(float deltaSeconds) {
    const std::array solids{floor_, crate_};
    // Advance both movers on the same clock and check contact each small step.
    float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
    while (remaining > 0.f) {
        const float step = std::min(remaining, 1.f / 120.f);
        remaining -= step;
        dronePatrol_.advance(scene_, drone_, step);
        movePlayer(scene_, physics_, robot_, solids, step, grounded_);
        if (resetOnDroneContact(scene_, physics_, robot_, drone_, robotSpawn, grounded_)) {
            engine::log::info("Drone contact! Back to the start.");
            break;
        }
        if (scene_.transform(robot_).position.y > levelSize.y) {
            resetRobot(scene_, robot_, robotSpawn, grounded_);
            break;
        }
    }
    // STEP 5: Detect robot/exit overlap and enter a won state.
}

void Game::render() {
    renderer_.clear({0, 0, 0, 255});
    renderer_.fillRect({0.f, 0.f}, levelSize, background);
    renderer_.drawEntities(scene_);
    renderer_.present();
}
