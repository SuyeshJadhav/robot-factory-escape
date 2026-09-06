#include <robot_factory_escape/game_progress.hpp>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        engine::Scene scene;
        engine::PhysicsSystem physics{980.f};
        const auto robot = scene.createEntity();
        scene.addRigidBody(robot, {.velocity = {300.f, 100.f}});
        scene.addCollider(robot, {.size = {64.f, 80.f}});
        const auto exit = scene.createEntity();
        scene.transform(exit).position = {1740.f, 800.f};
        scene.addCollider(exit, {.size = {100.f, 140.f}});
        const auto drone = scene.createEntity();
        DronePatrol patrol;
        patrol.advance(scene, drone, 2.f);
        GameProgress progress;
        require(!progress.checkExit(scene, physics, robot, exit) && !progress.won, "Won away from exit");
        scene.transform(robot).position = {1730.f, 860.f};
        require(progress.checkExit(scene, physics, robot, exit) && progress.won, "Exit did not win");
        const auto velocity = scene.getRigidBody(robot)->velocity;
        require(velocity.x == 0.f && velocity.y == 0.f, "Win did not stop robot");
        require(!progress.checkExit(scene, physics, robot, exit), "Win triggered twice");
        bool grounded = true;
        progress.restart(scene, robot, drone, {120.f, 300.f}, grounded, patrol);
        require(!progress.won && !grounded, "Restart retained state");
        const auto position = scene.transform(robot).position;
        require(position.x == 120.f && position.y == 300.f, "Restart spawn incorrect");
        require(scene.transform(drone).position.x == 1100.f && patrol.distance == 600.f,
                "Restart did not restore patrol");
        scene.transform(robot).position = {1740.f, 800.f};
        require(progress.checkExit(scene, physics, robot, exit), "Cannot win again after restart");
        std::cout << "Exit trigger, one-shot win, velocity, and restart checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
