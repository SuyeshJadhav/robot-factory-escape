#include "test_support.hpp"
#include <robot_factory_escape/networking/world_binding.hpp>
#include <robot_factory_escape/gameplay/respawn.hpp>
#include <array>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <set>

using namespace std::chrono_literals;
namespace {
template<class Poll> void until(Poll poll, const char *message) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (poll()) return;
    std::this_thread::sleep_for(10ms);
  }
  throw std::runtime_error(message);
}
template<class Session> void scenario() {
  constexpr bool peer = std::is_same_v<Session,engine::networking::peerSession>;
  engine::networking::sessionServer server("tcp://127.0.0.1:0");
  engine::Scene serverScene;
  const auto serverWorld = factory::createWorld(serverScene);
  robotNet::bindWorld(server.replicator(),serverWorld);
  server.publishScene(serverScene,0);
  server.start();
  const auto endpoint = "tcp://127.0.0.1:" + std::to_string(server.port());
  struct Client {
    engine::Scene scene;
    factory::World world;
    engine::EntityId robot{};
    std::unique_ptr<Session> session;
    glm::vec2 expected{};
    bool active = false;
  };
  std::array<Client,4> clients;
  auto join = [&](std::size_t index) {
    auto &c = clients[index];
    c.world = factory::createWorld(c.scene);
    c.robot = factory::createPlayer(c.scene,c.world);
    if constexpr (peer) c.session = std::make_unique<Session>(endpoint,"127.0.0.1");
    else c.session = std::make_unique<Session>(endpoint);
    test::require(c.session->join(),"client joins");
    robotNet::bindWorld(c.session->replicator(),c.world);
    test::require(c.session->replicator().track(c.robot) == robotNet::robotId(c.session->id()),
                  "local robot network index");
    factory::assignSpawn(c.scene,c.world,c.robot,c.session->id());
    bool grounded = false;
    factory::respawnPlayer(c.scene,c.robot,grounded);
    // Different directly assigned positions; this does NOT exercise keyboard input.
    c.expected = {700.f + 100.f * static_cast<float>(index),700.f};
    c.scene.transform(c.robot).position = c.expected;
    c.session->start();
    c.active = true;
  };
  std::int64_t tick = 0;
  std::int64_t pausedTick = 0;
  bool firstPaused = false;
  auto pump = [&] {
    ++tick;
    for (std::size_t i = 0; i < clients.size(); ++i) {
      auto &c = clients[i];
      if (!c.active) continue;
      if constexpr (peer) c.session->publishScene(c.scene,i == 0 && firstPaused ? pausedTick : tick);
      else if (i == 0 && firstPaused) c.session->requestSnapshot(pausedTick);
      else c.session->submitScene(c.scene,tick);
    }
    if constexpr (!peer) server.applyClientStates(serverScene);
    server.publishScene(serverScene,tick);
    for (auto &c : clients) {
      if (!c.active) continue;
      if constexpr (peer) c.session->applyUpdates(c.scene);
      else c.session->applySnapshot(c.scene);
      for (const auto &owner : clients) {
        if (!owner.active || &owner == &c) continue;
        const auto id = c.session->replicator().entityOf(robotNet::robotId(owner.session->id()));
        if (id && c.scene.hasEntity(*id)) robotNet::makeRemoteVisual(c.scene,*id);
      }
    }
  };
  auto converged = [&] {
    pump();
    for (const auto &c : clients) {
      if (!c.active) continue;
      if (c.scene.transform(c.robot).position != c.expected) return false;
      if (c.scene.template all<factory::SpawnPoint>().size() != 2 ||
          c.scene.template all<factory::DeathZone>().size() != 4) return false;
      if (c.scene.transform(c.world.movingPlatform).position !=
          serverScene.transform(serverWorld.movingPlatform).position) return false;
      if (c.scene.transform(c.world.drone).position !=
          serverScene.transform(serverWorld.drone).position) return false;
      for (const auto &owner : clients) {
        if (!owner.active || &owner == &c) continue;
        const auto id = c.session->replicator().entityOf(robotNet::robotId(owner.session->id()));
        if (!id || !c.scene.hasEntity(*id) || c.scene.transform(*id).position != owner.expected ||
            c.scene.getRigidBody(*id) || c.scene.getCollider(*id)) return false;
      }
    }
    return true;
  };
  join(0); join(1); join(2);
  until(converged,"three clients exchange movement and complete shared world");
  auto &first = clients[0];
  first.scene.transform(first.robot).position = {2100.f,1000.f};
  engine::PhysicsSystem physics(1800.f);
  test::require(factory::inDeathZone(first.scene,physics,first.robot),"network client's pit collision");
  bool grounded = true;
  factory::respawnPlayer(first.scene,first.robot,grounded);
  first.expected = first.scene.transform(first.robot).position;
  until(converged,"respawn reaches other endpoints before pause");
  pausedTick = tick;
  firstPaused = true;
  factory::advanceWorld(serverScene,1.f);
  join(3); // fourth client joins while the first is frozen
  until(converged,"fourth client gets paused player and new shared mover state");
  std::set<engine::ClientId> ids;
  for (const auto &c : clients) ids.insert(c.session->id());
  test::require(ids.size() == 4,"four distinct client IDs");
  const auto departed = clients[1].session->id();
  std::array<engine::EntityId,4> oldReplicas{};
  for (std::size_t i : {0u,2u,3u}) {
    const auto id = clients[i].session->replicator().entityOf(robotNet::robotId(departed));
    test::require(id.has_value(),"replica exists before departure");
    oldReplicas[i] = *id;
  }
  clients[1].session->leave();
  clients[1].active = false;
  until([&] {
    if (!converged()) return false;
    for (std::size_t i : {0u,2u,3u}) {
      const auto &c = clients[i];
      if (c.session->replicator().entityOf(robotNet::robotId(departed)) ||
          c.scene.hasEntity(oldReplicas[i])) return false;
    }
    if constexpr (!peer)
      if (server.replicator().entityOf(robotNet::robotId(departed))) return false;
    return true;
  },"departure removes replicas while remaining clients continue");
  for (auto &c : clients) if (c.active) c.session->leave();
  server.stop();
}
}
int main() {
  return test::run([] {
    engine::log::init();
    scenario<engine::networking::sessionClient>();
    scenario<engine::networking::peerSession>();
  });
}
