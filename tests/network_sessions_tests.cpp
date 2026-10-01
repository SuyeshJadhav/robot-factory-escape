#include <robot_factory_escape/gameplay/player_motion.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <array>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

template <class Poll> void until(Poll poll, const char *message) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (poll())
      return;
    std::this_thread::sleep_for(10ms);
  }
  throw std::runtime_error(message);
}

engine::EntityId addRobot(engine::Scene &scene, float x) {
  const auto id = scene.createEntity();
  scene.transform(id).position = {x, 300.f};
  scene.addShape(id, {.size = {96.f, 128.f}, .color = {255, 210, 60, 255}});
  scene.addRigidBody(id);
  scene.addCollider(id, {.size = {80.f, 128.f}});
  return id;
}

engine::EntityId addPlatform(engine::Scene &scene) {
  const auto id = scene.createEntity();
  scene.transform(id).position = {330.f, 750.f};
  scene.addShape(id, {.size = {300.f, 66.f}, .color = {255, 255, 255, 255}});
  scene.addCollider(id, {.size = {300.f, 24.f}});
  return id;
}

void clientServer() {
  engine::networking::sessionServer server("tcp://127.0.0.1:0");
  engine::Scene serverScene;
  const auto drone = serverScene.createEntity();
  serverScene.transform(drone).position = {1100.f, 440.f};
  require(server.replicator().track(drone) == robotNet::droneId, "drone id");
  const auto serverPlatform = addPlatform(serverScene);
  require(server.replicator().track(serverPlatform) == robotNet::movingPlatformId,
          "server platform id");
  server.publishScene(serverScene, 0);
  server.start();
  const auto endpoint = "tcp://127.0.0.1:" + std::to_string(server.port());
  engine::networking::sessionClient first(endpoint), second(endpoint);
  require(first.join() && second.join(), "two clients join");
  require(first.id() != second.id(), "distinct client ids");
  first.start();
  second.start();
  engine::Scene a, b;
  const auto aRobot = addRobot(a, 120.f);
  const auto bRobot = addRobot(b, 240.f);
  const auto aDrone = a.createEntity();
  const auto bDrone = b.createEntity();
  const auto aPlatform = addPlatform(a);
  const auto bPlatform = addPlatform(b);
  first.replicator().bind(robotNet::droneId, aDrone);
  second.replicator().bind(robotNet::droneId, bDrone);
  first.replicator().bind(robotNet::movingPlatformId, aPlatform);
  second.replicator().bind(robotNet::movingPlatformId, bPlatform);
  first.replicator().track(aRobot);
  second.replicator().track(bRobot);
  until(
      [&] {
        first.submitScene(a, 1);
        second.submitScene(b, 1);
        server.applyClientStates(serverScene);
        server.publishScene(serverScene, 1);
        first.applySnapshot(a);
        second.applySnapshot(b);
        const auto fromA = second.replicator().entityOf(robotNet::robotId(first.id()));
        const auto fromB = first.replicator().entityOf(robotNet::robotId(second.id()));
        return fromA && fromB && b.transform(*fromA).position.x == 120.f &&
               a.transform(*fromB).position.x == 240.f &&
               a.transform(aDrone).position.x == 1100.f &&
               b.transform(bDrone).position.x == 1100.f &&
               a.transform(aPlatform).position.x == 330.f &&
               b.transform(bPlatform).position.x == 330.f;
      },
      "bidirectional client-server replication");
  // Both local robots stand on the old server deck before the next snapshot.
  a.transform(aRobot).position = {360.f, 622.f};
  b.transform(bRobot).position = {390.f, 622.f};
  bool aGrounded = true, bGrounded = true;
  serverScene.transform(serverPlatform).position.x = 420.f;
  until(
      [&] {
        server.publishScene(serverScene, 2);
        first.submitScene(a, 2);
        second.submitScene(b, 2);
        const auto previousA = a.transform(aPlatform).position;
        const auto previousB = b.transform(bPlatform).position;
        first.applySnapshot(a);
        second.applySnapshot(b);
        carryPlayerWithPlatform(a, aRobot, aPlatform, previousA, std::array{aPlatform}, aGrounded);
        carryPlayerWithPlatform(b, bRobot, bPlatform, previousB, std::array{bPlatform}, bGrounded);
        return a.transform(aRobot).position.x == 450.f && b.transform(bRobot).position.x == 480.f &&
               aGrounded && bGrounded && a.transform(aPlatform).position.x == 420.f &&
               b.transform(bPlatform).position.x == 420.f && a.getCollider(aPlatform) &&
               b.getCollider(bPlatform);
      },
      "server-owned moving platform carries both client robots");
  // Pausing local simulation must not pause reception of world/other-player state.
  // Keep the first client's tick and robot position fixed; request snapshots only.
  b.transform(bRobot).position.x = 510.f;
  serverScene.transform(serverPlatform).position.x = 400.f;
  until(
      [&] {
        first.requestSnapshot(2);
        second.submitScene(b, 3);
        server.applyClientStates(serverScene);
        server.publishScene(serverScene, 3);
        first.applySnapshot(a);
        second.applySnapshot(b);
        const auto remoteB = first.replicator().entityOf(robotNet::robotId(second.id()));
        return remoteB && a.transform(*remoteB).position.x == 510.f &&
               a.transform(aPlatform).position.x == 400.f &&
               b.transform(bPlatform).position.x == 400.f &&
               a.transform(aRobot).position.x == 450.f;
      },
      "paused client receives moving world and unpaused client without moving itself");
  const auto remoteInB = *second.replicator().entityOf(robotNet::robotId(first.id()));
  robotNet::makeRemoteVisual(b, remoteInB);
  require(!b.getRigidBody(remoteInB) && !b.getCollider(remoteInB), "remote physics disabled");
  first.leave();
  until(
      [&] {
        server.applyClientStates(serverScene);
        server.publishScene(serverScene, 2);
        second.applySnapshot(b);
        return !second.replicator().entityOf(robotNet::robotId(first.id()));
      },
      "departed robot removed");
  server.stop();
  until([&] { return !second.connected(); }, "server loss reported to client");
  second.leave();
}

void peerToPeer() {
  engine::networking::sessionServer server("tcp://127.0.0.1:0");
  engine::Scene serverScene;
  const auto drone = serverScene.createEntity();
  serverScene.transform(drone).position = {1100.f, 440.f};
  server.replicator().track(drone);
  const auto serverPlatform = addPlatform(serverScene);
  require(server.replicator().track(serverPlatform) == robotNet::movingPlatformId,
          "peer server platform id");
  server.publishScene(serverScene, 0);
  server.start();
  const auto endpoint = "tcp://127.0.0.1:" + std::to_string(server.port());
  engine::networking::peerSession first(endpoint, "127.0.0.1");
  require(first.join(), "first peer joins");
  first.start();
  engine::Scene a;
  const auto aRobot = addRobot(a, 120.f);
  const auto aDrone = a.createEntity();
  const auto aPlatform = addPlatform(a);
  first.replicator().bind(robotNet::droneId, aDrone);
  first.replicator().bind(robotNet::movingPlatformId, aPlatform);
  first.replicator().track(aRobot);
  // The second peer joins after the first published. Repeated publication is
  // required because pub/sub has no replay for a newly connected subscriber.
  first.publishScene(a, 1);
  engine::networking::peerSession second(endpoint, "127.0.0.1");
  require(second.join(), "second peer joins");
  second.start();
  engine::Scene b;
  const auto bRobot = addRobot(b, 240.f);
  const auto bDrone = b.createEntity();
  const auto bPlatform = addPlatform(b);
  second.replicator().bind(robotNet::droneId, bDrone);
  second.replicator().bind(robotNet::movingPlatformId, bPlatform);
  second.replicator().track(bRobot);
  until(
      [&] {
        first.applyUpdates(a);
        second.applyUpdates(b);
        first.publishScene(a, 1); // frozen player, as during a local pause
        second.publishScene(b, 1);
        first.applyUpdates(a);
        second.applyUpdates(b);
        const auto fromA = second.replicator().entityOf(robotNet::robotId(first.id()));
        const auto fromB = first.replicator().entityOf(robotNet::robotId(second.id()));
        return fromA && fromB && b.transform(*fromA).position.x == 120.f &&
               a.transform(*fromB).position.x == 240.f &&
               a.transform(aDrone).position.x == 1100.f &&
               b.transform(bDrone).position.x == 1100.f &&
               a.transform(aPlatform).position.x == 330.f &&
               b.transform(bPlatform).position.x == 330.f;
      },
      "direct peer replication and late join");
  // Both local robots stand on the old server deck before the next snapshot.
  a.transform(aRobot).position = {360.f, 622.f};
  b.transform(bRobot).position = {390.f, 622.f};
  bool aGrounded = true, bGrounded = true;
  serverScene.transform(serverPlatform).position.x = 420.f;
  until(
      [&] {
        server.publishScene(serverScene, 2);
        first.publishScene(a, 2);
        second.publishScene(b, 2);
        const auto previousA = a.transform(aPlatform).position;
        const auto previousB = b.transform(bPlatform).position;
        first.applyUpdates(a);
        second.applyUpdates(b);
        carryPlayerWithPlatform(a, aRobot, aPlatform, previousA, std::array{aPlatform}, aGrounded);
        carryPlayerWithPlatform(b, bRobot, bPlatform, previousB, std::array{bPlatform}, bGrounded);
        return a.transform(aRobot).position.x == 450.f && b.transform(bRobot).position.x == 480.f &&
               aGrounded && bGrounded && a.transform(aPlatform).position.x == 420.f &&
               b.transform(bPlatform).position.x == 420.f && a.getCollider(aPlatform) &&
               b.getCollider(bPlatform);
      },
      "server-owned moving platform carries both peer robots");
  b.transform(bRobot).position.x = 510.f;
  serverScene.transform(serverPlatform).position.x = 400.f;
  until(
      [&] {
        // Republishing the same local tick also refreshes coordinator snapshots.
        first.publishScene(a, 2);
        second.publishScene(b, 3);
        server.publishScene(serverScene, 3);
        first.applyUpdates(a);
        second.applyUpdates(b);
        const auto remoteB = first.replicator().entityOf(robotNet::robotId(second.id()));
        return remoteB && a.transform(*remoteB).position.x == 510.f &&
               a.transform(aPlatform).position.x == 400.f &&
               b.transform(bPlatform).position.x == 400.f &&
               a.transform(aRobot).position.x == 450.f;
      },
      "paused peer receives moving world and unpaused peer without moving itself");
  first.leave();
  until(
      [&] {
        second.applyUpdates(b);
        return !second.replicator().entityOf(robotNet::robotId(first.id()));
      },
      "peer departure cleanup");
  second.leave();
  server.stop();
}
} // namespace

int main() {
  engine::log::init();
  clientServer();
  peerToPeer();
}
