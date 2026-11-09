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

int main() {
  return test::run([] {
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
