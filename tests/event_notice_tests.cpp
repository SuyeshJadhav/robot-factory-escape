#include "test_support.hpp"
#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

int main() {
  return test::run([] {
    robotNet::LifeEvent event;
    event.entity = robotNet::robotId(2);
    event.revision = 2;
    event.kind = robotNet::LifeKind::Death;
    event.remote = true;
    event.from = engine::kServerId;
    const std::unordered_set<engine::ClientId> departed;
    robotNet::LifeNotices notices;
    // A newer snapshot must neither synthesize nor suppress an event-only notice.
    robotNet::RemoteLives snapshots;
    snapshots.observeSnapshot(event.entity, 2, false);
    const auto message = notices.receive(event, 1, departed);
    test::require(message && *message == "STATUS: PLAYER 2 DESTROYED",
                  "a real received event has its own outcome despite snapshot-first reconciliation");
    test::require(!notices.receive(event, 1, departed), "duplicate event does not repeat notice");
    event.kind = robotNet::LifeKind::Spawn;
    event.revision = 3;
    test::require(notices.receive(event, 1, departed) == "STATUS: PLAYER 2 SPAWNED",
                  "a newer spawn event produces its own notice");
    event.revision = 2;
    test::require(!notices.receive(event, 1, departed), "stale event cannot roll notice back");
    robotNet::LifeNotices late;
    event.kind = robotNet::LifeKind::Death;
    event.repair = true;
    const auto decoded = robotNet::LifeEvent::decode(event.encode());
    test::require(decoded && decoded->repair, "repair provenance survives wire encoding");
    test::require(!late.receive(event, 1, departed), "late join does not announce historical death");
    event.repair = false;
    test::require(!late.receive(event, 1, departed), "repair also suppresses duplicate live delivery");
    event.kind = robotNet::LifeKind::Spawn;
    event.revision = 3;
    test::require(late.receive(event, 1, departed).has_value(), "next live event after repair is announced");
    event.revision = 4;
    event.from = 2;
    test::require(!late.receive(event, 1, departed), "nonserver notice is denied");
    event.from = engine::kServerId;
    test::require(!late.receive(event, 2, departed), "local echo is not a remote notice");
    test::require(!late.receive(event, 1, {2}), "departed owner cannot announce");
    event.remote = false;
    test::require(!late.receive(event, 1, departed), "local event cannot announce remote lifecycle");
    event.remote = true;
    event.from = 2;
    event.repair = true;
    robotNet::LifeRelay relay;
    test::require(!relay.accept(event, true), "clients cannot forge server repair provenance");
  });
}
