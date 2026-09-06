#pragma once

#include <robot_factory_escape/drone_patrol.hpp>

struct GameProgress {
    bool won = false;

    bool checkExit(engine::Scene& scene, engine::PhysicsSystem& physics,
                   engine::EntityId robot, engine::EntityId exit) {
        if (won || !physics.isCollision(scene, robot, exit)) return false;
        won = true;
        scene.getRigidBody(robot)->velocity = {0.f, 0.f};
        return true;
    }

    void restart(engine::Scene& scene, engine::EntityId robot, engine::EntityId drone,
                 glm::vec2 spawn, bool& grounded, DronePatrol& patrol) {
        resetRobot(scene, robot, spawn, grounded);
        patrol = DronePatrol{};
        patrol.advance(scene, drone, 0.f);
        won = false;
    }
};
