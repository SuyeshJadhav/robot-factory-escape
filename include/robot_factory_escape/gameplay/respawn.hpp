#pragma once
#include <robot_factory_escape/gameplay/factory_world.hpp>
namespace factory {
void assignSpawn(engine::Scene &, const World &, engine::EntityId, engine::ClientId);
void respawnPlayer(engine::Scene &, engine::EntityId, bool &grounded);
bool inDeathZone(engine::Scene &, const engine::PhysicsSystem &, engine::EntityId);
}
