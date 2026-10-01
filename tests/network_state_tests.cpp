#include <engine/physics.hpp>
#include <robot_factory_escape/gameplay/moving_platform.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <cassert>

int main() {
  assert(robotNet::droneId == engine::makeNetId(0, 0));
  assert(robotNet::movingPlatformId == engine::makeNetId(0, 1));
  assert(robotNet::robotId(2) == engine::makeNetId(2, 0));
  engine::Scene platformScene;
  const auto platform = platformScene.createEntity();
  MovingPlatformPatrol platformPatrol;
  platformPatrol.advance(platformScene, platform, 0.f);
  assert(platformScene.transform(platform).position.x == 330.f);
  platformPatrol.advance(platformScene, platform, 2.f);
  assert(platformScene.transform(platform).position.x == 450.f);
  platformPatrol.advance(platformScene, platform, 2.f);
  assert(platformScene.transform(platform).position.x == 330.f);
  const auto bytes = robotNet::encode({5, 32, 12});
  const auto state = robotNet::decode(bytes);
  assert(state && state->clip == 5 && state->frame == 32 && state->elapsedMilliseconds == 12);
  assert(!robotNet::decode({}));
  auto bad = bytes;
  bad[0] = std::byte{2};
  assert(!robotNet::decode(bad));
  bad = bytes;
  bad[1] = std::byte{6};
  assert(!robotNet::decode(bad));

  engine::Scene ownerScene;
  const auto owned = ownerScene.createEntity();
  ownerScene.transform(owned).position = {50.f, 70.f};
  ownerScene.addShape(owned, {.size = {96.f, 128.f}, .color = {255, 210, 60, 255}});
  ownerScene.addRigidBody(owned, {.velocity = {1.f, 0.f}});
  ownerScene.addCollider(owned, {.size = {80.f, 128.f}});
  engine::networking::sceneReplicator sender(2);
  assert(sender.track(owned) == robotNet::robotId(2));
  sender.setExtra(robotNet::robotId(2), bytes);

  engine::Scene receiverScene;
  engine::networking::sceneReplicator receiver(1);
  assert(receiver.apply(receiverScene, sender.encodeOwned(ownerScene), 2));
  const auto remote = *receiver.entityOf(robotNet::robotId(2));
  assert(receiverScene.transform(remote).position.x == 50.f);
  assert(receiverScene.getRigidBody(remote));
  assert(receiverScene.getCollider(remote));
  assert(robotNet::decode(*receiver.extraOf(robotNet::robotId(2))));
  robotNet::makeRemoteVisual(receiverScene, remote);
  assert(!receiverScene.getRigidBody(remote));
  assert(!receiverScene.getCollider(remote));
  engine::PhysicsSystem physics(1800.f);
  physics.step(receiverScene, 1.f / 60.f);
  assert(receiverScene.transform(remote).position.x == 50.f);
  assert(receiverScene.transform(remote).position.y == 70.f);
  receiver.dropOwner(receiverScene, 2);
  assert(!receiverScene.hasEntity(remote));

  std::int64_t clock = 0;
  engine::Timeline wall(&clock, 60);
  engine::Timeline game(wall, 60);
  engine::Scene simScene;
  const auto local = simScene.createEntity();
  int ticks = 0;
  engine::SimulationThread simulation(std::move(simScene), game,
                                      [&](const engine::TickContext &context) {
                                        ++ticks;
                                        context.scene.transform(local).position.x += 10.f;
                                      });
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  assert(ticks == 1);
  simulation.pause();
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  assert(ticks == 1);
  simulation.post([](engine::Scene &scene, engine::Timeline &) {
    const auto other = scene.createEntity();
    scene.transform(other).position = {250.f, 300.f};
  });
  simulation.advanceFrame();
  const auto frame = simulation.takeRenderFrame();
  assert(frame && frame->status.paused && frame->scene.transforms().size() == 2);
  assert(ticks == 1);
  simulation.unpause();
  simulation.setSpeed(2.f);
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  assert(ticks == 3);
  simulation.setSpeed(0.5f);
  simulation.advanceFrame();
  clock += 2;
  simulation.advanceFrame();
  assert(ticks == 4);
}
