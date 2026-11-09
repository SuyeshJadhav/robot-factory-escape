#include "test_support.hpp"
#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <limits>

int main() {
  return test::run([] {
    robotNet::LifeRelay relay;
    robotNet::LifeEvent event;
    event.entity = robotNet::robotId(1);
    event.revision = 1;
    event.kind = robotNet::LifeKind::Spawn;
    event.position = {120.f, 300.f};
    event.remote = true;
    event.from = 1;
    test::require(relay.accept(event, true), "connected owner can publish its own lifecycle");
    auto bad = event;
    bad.revision = 2;
    bad.from = 2;
    test::require(!relay.accept(bad, true), "sender cannot claim another owner's robot");
    bad = event;
    bad.revision = 2;
    bad.remote = false;
    test::require(!relay.accept(bad, true), "relay refuses local/nonremote requests");
    bad = event;
    bad.revision = 2;
    bad.from = engine::kServerId;
    test::require(!relay.accept(bad, true), "server cannot impersonate a player request");
    bad.from = engine::kMaxClientIdPerRun + 1;
    test::require(!relay.accept(bad, true), "out-of-range sender is rejected without NetId assertion");
    bad = event;
    bad.revision = 2;
    test::require(!relay.accept(bad, false), "disconnected sender is rejected");
    bad.kind = static_cast<robotNet::LifeKind>(9);
    test::require(!relay.accept(bad, true), "invalid kind is rejected");
    bad = event;
    bad.revision = 2;
    bad.position.y = std::numeric_limits<float>::quiet_NaN();
    test::require(!relay.accept(bad, true), "nonfinite position is rejected");
    bad = event;
    bad.revision = 0;
    test::require(!relay.accept(bad, true) && !relay.accept(event, true),
                  "zero revision and duplicate event are rejected");
    const auto unchanged = relay.latest();
    test::require(unchanged.size() == 1 && unchanged[0].revision == 1 &&
                      unchanged[0].position == event.position,
                  "rejected requests leave relay state unchanged");
    event.revision = 3;
    event.kind = robotNet::LifeKind::Death;
    test::require(relay.accept(event, true), "higher lifecycle revision is accepted");
    bad = event;
    bad.revision = 2;
    test::require(!relay.accept(bad, true), "stale lifecycle revision is rejected");
    event.entity = robotNet::robotId(2);
    event.from = 2;
    event.revision = 1;
    test::require(relay.accept(event, true), "another owner has independent lifecycle state");
    const auto latest = relay.latest();
    test::require(latest.size() == 2 && latest[0].entity == robotNet::robotId(1) &&
                      latest[1].entity == robotNet::robotId(2),
                  "latest lifecycle replay is sorted deterministically by NetId");
    relay.forget(1);
    test::require(relay.latest().size() == 1 && relay.latest()[0].entity == robotNet::robotId(2),
                  "owner departure removes only its cached lifecycle");
  });
}
