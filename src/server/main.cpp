#include <engine/engine.hpp>
#include <robot_factory_escape/gameplay/drone_patrol.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/gameplay/moving_platform.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t stopping = 0;
void requestStop(int) { stopping = 1; }
} // namespace

int main(int argc, char **argv) {
  try {
    engine::log::init();
    bool peerToPeer = false;
    int port = 5555;
    int statsIntervalMs = 0;
    std::string bind = "tcp://*";
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        engine::log::info("Usage: robot_factory_server [--mode client-server|peer-to-peer] "
                          "[--port PORT] [--bind ADDRESS] [--stats-interval-ms N]");
        return 0;
      }
      if (i + 1 >= argc)
        throw std::invalid_argument("missing value for " + option);
      const std::string value = argv[++i];
      if (option == "--mode") {
        if (value == "peer-to-peer")
          peerToPeer = true;
        else if (value == "client-server")
          peerToPeer = false;
        else
          throw std::invalid_argument("mode must be client-server or peer-to-peer");
      } else if (option == "--port") {
        port = std::stoi(value);
        if (port < 1 || port > 65535)
          throw std::invalid_argument("invalid port");
      } else if (option == "--bind")
        bind = value;
      else if (option == "--stats-interval-ms") {
        statsIntervalMs = std::stoi(value);
        if (statsIntervalMs < 0)
          throw std::invalid_argument("stats interval must not be negative");
      } else
        throw std::invalid_argument("unknown option: " + option);
    }

    engine::networking::sessionServer server(bind + ":" + std::to_string(port));
    engine::Timeline realTime;
    engine::Timeline serverTime(realTime, 60);
    engine::Scene scene;
    const auto drone = scene.createEntity();
    DronePatrol patrol;
    patrol.advance(scene, drone, 0.f);
    scene.addShape(drone, {.size = levelLayout::droneSize, .color = {235, 85, 93, 255}});
    scene.addCollider(drone, {.size = levelLayout::droneSize});
    if (server.replicator().track(drone) != robotNet::droneId)
      throw std::runtime_error("unexpected drone network ID");
    const auto movingPlatform = scene.createEntity();
    MovingPlatformPatrol platformPatrol;
    platformPatrol.advance(scene, movingPlatform, 0.f);
    scene.addShape(movingPlatform,
                   {.size = levelLayout::platformSize, .color = {255, 255, 255, 255}});
    scene.addCollider(movingPlatform, {.size = levelLayout::platformColliderSize});
    if (server.replicator().track(movingPlatform) != robotNet::movingPlatformId)
      throw std::runtime_error("unexpected moving-platform network ID");
    server.publishScene(scene, 0);
    server.start();
    std::atomic<float> platformX{MovingPlatformPatrol::left};

    engine::SimulationThread simulation(
        std::move(scene), serverTime, [&](const engine::TickContext &ctx) {
          if (!peerToPeer)
            server.applyClientStates(ctx.scene);
          patrol.advance(ctx.scene, drone, ctx.dt);
          platformPatrol.advance(ctx.scene, movingPlatform, ctx.dt);
          platformX.store(ctx.scene.transform(movingPlatform).position.x);
          server.publishScene(ctx.scene, ctx.tick);
          for (const auto &event : server.drainRosterEvents()) {
            engine::log::info("Client {} {}; {} connected", event.client.id,
                              event.change == engine::networking::RosterChange::Joined ? "joined"
                                                                                       : "left",
                              server.roster().size());
          }
        });
    simulation.start();
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    engine::log::info("Headless {} server listening on {} port {}",
                      peerToPeer ? "peer-to-peer" : "client-server", bind, server.port());
    auto nextStats = std::chrono::steady_clock::now() + std::chrono::milliseconds(statsIntervalMs);
    while (!stopping && !simulation.failure()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (statsIntervalMs > 0 && std::chrono::steady_clock::now() >= nextStats) {
        engine::log::info("Shared moving platform x={}", platformX.load());
        for (const auto &client : server.stats())
          engine::log::info("Client {} updates={} state={} snapshots={}", client.id, client.updates,
                            client.stateUpdates, client.snapshotsSent);
        nextStats = std::chrono::steady_clock::now() + std::chrono::milliseconds(statsIntervalMs);
      }
    }
    simulation.stop();
    server.stop();
    if (const auto error = simulation.failure())
      std::rethrow_exception(error);
  } catch (const std::exception &error) {
    engine::log::error("Server: {}", error.what());
    return 1;
  }
  return 0;
}
