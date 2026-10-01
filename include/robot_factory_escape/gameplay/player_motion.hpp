#pragma once

#include <engine/engine.hpp>
#include <span>

void resetRobot(engine::Scene &scene, engine::EntityId robot, glm::vec2 spawn, bool &grounded);

// Game rules layered on top of the engine's movement and overlap queries.
bool jumpIfGrounded(engine::RigidBody &body, bool &grounded, bool newPress);
bool playerIsSupported(engine::Scene &scene, engine::EntityId player,
                       std::span<const engine::EntityId> solids);

// Apply a horizontal platform's observed displacement, never its velocity.
// Called immediately after local patrol or replication changes the platform.
// Skip this while paused; discarded deltas must not accumulate until resume.
void carryPlayerWithPlatform(engine::Scene &scene, engine::EntityId player,
                             engine::EntityId platform, glm::vec2 previousPosition,
                             std::span<const engine::EntityId> solids, bool &grounded);
void movePlayer(engine::Scene &scene, engine::PhysicsSystem &physics, engine::EntityId player,
                std::span<const engine::EntityId> solids, float deltaSeconds, bool &grounded);
