#include <robot_factory_escape/drone_patrol.hpp>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        engine::Scene scene;
        engine::PhysicsSystem physics{980.f};
        const auto drone = scene.createEntity();
        scene.addCollider(drone, {.size = {90.f, 50.f}});
        DronePatrol patrol;
        patrol.advance(scene, drone, 0.f);
        require(scene.transform(drone).position.x == 1100.f, "Incorrect starting position");
        patrol.distance = 0.f;
        patrol.advance(scene, drone, 1100.f / 180.f);
        require(std::abs(scene.transform(drone).position.x - 1600.f) < 0.01f, "Missed right endpoint");
        patrol.advance(scene, drone, 1.f);
        require(std::abs(scene.transform(drone).position.x - 1420.f) < 0.01f, "Did not reverse");
        for (int i = 0; i < 10000; ++i) {
            patrol.advance(scene, drone, 1.f / 120.f);
            const auto p = scene.transform(drone).position;
            require(p.x >= DronePatrol::left && p.x <= DronePatrol::right &&
                    p.y == DronePatrol::height, "Patrol escaped bounds");
        }
        require(scene.getRigidBody(drone) == nullptr, "Drone must not receive gravity");
        const auto robot = scene.createEntity();
        scene.addCollider(robot, {.size = {64.f, 80.f}});
        scene.addRigidBody(robot, {.velocity = {300.f, -200.f}});
        scene.transform(robot).position = scene.transform(drone).position;
        bool grounded = true;
        constexpr glm::vec2 spawn{120.f, 300.f};
        require(resetOnDroneContact(scene, physics, robot, drone, spawn, grounded), "Contact missed");
        const auto position = scene.transform(robot).position;
        const auto velocity = scene.getRigidBody(robot)->velocity;
        require(position.x == spawn.x && position.y == spawn.y, "Spawn not restored");
        require(velocity.x == 0.f && velocity.y == 0.f && !grounded, "Reset left motion or grounding");
        require(!resetOnDroneContact(scene, physics, robot, drone, spawn, grounded), "False contact at spawn");
        std::cout << "Patrol bounds, reversal, floating, contact, and reset checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
