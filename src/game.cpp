#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/player_motion.hpp>

#include <array>
#include <algorithm>
#include <string>
#include <utility>

namespace {
constexpr glm::vec2 levelSize{1920.f, 1080.f};
constexpr glm::vec2 robotSpawn{120.f, 300.f};
constexpr float robotSpeed = 300.f; // Logical pixels per second.
} // namespace

Game::Game()
    : window_("Robot Factory Escape | A/D: move | Space: jump | P: scaling | R: restart", 1280, 720),
      renderer_(window_) {
    renderer_.setScalingMode(engine::ScalingMode::Proportional);
    createLevel();
    engine::log::info("Factory ready. Cyborg: player; red drone: hazard; cyan door: exit.");
    engine::log::info("A/D: move; Space: jump; P: scaling. Falling off-screen resets the robot.");
    engine::log::info("Avoid the red patrol drone: contact returns you to the start.");
    engine::log::info("Reach the cyan factory exit to win. Press R to restart at any time.");
}

engine::EntityId Game::createBox(glm::vec2 position, glm::vec2 size, engine::Color color) {
    const auto id = scene_.createEntity();
    scene_.transform(id).position = position;
    scene_.addShape(id, {.size = size, .color = color, .texture = std::nullopt});
    scene_.addCollider(id, {.size = size});
    return id;
}

void Game::createLevel() {
    const std::string assetDir = GAME_ASSET_DIR;
    backdropTexture_ = renderer_.loadTexture(assetDir + "background/cyberpunk_background_backdropa_idle.png");
    const auto platformTexture = renderer_.loadTexture(assetDir + "platform/cyberpunk_platform_antigravcart_idle.png");
    // Most boxes have matching visual and collision bounds, with scale left at 1.
    floor_ = createBox({0.f, 940.f}, {1920.f, 140.f}, {70, 78, 91, 255});
    robot_ = createBox(robotSpawn, {96.f, 128.f}, {72, 205, 230, 255});
    // The cyborg is narrower than its animation cell; keep the full visual height
    // while tightening horizontal collision to the character's body.
    scene_.addCollider(robot_, {.size = {80.f, 128.f}});

    // The source platform has a wide solid deck and transparent hanging space.
    // Render the deck at its natural proportions, but collide only with its top.
    platform_ = scene_.createEntity();
    scene_.transform(platform_).position = {570.f, 735.f};
    scene_.addShape(platform_, {.size = {360.f, 79.f}, .color = {255, 255, 255, 255},
                                .texture = std::nullopt});
    scene_.addCollider(platform_, {.size = {360.f, 28.f}});
    engine::SpriteSheetLayout platformLayout;
    platformLayout.frames.push_back({{0.f, 0.f}, {1024.f, 224.f}});
    const auto platformSheet = renderer_.createSpriteSheet(platformTexture, std::move(platformLayout));
    scene_.addSpriteAnimation(platform_, engine::SpriteAnimation::uniform(platformSheet, {0}, 1.f));

    drone_ = createBox({1100.f, 840.f}, {130.f, 80.f}, {235, 85, 93, 255});
    // The trigger remains a simple collider; drawExit provides the themed artwork.
    exit_ = scene_.createEntity();
    scene_.transform(exit_).position = {1740.f, 800.f};
    scene_.addCollider(exit_, {.size = {100.f, 140.f}});

    // Only the robot participates in gravity. The drone follows a manual path.
    scene_.addRigidBody(robot_);
    sprites_.load(renderer_, assetDir);
    sprites_.reset(scene_, robot_, drone_);
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

    const bool restartKeyIsPressed = input_.isKeyPressed(engine::SC::SDL_SCANCODE_R);
    const bool restartPressed = restartKeyIsPressed && !restartKeyWasPressed_;
    restartKeyWasPressed_ = restartKeyIsPressed;
    const bool jumpKeyIsPressed = input_.isKeyPressed(engine::SC::SDL_SCANCODE_SPACE);
    const bool jumpPressed = jumpKeyIsPressed && !jumpKeyWasPressed_;
    jumpKeyWasPressed_ = jumpKeyIsPressed;
    if (restartPressed) {
        progress_.restart(scene_, robot_, drone_, robotSpawn, grounded_, dronePatrol_);
        sprites_.reset(scene_, robot_, drone_);
        engine::log::info("Restarted. Reach the exit!");
        return;
    }
    if (progress_.won) return;

    const bool moveLeft = input_.isKeyPressed(engine::SC::SDL_SCANCODE_A);
    const bool moveRight = input_.isKeyPressed(engine::SC::SDL_SCANCODE_D);
    const float direction = static_cast<float>(moveRight) - static_cast<float>(moveLeft);
    // Input sets speed; physics applies elapsed time and changes position.
    scene_.getRigidBody(robot_)->velocity.x = direction * robotSpeed;
    jumpIfGrounded(*scene_.getRigidBody(robot_), grounded_, jumpPressed);
}

void Game::update(float deltaSeconds) {
    if (progress_.won) return;
    const std::array solids{floor_, platform_};
    // Advance both movers on the same clock and check contact each small step.
    float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
    while (remaining > 0.f) {
        const float step = std::min(remaining, 1.f / 120.f);
        remaining -= step;
        dronePatrol_.advance(scene_, drone_, step);
        movePlayer(scene_, physics_, robot_, solids, step, grounded_);
        if (resetOnDroneContact(scene_, physics_, robot_, drone_, robotSpawn, grounded_)) {
            sprites_.reset(scene_, robot_, drone_);
            engine::log::info("Drone contact! Back to the start.");
            break;
        }
        if (scene_.transform(robot_).position.y > levelSize.y) {
            resetRobot(scene_, robot_, robotSpawn, grounded_);
            sprites_.reset(scene_, robot_, drone_);
            break;
        }
        if (progress_.checkExit(scene_, physics_, robot_, exit_)) {
            engine::log::info("You escaped! Press R to play again.");
            break;
        }
    }
    sprites_.update(scene_, robot_, grounded_, progress_.won, deltaSeconds);
}

void Game::render() {
    renderer_.clear({0, 0, 0, 255});
    // Draw the backdrop first, outside the unordered entity collection.
    renderer_.drawTexture(backdropTexture_, {0.f, 0.f}, levelSize);
    drawExit();
    renderer_.drawEntities(scene_);
    renderer_.present();
}

void Game::drawExit() {
    const auto position = scene_.transform(exit_).position;
    const engine::Color frame = progress_.won ? engine::Color{80, 255, 140, 255}
                                               : engine::Color{50, 220, 235, 255};
    const engine::Color indicator = progress_.won ? engine::Color{80, 255, 100, 255}
                                                   : engine::Color{255, 170, 35, 255};

    // A compact cyberpunk factory door built from engine rectangles.
    renderer_.fillRect(position + glm::vec2{-18.f, -20.f}, {136.f, 160.f}, {7, 18, 27, 255});
    renderer_.fillRect(position + glm::vec2{-12.f, -14.f}, {124.f, 154.f}, frame);
    renderer_.fillRect(position + glm::vec2{-5.f, -7.f}, {110.f, 147.f}, {12, 29, 39, 255});
    renderer_.fillRect(position + glm::vec2{3.f, 2.f}, {94.f, 138.f}, {23, 46, 57, 255});
    renderer_.fillRect(position + glm::vec2{48.f, 2.f}, {4.f, 138.f}, frame);

    // Hazard rails and a status panel tie the door to the orange/cyan factory palette.
    renderer_.fillRect(position + glm::vec2{-12.f, 8.f}, {7.f, 22.f}, {255, 116, 24, 255});
    renderer_.fillRect(position + glm::vec2{-12.f, 42.f}, {7.f, 22.f}, {255, 116, 24, 255});
    renderer_.fillRect(position + glm::vec2{105.f, 8.f}, {7.f, 22.f}, {255, 116, 24, 255});
    renderer_.fillRect(position + glm::vec2{105.f, 42.f}, {7.f, 22.f}, {255, 116, 24, 255});
    renderer_.fillRect(position + glm::vec2{27.f, 14.f}, {46.f, 25.f}, {5, 15, 22, 255});
    renderer_.fillRect(position + glm::vec2{34.f, 20.f}, {32.f, 13.f}, indicator);
    renderer_.fillRect(position + glm::vec2{76.f, 76.f}, {8.f, 22.f}, indicator);
}
