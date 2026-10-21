#pragma once
#include <robot_factory_escape/gameplay/factory_world.hpp>
#include <robot_factory_escape/networking/network_state.hpp>
namespace robotNet {
inline void bindWorld(engine::networking::sceneReplicator &rep, const factory::World &world) {
  rep.bind(droneId,world.drone);
  rep.bind(movingPlatformId,world.movingPlatform);
}
}
