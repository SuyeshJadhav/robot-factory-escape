#include "test_support.hpp"
#include <robot_factory_escape/gameplay/game_events.hpp>
#include <robot_factory_escape/replay/game_replay.hpp>
#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>
#include <engine/events/eventManager.hpp>
#include <engine/networking/sessionClient.hpp>
#include <engine/networking/sessionServer.hpp>
#include <engine/networking/peerSession.hpp>

#include <algorithm>
#include <chrono>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;

template <class Session> struct Endpoint {
  Session session;
  engine::events::eventManager events;
  engine::Scene scene;
  std::vector<robotNet::LifeEvent> received;
  robotNet::RemoteLives lives;
  robotNet::LifeNotices noticeState;
  std::vector<std::string> notices;
  std::unordered_set<engine::ClientId> departed;
  std::optional<std::int64_t> frozenTick;

  static Session makeSession(const std::string& address) {
    if constexpr (std::is_same_v<Session, engine::networking::peerSession>)
      return Session(address, "127.0.0.1");
    else
      return Session(address);
  }
  explicit Endpoint(const std::string& address) : session(makeSession(address)) {
    events.subscribe<robotNet::LifeEvent>([this](const auto& event, engine::Scene&) {
      test::require(event.remote && event.from == engine::kServerId,
                    "accepted result carries server transport identity");
      received.push_back(event);
      robotNet::observeRemoteLife(lives, event, session.id(), departed);
      if (const auto notice = noticeState.receive(event, session.id(), departed))
        notices.push_back(*notice);
    }, true);
    test::require(session.join(), "socket client joins");
    session.start();
  }
  void poll(std::int64_t tick) {
    if constexpr (std::is_same_v<Session, engine::networking::peerSession>)
      session.publishScene(scene, tick);
    else
      session.requestSnapshot(tick);
    tick = frozenTick.value_or(tick);
    events.receive(session.drainMessages(), tick);
    events.run(scene, tick);
    const auto local = session.replicator().entityOf(robotNet::robotId(session.id())).value_or(0);
    for (const auto& [entity, transform] : scene.transforms())
      if (const auto netId = session.replicator().netIdOf(entity))
        robotNet::enforceRemoteLife(scene, local, session.replicator(), lives, *netId,
                                    engine::Shape{.size = {96.f, 128.f}});
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

template <class Session> void eventOnlyLocalLifecycle() {
  engine::networking::sessionServer server("tcp://127.0.0.1:0");
  engine::Scene serverScene;
  robotNet::LifeServer service(server);
  server.publishScene(serverScene, 0);
  server.start();
  const auto address = "tcp://127.0.0.1:" + std::to_string(server.port());
  Endpoint<Session> b(address), a(address); // Exercise player 2's assigned spawn.
  const auto world = factory::createWorld(a.scene);
  const auto local = factory::createPlayer(a.scene, world);
  factory::assignSpawn(a.scene, world, local, a.session.id());
  a.session.replicator().track(local);
  const auto ownB = b.scene.createEntity();
  b.session.replicator().track(ownB);
  bool grounded = false;
  GameProgress progress;
  factory::GameEvents rules(a.events, world, local, a.session.id(), false, grounded, progress);
  rules.start(a.scene, 0);
  a.frozenTick = 0;
  const auto id = robotNet::robotId(a.session.id());
  a.session.replicator().setExtra(id, robotNet::encode({0, 0, 0, 1, true}));
  test::require(b.session.replicator().apply(b.scene,
      a.session.replicator().encodeOwned(a.scene), a.session.id()), "initial replica setup");
  const auto remote = *b.session.replicator().entityOf(id);
  robotNet::reconcileRemotePose(b.scene, b.session.replicator(), b.lives, id);
  std::int64_t tick = 0;
  std::vector<Endpoint<Session>*> active{&a, &b};
  const auto pump = [&] {
    ++tick;
    service.pump(serverScene, tick, server.drainRosterEvents());
    for (auto* endpoint : active)
      endpoint->poll(tick);
  };
  test::require(robotNet::publishLifeEvents(a.session, rules.takeLifeEvents(), 0) == 1,
                "initial spawn produces exactly one send");
  until([&] { pump(); return b.received.size() == 1; }, "initial lifecycle reaches replica");
  a.scene.transform(local).position = a.scene.transform(world.drone).position;
  engine::PhysicsSystem physics(1800.f);
  const auto deathTick = ++tick;
  a.frozenTick = deathTick;
  test::require(rules.step(a.scene, physics, deathTick, 1.f / 60.f) == factory::TickResult::DroneHit,
                "real local collision generates death before network publication");
  test::require(robotNet::publishLifeEvents(a.session, rules.takeLifeEvents(), deathTick) == 1 &&
                    robotNet::publishLifeEvents(a.session, rules.takeLifeEvents(), deathTick) == 0,
                "one local death sends once and second drain sends nothing");
  until([&] { pump(); return b.lives.alive(id) == false; }, "event-only death reaches replica");
  test::require(!b.scene.getShape(remote) && !rules.alive() && rules.revision() == 2 &&
                    !a.lives.alive(id), "remote hides while local echo cannot change local lifecycle");
  a.session.replicator().setExtra(id, robotNet::encode({0, 0, 0, 2, false}));
  Endpoint<Session> lateDead(address);
  const auto lateLocal = lateDead.scene.createEntity();
  lateDead.session.replicator().track(lateLocal);
  test::require(lateDead.session.replicator().apply(lateDead.scene,
      a.session.replicator().encodeOwned(a.scene), a.session.id()),
      "late join receives current dead repair metadata");
  robotNet::reconcileRemotePose(lateDead.scene, lateDead.session.replicator(), lateDead.lives, id);
  active.push_back(&lateDead);
  until([&] { pump(); return !lateDead.received.empty(); },
        "late join while dead receives cached death result");
  const auto lateReplica = *lateDead.session.replicator().entityOf(id);
  test::require(lateDead.received[0].kind == robotNet::LifeKind::Death &&
                    lateDead.received[0].repair && lateDead.notices.empty() &&
                    lateDead.lives.alive(id) == false && !lateDead.scene.getShape(lateReplica),
                "late join stays dead instead of inventing a live remote robot");
  a.frozenTick = deathTick + 60;
  a.events.run(a.scene, *a.frozenTick);
  test::require(rules.alive() && rules.revision() == 3, "scheduled local spawn occurs");
  test::require(robotNet::publishLifeEvents(a.session, rules.takeLifeEvents(), *a.frozenTick) == 1 &&
                    robotNet::publishLifeEvents(a.session, rules.takeLifeEvents(), *a.frozenTick) == 0,
                "one local respawn sends once and drains its outbox");
  until([&] { pump(); return b.lives.alive(id) == true; }, "event-only spawn reaches replica");
  test::require(b.scene.getShape(remote) &&
                    b.scene.transform(remote).position == a.scene.transform(local).position &&
                    a.scene.transform(local).position == glm::vec2{360.f, 300.f} &&
                    b.received.size() == 3,
                "spawn event restores remote artwork and pose without newer snapshot application");
  const auto metadata = robotNet::decode(*b.session.replicator().extraOf(id));
  test::require(metadata && metadata->lifeRevision == 1,
                "replica metadata stayed at initial state: lifecycle proof is event-only");
  until([&] { pump(); return lateDead.lives.alive(id) == true; },
        "late-joining endpoint also receives respawn");
  test::require(lateDead.scene.getShape(lateReplica) &&
                    lateDead.notices.size() == 1 &&
                    lateDead.scene.transform(lateReplica).position == glm::vec2{360.f, 300.f},
                "late join restores owner-assigned spawn through the event channel");
  a.session.leave();
  b.session.leave();
  lateDead.session.leave();
  server.stop();
}

template <class Session> void relaySocketScenario() {
    engine::networking::sessionServer server("tcp://127.0.0.1:0");
    engine::Scene scene;
    robotNet::LifeServer life(server);
    server.publishScene(scene, 0);
    server.start();
    const auto address = "tcp://127.0.0.1:" + std::to_string(server.port());
    Endpoint<Session> a(address), b(address);
    std::vector<Endpoint<Session>*> active{&a, &b};
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
    Endpoint<Session> late(address);
    active.push_back(&late);
    until([&] { pump(); return !late.received.empty(); }, "late join receives cached lifecycle");
    test::require(late.received.size() == 1 && late.received[0].entity == spawn.entity &&
                      late.received[0].revision == 3 && late.received[0].repair && late.notices.empty(),
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
    Endpoint<Session> afterDeparture(address);
    active.push_back(&afterDeparture);
    until([&] { pump(); return !afterDeparture.received.empty(); },
          "post-departure join receives remaining owner marker");
    test::require(afterDeparture.received.size() == 1 &&
                      afterDeparture.received[0].entity == marker.entity,
                  "departed owner's cached lifecycle is not replayed to new clients");
    for (auto* endpoint : active)
      endpoint->session.leave();
    server.stop();
}

void replayDisplayNeverBecomesPublishedState() {
  engine::networking::sessionServer server("tcp://127.0.0.1:0");
  engine::Scene serverScene;
  server.publishScene(serverScene, 0);
  server.start();
  const auto address = "tcp://127.0.0.1:" + std::to_string(server.port());
  Endpoint<engine::networking::sessionClient> a(address), b(address);
  engine::Scene live;
  const auto world = factory::createWorld(live);
  const auto robot = factory::createPlayer(live, world);
  const auto id = a.session.replicator().track(robot);
  a.session.replicator().setExtra(id, robotNet::encode({0, 0, 0, 1, true}));
  factory::ReplaySession replay(a.events, 0);
  std::int64_t clock = 0;
  engine::Timeline source(&clock, 60), time(source, 60);
  engine::SimulationThread simulation(std::move(live), time, [&](const engine::TickContext& ctx) {
    if (ctx.tick <= 2)
      ctx.scene.transform(robot).position = {100.f * static_cast<float>(ctx.tick), 300.f};
    a.session.submitScene(ctx.scene, ctx.tick);
    simulation.showScene(replay.update(ctx.scene, world, robot, false, ctx.tick));
  });
  simulation.post([&](engine::Scene& scene, engine::Timeline&) {
    a.events.raise(engine::events::replayRecordStart{}, 0);
    a.events.run(scene, 0);
  });
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  simulation.post([&](engine::Scene& scene, engine::Timeline&) {
    a.events.raise(engine::events::replayRecordStop{}, 2);
    a.events.run(scene, 2);
    scene.transform(robot).position = {700.f, 300.f};
    scene.getRigidBody(robot)->velocity = {41.f, 42.f};
    a.events.raise(engine::events::replayPlayStart{}, 2);
    a.events.run(scene, 2);
  });
  simulation.advanceFrame();
  ++clock;
  simulation.advanceFrame();
  const auto frame = simulation.takeRenderFrame();
  test::require(frame && frame->scene.all<factory::ReplayView>().size() == 1,
                "SimulationThread copies isolated replay marker into displayed frame");
  bool recordedPoseDisplayed = false;
  for (const auto& [entity, shape] : frame->scene.shapes())
    if (shape.color.r == 72 && frame->scene.transform(entity).position.x == 100.f)
      recordedPoseDisplayed = true;
  test::require(recordedPoseDisplayed, "display shows first recorded pose, not live pose");
  std::int64_t serverTick = 0;
  until([&] {
    server.applyClientStates(serverScene);
    server.publishScene(serverScene, ++serverTick);
    b.session.requestSnapshot(serverTick);
    b.session.applySnapshot(b.scene);
    const auto remote = b.session.replicator().entityOf(id);
    return remote && b.scene.transform(*remote).position == glm::vec2{700.f, 300.f};
  }, "networking advances while replay display is held; only live pose is published");
  simulation.post([&](engine::Scene& scene, engine::Timeline&) {
    test::require(replay.playing() && scene.transform(robot).position == glm::vec2{700.f, 300.f} &&
                      scene.getRigidBody(robot)->velocity == glm::vec2{41.f, 42.f},
                  "display and network pumping do not mutate owned live motion");
  });
  simulation.advanceFrame();
  a.session.leave();
  b.session.leave();
  server.stop();
}

int main() {
  return test::run([] {
    replayDisplayNeverBecomesPublishedState();
    eventOnlyLocalLifecycle<engine::networking::sessionClient>();
    eventOnlyLocalLifecycle<engine::networking::peerSession>();
    relaySocketScenario<engine::networking::sessionClient>();
    relaySocketScenario<engine::networking::peerSession>();
  });
}
