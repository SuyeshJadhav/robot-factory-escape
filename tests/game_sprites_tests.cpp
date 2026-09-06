#include <robot_factory_escape/game_sprites.hpp>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        engine::Window window("Sprite checks", 320, 240);
        engine::Renderer renderer(window);
        engine::Scene scene;
        const auto robot = scene.createEntity();
        const auto drone = scene.createEntity();
        scene.addRigidBody(robot);
        scene.addShape(robot, {.texture = std::nullopt});
        scene.addShape(drone, {.texture = std::nullopt});
        GameSprites sprites;
        sprites.load(renderer, GAME_ASSET_DIR);
        sprites.reset(scene, robot, drone);
        const auto idle = scene.getSpriteAnimation(robot)->sheet;
        scene.getRigidBody(robot)->velocity.x = 300.f;
        sprites.update(scene, robot, true, false, 0.05f);
        const auto walk = scene.getSpriteAnimation(robot)->sheet;
        require(walk != idle, "Movement did not select walking");
        const auto frame = scene.getSpriteAnimation(robot)->currentFrame;
        sprites.update(scene, robot, true, false, 0.05f);
        require(scene.getSpriteAnimation(robot)->currentFrame > frame, "Walking restarts each frame");
        scene.getRigidBody(robot)->velocity.x = -300.f;
        sprites.update(scene, robot, true, false, 0.f);
        require(scene.getSpriteAnimation(robot)->sheet != walk, "Facing did not change");
        sprites.update(scene, robot, false, false, 0.f);
        require(!scene.getSpriteAnimation(robot)->loop, "Jump should hold its final frame");
        const auto frozenFrame = scene.getSpriteAnimation(drone)->currentFrame;
        sprites.update(scene, robot, false, true, 0.1f);
        require(scene.getSpriteAnimation(drone)->currentFrame == frozenFrame, "Win did not freeze sprites");
        // Exercise each loaded animation frame through the real renderer.
        for (float direction : {300.f, -300.f}) {
            for (int state = 0; state < 3; ++state) {
                scene.getRigidBody(robot)->velocity.x = state == 0 ? 0.f : direction;
                for (int i = 0; i < 150; ++i) {
                    sprites.update(scene, robot, state != 2, false, 0.025f);
                    renderer.clear({0, 0, 0, 255});
                    renderer.drawEntities(scene);
                    renderer.present();
                }
            }
        }
        sprites.reset(scene, robot, drone);
        require(scene.getSpriteAnimation(robot)->sheet == idle &&
                scene.getSpriteAnimation(robot)->currentFrame == 0, "Restart did not reset sprites");
        std::cout << "Sprite loading, state transitions, playback, and rendering passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
