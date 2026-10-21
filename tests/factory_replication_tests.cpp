#include "test_support.hpp"
#include <robot_factory_escape/networking/world_binding.hpp>
#include <robot_factory_escape/gameplay/respawn.hpp>

int main() {
  return test::run([] {
    for (const auto format : {engine::networking::SnapshotFormat::Binary,
                              engine::networking::SnapshotFormat::Json}) {
      engine::Scene source, destination;
      const auto sw = factory::createWorld(source);
      destination.createEntity(); // local EntityIds deliberately differ
      const auto dw = factory::createWorld(destination);
      const auto local = factory::createPlayer(destination,dw);
      engine::networking::sceneReplicator sender(engine::kServerId), receiver(2);
      sender.setFormat(format);
      robotNet::bindWorld(sender,sw);
      robotNet::bindWorld(receiver,dw);
      test::require(receiver.track(local) == robotNet::robotId(2), "robot local index stays zero");
      const auto count = destination.transforms().size();
      factory::advanceWorld(source,1.f);
      for (int repeat = 0; repeat < 2; ++repeat) {
        test::require(receiver.apply(destination,sender.encodeOwned(source),engine::kServerId),
                      "world snapshot applies");
        test::require(receiver.entityOf(robotNet::droneId) == dw.drone &&
                      receiver.entityOf(robotNet::movingPlatformId) == dw.movingPlatform &&
                      destination.transforms().size() == count, "bound world never duplicates");
      }
      test::require(destination.transform(dw.movingPlatform).position.x == 390.f,
                    "server platform state arrives");
      test::require(test::near(destination.transform(dw.drone).position.x,
                               source.transform(sw.drone).position.x) &&
                    test::near(destination.transform(dw.drone).position.y,
                               source.transform(sw.drone).position.y), "circular drone state arrives");
      test::require(destination.has<factory::Orbit>(dw.drone) &&
                    destination.all<factory::SpawnPoint>().size() == 2 &&
                    destination.all<factory::DeathZone>().size() == 4,
                    "local components survive built-in snapshots");
      test::require(!sender.netIdOf(sw.spawns[0]), "hidden static objects are not tracked");
      engine::Scene other;
      const auto ow = factory::createWorld(other);
      const auto remoteRobot = factory::createPlayer(other,ow);
      factory::assignSpawn(other,ow,remoteRobot,3);
      bool grounded = true;
      factory::respawnPlayer(other,remoteRobot,grounded);
      engine::networking::sceneReplicator remote(3);
      remote.setFormat(format);
      remote.track(remoteRobot);
      test::require(receiver.apply(destination,remote.encodeOwned(other),3), "respawn replicates");
      const auto replica = receiver.entityOf(robotNet::robotId(3));
      test::require(replica && destination.transform(*replica).position.x == 120.f,
                    "replicated respawn remains world-space");
      robotNet::makeRemoteVisual(destination,*replica);
      test::require(!destination.getRigidBody(*replica) && !destination.getCollider(*replica),
                    "remote player stays visual-only");
      receiver.dropOwner(destination,3);
      test::require(!destination.hasEntity(*replica) && destination.hasEntity(dw.drone) &&
                    destination.hasEntity(local) && destination.hasEntity(dw.spawns[0]),
                    "departure removes only the remote owner");
    }
  });
}
