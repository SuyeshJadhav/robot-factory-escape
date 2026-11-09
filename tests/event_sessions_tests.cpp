#include "test_support.hpp"
#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>
#include <engine/events/eventManager.hpp>
#include <engine/networking/sessionClient.hpp>
#include <engine/networking/sessionServer.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

struct Endpoint {
  engine::networking::sessionClient session;
  engine::events::eventManager events;
  engine::Scene scene;
  std::vector<robotNet::LifeEvent> received;

  explicit Endpoint(const std::string& address) : session(address) {
    events.subscribe<robotNet::LifeEvent>([this](const auto& event, engine::Scene&) {
      test::require(event.remote && event.from == engine::kServerId,
                    "accepted result carries server transport identity");
      received.push_back(event);
    }, true);
    test::require(session.join(), "socket client joins");
    session.start();
  }
  void poll(std::int64_t tick) {
    session.requestSnapshot(tick);
    events.receive(session.drainMessages(), tick);
    events.run(scene, tick);
  }
};

template <class Predicate> void until(Predicate predicate, const char* message) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate())
      return;
    std::this_thread::sleep_for(10ms);
  }
  throw std::runtime_error(message);
}

int main() {
  return test::run([] {
    engine::networking::sessionServer server("tcp://127.0.0.1:0");
    engine::Scene scene;
    robotNet::LifeServer life(server);
    server.publishScene(scene, 0);
    server.start();
    const auto address = "tcp://127.0.0.1:" + std::to_string(server.port());
    Endpoint a(address), b(address);
    std::vector<Endpoint*> active{&a, &b};
    std::int64_t tick = 0;
    const auto pump = [&] {
      ++tick;
      life.pump(scene, tick, server.drainRosterEvents());
      for (auto* endpoint : active)
        endpoint->poll(tick);
    };
    robotNet::LifeEvent death;
    death.entity = robotNet::robotId(a.session.id());
    death.revision = 2;
    death.kind = robotNet::LifeKind::Death;
    death.position = {2100.f, 1000.f};
    a.session.send(robotNet::LifeEvent::kType, death.encode(), 1);
    until([&] { pump(); return !b.received.empty(); }, "death reaches another socket endpoint");
    test::require(b.received.size() == 1 && b.received[0].entity == death.entity &&
                      b.received[0].kind == robotNet::LifeKind::Death,
                  "production relay delivers a valid owner death");
    auto forged = death;
    forged.entity = robotNet::robotId(b.session.id());
    forged.revision = 99;
    a.session.send(robotNet::LifeEvent::kType, forged.encode(), 2);
    auto spawn = death;
    spawn.revision = 3;
    spawn.kind = robotNet::LifeKind::Spawn;
    spawn.position = {120.f, 300.f};
    a.session.send(robotNet::LifeEvent::kType, spawn.encode(), 3);
    until([&] { pump(); return b.received.size() >= 2; }, "valid marker follows forged traffic");
    test::require(b.received.size() == 2 && b.received[1].entity == death.entity &&
                      b.received[1].revision == 3,
                  "forged owner request is rejected without dropping following valid traffic");
    Endpoint late(address);
    active.push_back(&late);
    until([&] { pump(); return !late.received.empty(); }, "late join receives cached lifecycle");
    test::require(late.received.size() == 1 && late.received[0].entity == spawn.entity &&
                      late.received[0].revision == 3,
                  "late join repairs to newest lifecycle, not historical death");
    const auto departed = a.session.id();
    a.session.leave();
    std::erase(active, &a);
    until([&] {
      pump();
      const auto roster = server.roster();
      return std::none_of(roster.begin(), roster.end(),
                          [&](const auto& client) { return client.id == departed; });
    }, "departed owner is removed");
    // Pump once after observing departure so its roster event is consumed.
    pump();
    auto marker = spawn;
    marker.entity = robotNet::robotId(b.session.id());
    marker.revision = 1;
    b.session.send(robotNet::LifeEvent::kType, marker.encode(), tick);
    until([&] { pump(); return b.received.back().entity == marker.entity; },
          "remaining owner publishes departure-check marker");
    Endpoint afterDeparture(address);
    active.push_back(&afterDeparture);
    until([&] { pump(); return !afterDeparture.received.empty(); },
          "post-departure join receives remaining owner marker");
    test::require(afterDeparture.received.size() == 1 &&
                      afterDeparture.received[0].entity == marker.entity,
                  "departed owner's cached lifecycle is not replayed to new clients");
    for (auto* endpoint : active)
      endpoint->session.leave();
    server.stop();
  });
}
