#include <robot_factory_escape/network_state.hpp>
#include <engine/simulationThread.hpp>
#include <engine/timeline.hpp>

#include <cassert>
#include <bit>
#include <limits>

int main() {
  using engine::networking::ServerEvent;
  using Kind = ServerEvent::Kind;
  engine::Scene scene;
  robotNet::RemoteRobots remotes(2);
  const auto state = [](engine::ClientId id, std::int64_t tick, float x, float y) {
    return ServerEvent{Kind::Player, id, tick, robotNet::encode({x, y})};
  };

  assert(!robotNet::decode({}).has_value());
  assert(!robotNet::decode(engine::networking::Bytes{std::byte{0}, std::byte{1}}).has_value());
  auto trailing = robotNet::encode({1.f, 2.f});
  trailing.push_back(std::byte{0});
  assert(!robotNet::decode(trailing).has_value());
  assert(!robotNet::decode(robotNet::encode({std::numeric_limits<float>::infinity(), 1.f})).has_value());
  assert(!robotNet::decode(robotNet::encode({0.f, std::numeric_limits<float>::quiet_NaN()})).has_value());
  assert(!remotes.apply(scene, state(2, 4, 10.f, 20.f)));
  assert(remotes.size() == 0);
  assert(!remotes.apply(scene, {Kind::Player, 3, 2, {std::byte{0}}}));
  assert(remotes.size() == 0);
  assert(!remotes.apply(scene, state(3, 2, std::numeric_limits<float>::infinity(), 0.f)));
  assert(remotes.apply(scene, state(3, 10, 100.f, 200.f)));
  const auto entity3 = *remotes.entity(3);
  assert(scene.hasEntity(entity3));
  assert(scene.transform(entity3).position.x == 100.f);
  assert(remotes.apply(scene, state(4, 1, 400.f, 500.f)));
  assert(remotes.size() == 2);
  assert(!remotes.apply(scene, state(3, 9, 999.f, 999.f)));
  assert(scene.transform(entity3).position.x == 100.f);
  assert(!remotes.apply(scene, state(3, 11, std::numeric_limits<float>::quiet_NaN(), 0.f)));
  assert(scene.transform(entity3).position.x == 100.f);
  assert(remotes.apply(scene, state(3, 10, 105.f, 200.f)));
  assert(scene.transform(entity3).position.x == 105.f);
  assert(remotes.apply(scene, state(4, 2, 410.f, 500.f)));
  assert(scene.transform(*remotes.entity(4)).position.x == 410.f);
  assert(remotes.apply(scene, {Kind::PlayerLeft, 3, 0, {}}));
  assert(!scene.hasEntity(entity3) && !remotes.entity(3));
  assert(remotes.size() == 1);
  assert(remotes.apply(scene, state(3, 1, 50.f, 60.f)));
  assert(scene.transform(*remotes.entity(3)).position.x == 50.f);

  const auto drone = scene.createEntity();
  robotNet::DroneState droneState;
  const auto world = [](std::int64_t tick, float x, float y) {
    return ServerEvent{Kind::World, 0, tick, robotNet::encode({x, y})};
  };
  assert(!droneState.apply(scene, drone, state(4, 12, 1.f, 2.f)));
  assert(!droneState.apply(scene, drone, {Kind::World, 0, 12, {std::byte{0}}}));
  assert(!droneState.apply(scene, drone, world(12, std::numeric_limits<float>::infinity(), 0.f)));
  assert(droneState.apply(scene, drone, world(12, 1100.f, 440.f)));
  assert(!droneState.apply(scene, drone, world(11, 1200.f, 440.f)));
  assert(droneState.lastTick() == 12);
  assert(scene.transform(drone).position.x == 1100.f);

  // Test local pause, speed scaling, and remote update processing while paused
  std::int64_t clock = 0;
  engine::Timeline realTime(&clock, 60);
  engine::Timeline gameTime(realTime, 60);
  engine::Scene simScene;
  const auto localRobot = simScene.createEntity();
  simScene.transform(localRobot).position = {120.f, 300.f};
  const auto simDrone = simScene.createEntity();
  simScene.transform(simDrone).position = {1100.f, 440.f};

  robotNet::RemoteRobots simRemotes(1);
  robotNet::DroneState simDroneState;

  int localTicks = 0;
  engine::SimulationThread sim(std::move(simScene), gameTime, [&](const engine::TickContext& ctx) {
    ++localTicks;
    ctx.scene.transform(localRobot).position.x += 10.f;
  });

  sim.advanceFrame();
  ++clock;
  sim.advanceFrame();
  assert(localTicks == 1);
  auto frame = sim.takeRenderFrame();
  assert(frame.has_value());
  assert(frame->scene.transform(localRobot).position.x == 130.f);

  // Pause local simulation
  sim.pause();
  sim.advanceFrame();
  ++clock;
  sim.advanceFrame();
  assert(localTicks == 1); // Frozen!

  // Post drone and remote player updates while paused
  sim.post([&](engine::Scene& s, engine::Timeline&) {
    simDroneState.apply(s, simDrone, world(20, 1160.f, 440.f));
    simRemotes.apply(s, state(2, 5, 250.f, 300.f));
  });
  sim.advanceFrame();
  frame = sim.takeRenderFrame();
  assert(frame.has_value());
  assert(frame->status.paused);
  assert(localTicks == 1); // Still frozen!
  assert(frame->scene.transform(localRobot).position.x == 130.f); // Local robot didn't move
  assert(frame->scene.transform(simDrone).position.x == 1160.f); // Drone updated!
  const auto remoteEntity2 = *simRemotes.entity(2);
  assert(frame->scene.transform(remoteEntity2).position.x == 250.f); // Remote robot updated!

  // Test speed scaling (0.5x, 1x, 2x)
  sim.unpause();
  sim.setSpeed(2.f);
  sim.advanceFrame();
  assert(gameTime.speedMultiplier() == 2.f);
  sim.setSpeed(0.5f);
  sim.advanceFrame();
  assert(gameTime.speedMultiplier() == 0.5f);
  sim.setSpeed(1.f);
  sim.advanceFrame();
  assert(gameTime.speedMultiplier() == 1.f);
}
