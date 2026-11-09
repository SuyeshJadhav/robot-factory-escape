#include "test_support.hpp"
#include <robot_factory_escape/replay/game_replay.hpp>
#include <engine/events/commonEvents.hpp>
#include <engine/events/eventManager.hpp>

std::optional<glm::vec2> recordedRobot(const engine::Scene* frame) {
  if (frame)
    for (const auto& [id, shape] : frame->shapes())
      if (shape.color.r == 72 && shape.color.g == 205 && shape.color.b == 230)
        return frame->transform(id).position;
  return std::nullopt;
}

int main() {
  return test::run([] {
    engine::Scene live;
    const auto world = factory::createWorld(live);
    const auto robot = factory::createPlayer(live, world);
    engine::events::eventManager events;
    factory::ReplaySession replay(events, 0);
    events.raise(engine::events::replayRecordStart{}, 0);
    events.run(live, 0);
    test::require(replay.recording(), "record-start event enables recording");
    live.transform(robot).position = {100.f, 300.f};
    replay.update(live, world, robot, false, 1);
    live.transform(robot).position = {200.f, 300.f};
    replay.update(live, world, robot, false, 2);
    events.raise(engine::events::replayRecordStop{}, 2);
    events.run(live, 2);
    test::require(!replay.recording(), "record-stop event preserves but stops recording");
    live.transform(robot).position = {500.f, 300.f};
    replay.update(live, world, robot, false, 3);
    events.raise(engine::events::replayPlayStart{}, 3);
    events.run(live, 3);
    const auto* first = replay.update(live, world, robot, false, 4);
    test::require(recordedRobot(first) == glm::vec2{100.f, 300.f},
                  "recording readback starts with the first captured state");
    test::require(first->all<factory::ReplayView>().size() == 1 && first->texts().size() == 1,
                  "reconstructed view receives one presentation marker and local replay banner");
    test::require(recordedRobot(replay.update(live, world, robot, false, 5)) == glm::vec2{200.f, 300.f},
                  "recording retains the second captured state");
    test::require(!replay.update(live, world, robot, false, 6),
                  "live changes after record-stop are not captured");
    events.raise(engine::events::replayRecordStart{}, 7);
    events.run(live, 7);
    live.transform(robot).position = {300.f, 300.f};
    replay.update(live, world, robot, false, 8);
    events.raise(engine::events::replayRecordStop{}, 8);
    events.run(live, 8);
    events.raise(engine::events::replayPlayStart{}, 9);
    events.run(live, 9);
    test::require(recordedRobot(replay.update(live, world, robot, false, 10)) == glm::vec2{300.f, 300.f} &&
                      !replay.update(live, world, robot, false, 11),
                  "starting a new recording clears old samples");
  });
}
