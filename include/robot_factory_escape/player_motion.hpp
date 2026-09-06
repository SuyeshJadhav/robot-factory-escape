#pragma once

#include <engine/engine.hpp>
#include <span>

// Game rules layered on top of the engine's movement and overlap queries.
bool jumpIfGrounded(engine::RigidBody& body, bool& grounded, bool newPress);
void movePlayer(engine::Scene& scene, engine::PhysicsSystem& physics,
                engine::EntityId player, std::span<const engine::EntityId> solids,
                float deltaSeconds, bool& grounded);
