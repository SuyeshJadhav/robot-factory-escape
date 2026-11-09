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

void naturalCompletionReportsOnce() {
  ReplayRig rig;
  rig.recordTwoFrames();
  test::require(!rig.replay->takeNaturalEnd(), "no end is reported before playback");
  rig.tap(engine::SC::SDL_SCANCODE_F6);
  for (int i = 0; i < 6 && rig.replay->playing(); ++i)
    rig.playbackFrame();
  test::require(!rig.replay->playing() && rig.replay->takeNaturalEnd() &&
                    !rig.replay->takeNaturalEnd(),
                "running out of samples is reported exactly once for the live HUD");
  rig.tap(engine::SC::SDL_SCANCODE_F6);
  rig.playbackFrame();
  rig.tap(engine::SC::SDL_SCANCODE_ESCAPE);
  rig.playbackFrame();
  test::require(!rig.replay->playing() && !rig.replay->takeNaturalEnd(),
                "manual stop is not reported as natural end (it already writes LIVE GAMEPLAY)");
}

void replayControlFeedback() {
  ReplayRig rig;
  rig.recordTwoFrames();
  rig.tap(engine::SC::SDL_SCANCODE_F6);
  rig.playbackFrame();
  rig.tap(engine::SC::SDL_SCANCODE_F5);
  const auto* frame = rig.playbackFrame();
  test::require(frame && !frame->texts().empty() &&
                    frame->texts().begin()->second.val.find("STOP REPLAY") != std::string::npos,
                "rejected recording control is visible in replay banner, not hidden live HUD");
  for (const auto key : {engine::SC::SDL_SCANCODE_T, engine::SC::SDL_SCANCODE_1,
                         engine::SC::SDL_SCANCODE_2, engine::SC::SDL_SCANCODE_3}) {
    rig.tap(key);
    test::require(rig.replay->playing() && rig.status.find("LOCKED") != std::string::npos,
                  "time-control events are handled and rejected during playback");
  }
}

void blockedLifecycleWork() {
  ReplayRig dead;
  dead.recordTwoFrames();
  dead.gameplay.events.raise(engine::events::deathEvent{{}, dead.gameplay.robot, 0}, 3);
  dead.gameplay.events.run(dead.gameplay.scene, 3);
  dead.gameplay.rules.takeLifeEvents();
  // Direct control events also need isolation, even though F6 rejects a dead player.
  dead.gameplay.events.raise(engine::events::replayPlayStart{}, 4);
  dead.gameplay.events.run(dead.gameplay.scene, 4);
  dead.replay->syncGameplay(dead.gameplay.rules);
  const auto position = dead.gameplay.scene.transform(dead.gameplay.robot).position;
  dead.gameplay.events.raise(engine::events::spawnEvent{{}, dead.gameplay.robot, 1, {}}, 64);
  for (std::int64_t tick = 64; tick <= 66; ++tick)
    dead.gameplay.events.run(dead.gameplay.scene, tick);
  test::require(!dead.gameplay.rules.alive() && dead.gameplay.rules.revision() == 2 &&
                    dead.gameplay.scene.transform(dead.gameplay.robot).position == position &&
                    dead.gameplay.rules.takeLifeEvents().empty(),
                "network/event pumping during playback cannot execute local delayed spawn work");
  dead.gameplay.events.raise(engine::events::replayPlayStop{}, 66);
  dead.gameplay.events.run(dead.gameplay.scene, 66);
  dead.replay->syncGameplay(dead.gameplay.rules);
  dead.gameplay.events.run(dead.gameplay.scene, 67);
  test::require(dead.gameplay.rules.alive() && dead.gameplay.rules.revision() == 3 &&
                    dead.gameplay.rules.takeLifeEvents().size() == 1,
                "deferred duplicate spawn work resumes exactly once after playback");
  ReplayRig alive;
  alive.recordTwoFrames();
  alive.tap(engine::SC::SDL_SCANCODE_F6);
  alive.gameplay.scene.getRigidBody(alive.gameplay.robot)->velocity = {19.f, 23.f};
  alive.gameplay.events.raise(engine::events::deathEvent{{}, alive.gameplay.robot, 0}, 3);
  alive.gameplay.events.run(alive.gameplay.scene, 3);
  test::require(alive.gameplay.rules.alive() && alive.gameplay.rules.revision() == 1 &&
                    alive.gameplay.scene.getRigidBody(alive.gameplay.robot)->velocity ==
                        glm::vec2{19.f, 23.f} && alive.gameplay.rules.takeLifeEvents().empty(),
                "queued local death cannot mutate a frozen live robot during replay");
}

void replayJumpBoundaries() {
  using namespace engine::SC;
  ReplayRig manual;
  manual.recordTwoFrames();
  manual.gameplay.stand();
  manual.tap(SDL_SCANCODE_F6);
  manual.tap(SDL_SCANCODE_SPACE);
  manual.tap(SDL_SCANCODE_ESCAPE);
  manual.gameplay.stand();
  manual.gameplay.step();
  test::require(manual.gameplay.scene.getRigidBody(manual.gameplay.robot)->velocity.y == 0.f,
                "space_tap_during_replay_then_escape_does_not_jump");
  ReplayRig natural;
  natural.recordTwoFrames();
  natural.gameplay.stand();
  natural.tap(SDL_SCANCODE_F6);
  natural.tap(SDL_SCANCODE_SPACE);
  natural.playbackFrame();
  natural.playbackFrame();
  natural.playbackFrame();
  natural.gameplay.stand();
  natural.gameplay.step();
  test::require(!natural.replay->playing() &&
                    natural.gameplay.scene.getRigidBody(natural.gameplay.robot)->velocity.y == 0.f,
                "natural completion clears replay-time one-shot jump requests");
  ReplayRig held;
  held.recordTwoFrames();
  held.gameplay.stand();
  held.key(SDL_SCANCODE_SPACE, true);
  held.tap(SDL_SCANCODE_F6);
  held.tap(SDL_SCANCODE_ESCAPE);
  held.gameplay.stand();
  held.gameplay.step();
  test::require(held.gameplay.scene.getRigidBody(held.gameplay.robot)->velocity.y == 0.f,
                "pending jump on entry and Space held across exit cannot auto-jump");
  held.key(SDL_SCANCODE_SPACE, false);
  held.key(SDL_SCANCODE_SPACE, true);
  held.gameplay.step();
  test::require(held.gameplay.scene.getRigidBody(held.gameplay.robot)->velocity.y < 0.f,
                "fresh release and press after playback jumps normally");
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
    naturalCompletionReportsOnce();
    replayControlFeedback();
    blockedLifecycleWork();
    replayJumpBoundaries();
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
