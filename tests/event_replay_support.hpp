#pragma once

#include "event_test_support.hpp"
#include <robot_factory_escape/replay/game_replay.hpp>
#include <memory>
#include <string>

struct ReplayRig {
  EventRig gameplay;
  std::unique_ptr<factory::ReplaySession> replay;
  bool paused = false;
  std::string status;

  ReplayRig() : gameplay(1, false,
      [this](const engine::events::inputEvent& event, engine::Scene&) {
        if (const auto message = replay->input(event, gameplay.rules.alive(), paused))
          status = *message;
      }) {
    replay = std::make_unique<factory::ReplaySession>(gameplay.events, 0);
  }

  void key(engine::SC::SDL_Scancode code, bool pressed) {
    gameplay.key(code, pressed);
    replay->syncGameplay(gameplay.rules);
  }
  void tap(engine::SC::SDL_Scancode code) { key(code, true); key(code, false); }

  const engine::Scene* playbackFrame() {
    const auto* frame = replay->update(gameplay.scene, gameplay.world, gameplay.robot,
                                        gameplay.progress.won, ++gameplay.tick);
    replay->syncGameplay(gameplay.rules);
    return frame;
  }

  void recordTwoFrames() {
    tap(engine::SC::SDL_SCANCODE_F5);
    gameplay.scene.transform(gameplay.robot).position = {100.f, 300.f};
    gameplay.tick = 1;
    replay->update(gameplay.scene, gameplay.world, gameplay.robot, false, gameplay.tick);
    gameplay.scene.transform(gameplay.robot).position = {200.f, 300.f};
    gameplay.tick = 2;
    replay->update(gameplay.scene, gameplay.world, gameplay.robot, false, gameplay.tick);
    tap(engine::SC::SDL_SCANCODE_F5);
  }
};
