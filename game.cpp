#include "game.hpp"

namespace {
constexpr glm::vec2 levelSize{1920.f, 1080.f};
constexpr engine::Color background{22, 27, 36, 255};
} // namespace

Game::Game()
    : window_("Robot Factory Escape | Scaffold | P: scaling", 1280, 720),
      renderer_(window_) {
    renderer_.setScalingMode(engine::ScalingMode::Proportional);
    createLevel();
    engine::log::info("Scaffold ready. Cyan: robot; orange: crate; red: drone; green: exit.");
    engine::log::info("Press P to toggle scaling. Gameplay is left for the next build steps.");
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
    robot_ = createBox({120.f, 860.f}, {64.f, 80.f}, {72, 205, 230, 255});
    crate_ = createBox({600.f, 780.f}, {180.f, 160.f}, {211, 145, 65, 255});
    drone_ = createBox({1100.f, 840.f}, {90.f, 50.f}, {235, 85, 93, 255});
    exit_ = createBox({1740.f, 800.f}, {100.f, 140.f}, {83, 187, 123, 255});

    // Only the robot participates in gravity. The drone will follow a manual path.
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

    // STEP 2: Read A/D and set scene_.getRigidBody(robot_)->velocity.x.
    // STEP 3: On a new Space press, jump only when grounded.
    // STEP 5: Read R to reset positions, velocity, and game state.
}

void Game::update([[maybe_unused]] float deltaSeconds) {
    // STEP 2: Advance gravity with physics_.step(scene_, deltaSeconds).
    // Leave physics paused until STEP 3, otherwise the robot falls through the floor.
    // STEP 3: Save the previous position before stepping, then check and resolve
    // robot/floor and robot/crate overlaps; update your grounded flag.
    // STEP 4: Move the drone on a repeating path and reset the robot on contact.
    // STEP 5: Detect robot/exit overlap and enter a won state.
}

void Game::render() {
    renderer_.clear({0, 0, 0, 255});
    renderer_.fillRect({0.f, 0.f}, levelSize, background);
    renderer_.drawEntities(scene_);
    renderer_.present();
}
