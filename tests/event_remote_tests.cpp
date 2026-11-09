#include "test_support.hpp"
#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <limits>

robotNet::LifeEvent life(engine::ClientId owner, std::uint32_t revision,
                         robotNet::LifeKind kind) {
  robotNet::LifeEvent event;
  event.entity = robotNet::robotId(owner);
  event.revision = revision;
  event.kind = kind;
  event.position = {360.f, 300.f};
  return event;
}

void visualAndPoseReconciliation() {
  for (const auto format : {engine::networking::SnapshotFormat::Binary,
                           engine::networking::SnapshotFormat::Json}) {
    engine::Scene source, destination;
    const auto ownerRobot = source.createEntity();
    source.addShape(ownerRobot, {.size = {96.f, 128.f}});
    source.addRigidBody(ownerRobot);
    source.addCollider(ownerRobot, {.size = {80.f, 128.f}});
    source.transform(ownerRobot).position = {120.f, 300.f};
    source.transform(ownerRobot).scale = {2.f, 3.f};
    const auto local = destination.createEntity();
    destination.addShape(local, {.size = {96.f, 128.f}});
    destination.transform(local).position = {50.f, 60.f};
    engine::networking::sceneReplicator sender(1), receiver(2);
    sender.setFormat(format);
    const auto id = sender.track(ownerRobot);
    receiver.track(local);
    sender.setExtra(id, robotNet::encode({0, 0, 0, 1, true}));
    const auto oldAlive = sender.encodeOwned(source);
    test::require(receiver.apply(destination, oldAlive, 1), "real initial snapshot applies");
    const auto remote = *receiver.entityOf(id);
    robotNet::RemoteLives cache;
    const engine::Shape fallback{.size = {96.f, 128.f}, .color = {255, 210, 60, 255}};
    test::require(robotNet::reconcileRemotePose(destination, receiver, cache, id),
                  "valid initial snapshot establishes an accepted full pose");
    destination.addSpriteAnimation(remote, {0, {{0, 0.2f}}, true});
    cache.observe(life(1, 2, robotNet::LifeKind::Death));
    robotNet::enforceRemoteLife(destination, local, receiver, cache, id, fallback);
    test::require(!destination.getShape(remote) && !destination.getSpriteAnimation(remote) &&
                      !destination.getRigidBody(remote) && !destination.getCollider(remote),
                  "event-only death hides artwork and strips remote physics");
    receiver.apply(destination, oldAlive, 1);
    test::require(!robotNet::reconcileRemotePose(destination, receiver, cache, id),
                  "stale alive snapshot is rejected after death");
    destination.addSpriteAnimation(remote, {0, {{0, 0.2f}}, true});
    robotNet::enforceRemoteLife(destination, local, receiver, cache, id, fallback);
    test::require(!destination.getShape(remote) && !destination.getSpriteAnimation(remote),
                  "snapshot and sprite reattachment cannot resurrect a dead robot");
    cache.observe(life(1, 3, robotNet::LifeKind::Spawn));
    robotNet::enforceRemoteLife(destination, local, receiver, cache, id, fallback);
    test::require(destination.getShape(remote) &&
                      destination.transform(remote).position == glm::vec2{360.f, 300.f} &&
                      destination.transform(remote).scale == glm::vec2{2.f, 3.f} &&
                      !cache.takeSpawnPosition(id),
                  "event-only spawn restores artwork and consumes its pose once");
    source.transform(ownerRobot).position = {2100.f, 1000.f};
    source.transform(ownerRobot).scale = {9.f, 9.f};
    sender.setExtra(id, robotNet::encode({0, 0, 0, 2, false}));
    const auto stale = sender.encodeOwned(source);
    receiver.apply(destination, stale, 1);
    test::require(!robotNet::reconcileRemotePose(destination, receiver, cache, id) &&
                      destination.transform(remote).position == glm::vec2{360.f, 300.f} &&
                      destination.transform(remote).scale == glm::vec2{2.f, 3.f} &&
                      cache.alive(id) == true,
                  "stale_snapshot_after_consumed_spawn_preserves_accepted_transform");
    source.transform(ownerRobot).position = {500.f, 400.f};
    source.transform(ownerRobot).scale = {4.f, 5.f};
    sender.setExtra(id, robotNet::encode({0, 0, 0, 3, true}));
    receiver.apply(destination, sender.encodeOwned(source), 1);
    test::require(robotNet::reconcileRemotePose(destination, receiver, cache, id) &&
                      destination.transform(remote).position == glm::vec2{500.f, 400.f},
                  "consistent same-revision movement advances the accepted pose");
    receiver.apply(destination, stale, 1);
    test::require(!robotNet::reconcileRemotePose(destination, receiver, cache, id) &&
                      destination.transform(remote).position == glm::vec2{500.f, 400.f} &&
                      destination.transform(remote).scale == glm::vec2{4.f, 5.f},
                  "later stale snapshot restores newest movement, not original spawn");
    for (const auto& metadata : {robotNet::encode({0, 0, 0, 3, false}),
                                engine::networking::Bytes{},
                                engine::networking::Bytes{std::byte{99}}}) {
      sender.setExtra(id, metadata);
      source.transform(ownerRobot).position = {900.f, 900.f};
      receiver.apply(destination, sender.encodeOwned(source), 1);
      test::require(!robotNet::reconcileRemotePose(destination, receiver, cache, id) &&
                        destination.transform(remote).position == glm::vec2{500.f, 400.f} &&
                        destination.transform(remote).scale == glm::vec2{4.f, 5.f},
                    "contradictory or missing/invalid metadata cannot poison accepted pose");
    }
    robotNet::enforceRemoteLife(destination, local, receiver, cache,
                               robotNet::robotId(2), fallback);
    test::require(destination.getShape(local) &&
                      destination.transform(local).position == glm::vec2{50.f, 60.f},
                  "remote visual enforcement never touches local player");
  }
}

void pendingAndAuthorization() {
  robotNet::RemoteLives cache;
  std::unordered_set<engine::ClientId> departed;
  auto event = life(3, 3, robotNet::LifeKind::Spawn);
  event.remote = true;
  event.from = engine::kServerId;
  test::require(robotNet::observeRemoteLife(cache, event, 2, departed),
                "unknown roster owner can be cached before its replica exists");
  test::require(!robotNet::observeRemoteLife(cache, event, 2, departed), "duplicate is a no-op");
  auto spoofed = event;
  spoofed.revision = 4;
  spoofed.from = 1;
  test::require(!robotNet::observeRemoteLife(cache, spoofed, 2, departed),
                "only server-origin remote results are accepted");
  spoofed.from = engine::kServerId;
  spoofed.remote = false;
  test::require(!robotNet::observeRemoteLife(cache, spoofed, 2, departed),
                "local/nonremote notice cannot masquerade as an authoritative result");
  auto echo = life(2, 3, robotNet::LifeKind::Death);
  echo.remote = true;
  echo.from = engine::kServerId;
  test::require(!robotNet::observeRemoteLife(cache, echo, 2, departed) &&
                    !cache.alive(echo.entity), "local echo is ignored");
  engine::Scene source, destination;
  const auto ownerRobot = source.createEntity();
  const auto local = destination.createEntity();
  engine::networking::sceneReplicator sender(3), receiver(2);
  sender.track(ownerRobot);
  receiver.track(local);
  const engine::Shape fallback{.size = {96.f, 128.f}};
  test::require(!robotNet::enforceRemoteLife(destination, local, receiver, cache,
                                            event.entity, fallback),
                "event before entity creation does not fabricate a local entity");
  sender.setExtra(event.entity, robotNet::encode({0, 0, 0, 1, true}));
  receiver.apply(destination, sender.encodeOwned(source), 3);
  robotNet::reconcileRemotePose(destination, receiver, cache, event.entity);
  robotNet::enforceRemoteLife(destination, local, receiver, cache, event.entity, fallback);
  const auto replica = *receiver.entityOf(event.entity);
  test::require(destination.getShape(replica) &&
                    destination.transform(replica).position == event.position,
                "pending event position survives until replica materializes");
  departed.insert(3);
  cache.forget(3);
  event.revision = 4;
  test::require(!robotNet::observeRemoteLife(cache, event, 2, departed) && !cache.alive(event.entity),
                "late event for departed owner cannot resurrect lifecycle state");
}

int main() {
  return test::run([] {
    visualAndPoseReconciliation();
    pendingAndAuthorization();
    robotNet::RemoteLives cache;
    const auto a = robotNet::robotId(1), b = robotNet::robotId(2);
    test::require(!cache.alive(a), "unknown lifecycle is not invented");
    const auto death = life(1, 2, robotNet::LifeKind::Death);
    test::require(cache.observe(death) && cache.alive(a) == false,
                  "death revision 2 initializes lifecycle cache");
    test::require(!cache.observeSnapshot(a, 1, true) && cache.alive(a) == false,
                  "stale alive snapshot cannot undo death");
    test::require(!cache.observe(death) && !cache.observeSnapshot(a, 2, true),
                  "duplicate event and contradictory same-revision snapshot are ignored");
    auto spawn = life(1, 3, robotNet::LifeKind::Spawn);
    test::require(cache.observe(spawn) && cache.alive(a) == true,
                  "new spawn wins over old death");
    test::require(!cache.observeSnapshot(a, 3, false), "same-revision dead snapshot rejected");
    test::require(cache.takeSpawnPosition(a) == spawn.position && !cache.takeSpawnPosition(a),
                  "spawn position is consumed exactly once, including after rejected snapshot");
    spawn.revision = 4;
    test::require(cache.observe(spawn) && cache.observeSnapshot(a, 4, true) &&
                      !cache.takeSpawnPosition(a),
                  "accepted same-revision snapshot supersedes a pending spawn pose");
    spawn.revision = 5;
    test::require(cache.observe(spawn) && cache.observeSnapshot(a, 6, true) &&
                      !cache.takeSpawnPosition(a),
                  "newer accepted snapshot also supersedes a pending pose");
    test::require(cache.observeSnapshot(b, 0, true) && cache.observeSnapshot(b, 0, true),
                  "legacy revision-zero movement initializes and advances unknown lifecycle");
    test::require(cache.observe(life(2, 1, robotNet::LifeKind::Death)) &&
                      !cache.observeSnapshot(b, 0, true),
                  "legacy metadata cannot override positive lifecycle revisions");
    engine::Transform pose;
    pose.position = {800.f, 500.f};
    pose.scale = {2.f, 3.f};
    cache.rememberPose(a, pose);
    test::require(cache.acceptedPose(a) && cache.acceptedPose(a)->position == pose.position &&
                      cache.acceptedPose(a)->scale == pose.scale,
                  "accepted pose stores a persistent full Transform value");
    pose.position = {900.f, 500.f};
    cache.rememberPose(a, pose);
    test::require(cache.acceptedPose(a)->position == pose.position,
                  "valid movement replaces accepted pose instead of pinning to spawn");
    auto bad = life(1, 7, static_cast<robotNet::LifeKind>(9));
    test::require(!cache.observe(bad), "direct invalid lifecycle kind is rejected");
    bad = life(1, 7, robotNet::LifeKind::Spawn);
    bad.position.x = std::numeric_limits<float>::infinity();
    test::require(!cache.observe(bad), "direct nonfinite lifecycle value is rejected");
    bad = life(1, 0, robotNet::LifeKind::Spawn);
    test::require(!cache.observe(bad), "wire revision zero is never accepted");
    cache.forget(1);
    test::require(!cache.alive(a) && !cache.takeSpawnPosition(a) && !cache.acceptedPose(a) &&
                      cache.alive(b) == false,
                  "forget clears only one owner's lifecycle, pending position, and accepted pose");
  });
}
