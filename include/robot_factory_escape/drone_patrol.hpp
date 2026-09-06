#pragma once

#include <engine/engine.hpp>
#include <cmath>

struct DronePatrol {
    // Positions describe the drone's top-left corner in logical coordinates.
    static constexpr float left = 500.f;
    static constexpr float right = 1600.f;
    static constexpr float height = 840.f;
    static constexpr float speed = 180.f;
    float distance = 600.f; // Start at x=1100, moving right.

    void advance(engine::Scene& scene, engine::EntityId drone, float dt) {
        constexpr float length = right - left;
        distance = std::fmod(distance + speed * dt, 2.f * length);
        const float offset = distance <= length ? distance : 2.f * length - distance;
        scene.transform(drone).position = {left + offset, height};
    }
};

inline void resetRobot(engine::Scene& scene, engine::EntityId robot,
                       glm::vec2 spawn, bool& grounded) {
    scene.transform(robot).position = spawn;
    scene.getRigidBody(robot)->velocity = {0.f, 0.f};
    grounded = false;
}

inline bool resetOnDroneContact(engine::Scene& scene, engine::PhysicsSystem& physics,
                                engine::EntityId robot, engine::EntityId drone,
                                glm::vec2 spawn, bool& grounded) {
    if (!physics.isCollision(scene, robot, drone)) {
        return false;
    }
    resetRobot(scene, robot, spawn, grounded);
    return true;
}
