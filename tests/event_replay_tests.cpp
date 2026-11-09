#include "test_support.hpp"
#include "event_replay_support.hpp"
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

void playbackIsolationAndVisibility() {
  engine::Scene live;
  const auto world = factory::createWorld(live);
  const auto robot = factory::createPlayer(live, world);
  engine::events::eventManager events;
  factory::ReplaySession replay(events, 0);
  events.raise(engine::events::replayPlayStart{}, 0);
  events.run(live, 0);
  test::require(!replay.playing() && !replay.update(live, world, robot, false, 0),
                "empty recording does not enter playback");
  events.raise(engine::events::replayRecordStart{}, 0);
  events.run(live, 0);
  live.transform(robot).position = {100.f, 300.f};
  live.getShape(robot)->texture = 7;
  const auto artwork = *live.getShape(robot);
  live.addSpriteAnimation(robot, {0, {{9, 0.2f}, {11, 0.2f}}, true, 1});
  replay.update(live, world, robot, false, 1);
  live.removeShape(robot);
  live.removeSpriteAnimation(robot);
  replay.update(live, world, robot, false, 2);
  live.addShape(robot, artwork);
  live.addSpriteAnimation(robot, {0, {{9, 0.2f}}, true});
  live.transform(robot).position = {300.f, 300.f};
  replay.update(live, world, robot, false, 3);
  events.raise(engine::events::replayRecordStop{}, 3);
  events.run(live, 3);
  live.transform(robot).position = {700.f, 300.f};
  live.getRigidBody(robot)->velocity = {99.f, 123.f};
  const auto count = live.transforms().size();
  events.raise(engine::events::replayPlayStart{}, 4);
  events.run(live, 4);
  const auto* frame = replay.update(live, world, robot, false, 4);
  test::require(frame && frame != &live && !replay.recording() &&
                    recordedRobot(frame) == glm::vec2{100.f, 300.f} &&
                    frame->all<engine::SpriteAnimation>().begin()->second.frames[0].index == 11,
                "first recorded frame and held animation are reconstructed before advancing");
  frame = replay.update(live, world, robot, false, 5);
  test::require(frame && !recordedRobot(frame) && frame->all<factory::ReplayView>().size() == 1,
                "death sample hides robot while retaining replay presentation marker");
  frame = replay.update(live, world, robot, false, 6);
  test::require(frame && recordedRobot(frame) == glm::vec2{300.f, 300.f} &&
                    frame->all<engine::SpriteAnimation>().begin()->second.frames[0].index == 9,
                "spawn sample restores recorded robot artwork and pose");
  test::require(!replay.update(live, world, robot, false, 7) && !replay.playing(),
                "natural completion returns live presentation");
  events.raise(engine::events::replayPlayStart{}, 8);
  events.run(live, 8);
  replay.update(live, world, robot, false, 8);
  events.raise(engine::events::replayPlayStop{}, 9);
  events.run(live, 9);
  test::require(!replay.playing() && !replay.update(live, world, robot, false, 9),
                "manual stop returns live presentation");
  test::require(live.transform(robot).position == glm::vec2{700.f, 300.f} &&
                    live.getRigidBody(robot)->velocity == glm::vec2{99.f, 123.f} &&
                    live.getShape(robot)->texture == 7 && live.transforms().size() == count &&
                    live.all<factory::ReplayView>().empty(),
                "playback never mutates live transforms, motion, artwork, or entity collection");
}

void manualReplayControls() {
  using namespace engine::SC;
  ReplayRig held;
  held.key(SDL_SCANCODE_F5, true);
  held.key(SDL_SCANCODE_F5, true);
  test::require(held.replay->recording(), "held F5 does not retrigger recording");
  held.key(SDL_SCANCODE_F5, false);
  held.tap(SDL_SCANCODE_F5);
  test::require(!held.replay->recording(), "fresh F5 press toggles record stop");
  ReplayRig empty;
  empty.tap(SDL_SCANCODE_F6);
  test::require(!empty.replay->playing() && empty.status.find("NO RECORDING") != std::string::npos,
                "empty F6 is harmless and explains why playback did not start");
  ReplayRig controls;
  controls.recordTwoFrames();
  controls.key(SDL_SCANCODE_F6, true);
  controls.key(SDL_SCANCODE_F6, true);
  test::require(controls.replay->playing(), "held F6 does not immediately stop playback");
  controls.key(SDL_SCANCODE_F6, false);
  controls.tap(SDL_SCANCODE_F5);
  test::require(controls.replay->playing() && !controls.replay->recording() &&
                    controls.status.find("STOP REPLAY") != std::string::npos,
                "F5 during playback is rejected without replacing the recording");
  controls.gameplay.progress.won = true;
  controls.tap(SDL_SCANCODE_R);
  test::require(!controls.replay->playing() && controls.gameplay.rules.alive() &&
                    !controls.gameplay.progress.won,
                "R stops replay and routes restart through the input lifecycle path");
  controls.gameplay.events.raise(engine::events::deathEvent{{}, controls.gameplay.robot, 0}, 2);
  controls.gameplay.events.run(controls.gameplay.scene, 2);
  controls.tap(SDL_SCANCODE_F6);
  test::require(!controls.replay->playing() &&
                    controls.status.find("RESPAWN") != std::string::npos,
                "F6 while dead is rejected with a readable status");
  controls.tap(SDL_SCANCODE_R);
  controls.paused = true;
  controls.tap(SDL_SCANCODE_F6);
  test::require(!controls.replay->playing() &&
                    controls.status.find("UNPAUSE") != std::string::npos,
                "F6 while Timeline paused is rejected");
  controls.paused = false;
  controls.tap(SDL_SCANCODE_F6);
  controls.tap(SDL_SCANCODE_ESCAPE);
  test::require(!controls.replay->playing(), "Escape stops playback");
}

int main() {
  return test::run([] {
    manualReplayControls();
    playbackIsolationAndVisibility();
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
