#pragma once
#include <optional>
#include <robot_factory_escape/gameplay/factory_world.hpp>
namespace factory {
void assignSpawn(engine::Scene &, const World &, engine::EntityId, engine::ClientId);
void respawnPlayer(engine::Scene &, engine::EntityId, bool &grounded);
std::optional<engine::EntityId> deathZoneContact(engine::Scene &, const engine::PhysicsSystem &,
                                                 engine::EntityId);
bool inDeathZone(engine::Scene &, const engine::PhysicsSystem &, engine::EntityId);
} // namespace factory
