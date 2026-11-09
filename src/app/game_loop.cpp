#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/graphics/hud.hpp>

#include <chrono>
#include <cmath>
#include <thread>

void Game::handleControls(engine::Scene& scene, const engine::events::inputEvent& e) {
  if (!e.pressed)
    return;
  if (const auto status = replays_->input(e, rules_->alive(), gameTime_.paused())) {
    setStatusText(scene, *status);
    return;
  }
  using namespace engine::SC;
  if (e.key == SDL_SCANCODE_R) {
    renderedWon_.store(false);
    setStatusText(scene, "STATUS: AVOID THE PATROL DRONE");
    engine::log::info("Restarted. Reach the exit!");
    return;
  }
  if (e.key == SDL_SCANCODE_T) {
    if (gameTime_.paused())
      gameTime_.unpause();
    else
      gameTime_.pause();
    engine::log::info("Local simulation {}", gameTime_.paused() ? "paused" : "resumed");
  } else {
    const float speed = e.key == SDL_SCANCODE_1 ? 0.5f :
                        e.key == SDL_SCANCODE_2 ? 1.f :
                        e.key == SDL_SCANCODE_3 ? 2.f : 0.f;
    if (speed == 0.f)
      return;
    gameTime_.setSpeedMultiplier(speed);
  }
  scene.getText(timeText_)->val = std::string("TIME: ") +
      (gameTime_.paused() ? "PAUSED " : "RUNNING ") + speedLabel(gameTime_.speedMultiplier());
}

void Game::run() {
  replays_ = std::make_unique<factory::ReplaySession>(events_, hudFont_);
  rules_ = std::make_unique<factory::GameEvents>(events_, world_, robot_, localClientId_,
                                                !clientSession_ && !peerSession_,
                                                grounded_, progress_,
      [this](const engine::events::inputEvent& e, engine::Scene& scene) {
        handleControls(scene, e);
      });
  events_.subscribe<robotNet::LifeEvent>(
      [this](const robotNet::LifeEvent& e, engine::Scene&) {
        robotNet::observeRemoteLife(remoteLives_, e, localClientId_, departedIds_);
      }, true);
  simulation_ = std::make_unique<engine::SimulationThread>(scene_, gameTime_,
                                                           [this](const engine::TickContext &ctx) {
                                                             lastSimulationTick_ = ctx.tick;
                                                             update(ctx.scene, ctx.dt);
                                                             if (clientSession_ || peerSession_)
                                                               publishRobot(ctx.scene, ctx.tick);
                                                           });
  rules_->postStart(*simulation_);
  simulation_->post([this](engine::Scene& scene, engine::Timeline&) {
    publishLifeEvents(0);
    if (clientSession_ || peerSession_)
      publishRobot(scene, 0);
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
      simulation_->post([this, keyboard, previous](engine::Scene& scene, engine::Timeline&) {
        const auto revision = rules_->revision();
        rules_->input(scene, keyboard, previous, lastSimulationTick_);
        if (rules_->revision() != revision && rules_->alive()) {
          sprites_.reset(scene, robot_, world_.drone);
          if (clientSession_ || peerSession_)
            publishRobot(scene, lastSimulationTick_);
        }
        publishLifeEvents(lastSimulationTick_);
      });
      using namespace engine::SC;
      if (keyboard.justPressed(SDL_SCANCODE_P, previous))
        renderer_.toggleScalingMode();

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
