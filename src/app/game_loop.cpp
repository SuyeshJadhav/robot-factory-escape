#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/graphics/hud.hpp>

#include <chrono>
#include <cmath>
#include <thread>

void Game::run() {
  simulation_ = std::make_unique<engine::SimulationThread>(scene_, gameTime_,
                                                           [this](const engine::TickContext &ctx) {
                                                             handleInput(ctx.scene, ctx.keyboard);
                                                             update(ctx.scene, ctx.dt);
                                                             if (clientSession_ || peerSession_)
                                                               publishRobot(ctx.scene, ctx.tick);
                                                           });
  simulation_->start();
  engine::KeyboardState previous;
  using Clock = std::chrono::steady_clock;
  constexpr auto frameInterval = std::chrono::nanoseconds(1'000'000'000 / 60);
  auto fpsSampleStart = Clock::now();
  unsigned framesInSample = 0;
  std::string fpsLabel = "FPS: --";
  try {
    while (!window_.shouldClose()) {
      const auto frameStart = Clock::now();
      window_.pollEvents();
      const auto keyboard = engine::KeyboardState::capture(input_);
      simulation_->submitKeyboard(keyboard);
      using namespace engine::SC;
      if (keyboard.justPressed(SDL_SCANCODE_T, previous)) {
        simulation_->post([this](engine::Scene &scene, engine::Timeline &time) {
          if (time.paused())
            time.unpause();
          else
            time.pause();
          scene.getText(timeText_)->val = std::string("TIME: ") +
                                          (time.paused() ? "PAUSED " : "RUNNING ") +
                                          speedLabel(time.speedMultiplier());
          engine::log::info("Local simulation {}", time.paused() ? "paused" : "resumed");
        });
      }
      const float requestedSpeed = keyboard.justPressed(SDL_SCANCODE_1, previous)   ? 0.5f
                                   : keyboard.justPressed(SDL_SCANCODE_2, previous) ? 1.f
                                   : keyboard.justPressed(SDL_SCANCODE_3, previous) ? 2.f
                                                                                    : 0.f;
      if (requestedSpeed > 0.f) {
        simulation_->post([this, requestedSpeed](engine::Scene &scene, engine::Timeline &time) {
          time.setSpeedMultiplier(requestedSpeed);
          scene.getText(timeText_)->val = std::string("TIME: ") +
                                          (time.paused() ? "PAUSED " : "RUNNING ") +
                                          speedLabel(requestedSpeed);
        });
      }
      if (keyboard.justPressed(SDL_SCANCODE_P, previous))
        renderer_.toggleScalingMode();
      if (keyboard.justPressed(SDL_SCANCODE_R, previous)) {
        simulation_->post([this](engine::Scene &scene, engine::Timeline &time) {
          restart(scene);
          if (clientSession_ || peerSession_)
            publishRobot(scene, time.now());
        });
      }
      previous = keyboard;

      if (clientSession_ || peerSession_) {
        simulation_->post([this](engine::Scene &scene, engine::Timeline &time) {
          pumpNetwork(scene, time.paused());
          // Rendering continues on wall time when the local Timeline stops.
          // Keep fetching shared state at that cadence instead of falling
          // back to 100/250 ms heartbeat updates (10/4 visible updates/sec).
          if (time.paused()) {
            if (peerToPeer_)
              publishRobot(scene, time.now()); // Frozen state also reaches late joiners.
            else
              clientSession_->requestSnapshot(time.now()); // No movement submission.
          }
        });
        const bool connectedNow =
            peerToPeer_ ? peerSession_->connected() : clientSession_->connected();
        if (connectedNow != connected_) {
          connected_ = connectedNow;
          simulation_->post([this, connectedNow](engine::Scene &scene, engine::Timeline &) {
            scene.getText(networkText_)->val =
                std::string("NETWORK: ") + (peerToPeer_ ? "PEER" : "SERVER") + "  PLAYER " +
                std::to_string(localClientId_) + (connectedNow ? "  CONNECTED" : "  DISCONNECTED");
          });
        }
      }
      if (auto frame = simulation_->takeRenderFrame()) {
        scene_ = std::move(frame->scene);
      }
      if (auto failure = simulation_->failure())
        std::rethrow_exception(failure);
      const float sampleSeconds = std::chrono::duration<float>(frameStart - fpsSampleStart).count();
      if (sampleSeconds >= 1.f) {
        fpsLabel = "FPS: " + std::to_string(std::lround(framesInSample / sampleSeconds));
        fpsSampleStart = frameStart;
        framesInSample = 0;
      }
      // This is a view-only metric: it must not follow the paused game clock.
      scene_.getText(fpsText_)->val = fpsLabel;
      render();
      ++framesInSample;
      // Include rendering time in the budget; multiple clients should not
      // compete to redraw the same 60 Hz world more than 100 times a second.
      std::this_thread::sleep_until(frameStart + frameInterval);
    }
  } catch (...) {
    simulation_->stop();
    if (peerSession_)
      peerSession_->leave();
    if (clientSession_)
      clientSession_->leave();
    throw;
  }
  simulation_->stop();
  if (peerSession_)
    peerSession_->leave();
  if (clientSession_)
    clientSession_->leave();
}
