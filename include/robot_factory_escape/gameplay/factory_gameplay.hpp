#pragma once
#include <robot_factory_escape/gameplay/respawn.hpp>
#include <robot_factory_escape/gameplay/game_progress.hpp>
namespace factory {
enum class TickResult { None, DroneHit, DeathZone, Won };
TickResult tickPlayer(engine::Scene &, engine::PhysicsSystem &, const World &,
                      engine::EntityId, bool &, GameProgress &, float dt, bool ownsWorld);
void restartPlayer(engine::Scene &, engine::EntityId, bool &, GameProgress &, bool ownsWorld);
}
