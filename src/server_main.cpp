#include <engine/engine.hpp>
#include <robot_factory_escape/drone_patrol.hpp>
#include <robot_factory_escape/network_state.hpp>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t stopping = 0;
void requestStop(int) { stopping = 1; }
}

int main(int argc, char** argv) {
  try {
    engine::log::init();
    engine::networking::ServerConfig config;
    config.relayPlayers = false;
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        engine::log::info("Usage: robot_factory_server [--mode client-server|peer-to-peer] "
                          "[--port PORT] [--bind ADDRESS]");
        return 0;
      }
      if (option == "--mode" && i + 1 < argc) {
        const std::string value = argv[++i];
        if (value == "client-server") config.relayPlayers = true;
        else if (value == "peer-to-peer") config.relayPlayers = false;
        else throw std::invalid_argument("mode must be client-server or peer-to-peer");
      } else if (option == "--port" && i + 1 < argc) {
        const int value = std::stoi(argv[++i]);
        if (value < 1 || value > 65535) throw std::invalid_argument("invalid port");
        config.joinPort = static_cast<std::uint16_t>(value);
      } else if (option == "--bind" && i + 1 < argc) {
        config.bindAddress = argv[++i];
      } else {
        throw std::invalid_argument("usage: robot_factory_server [--mode client-server|peer-to-peer] [--port 5555] [--bind tcp://*]");
      }
    }
    engine::networking::Server server(config);
    server.start();
    engine::Timeline realTime;
    engine::Timeline serverTime(realTime, 60);
    engine::Scene scene;
    const auto drone = scene.createEntity();
    DronePatrol patrol;
    patrol.advance(scene, drone, 0.f);
    engine::SimulationThread simulation(
        std::move(scene), serverTime, [&](const engine::TickContext& ctx) {
          patrol.advance(ctx.scene, drone, ctx.dt);
          const auto position = ctx.scene.transform(drone).position;
          server.publishWorld(robotNet::encode({position.x, position.y}), ctx.tick);
          if (ctx.tick % 30 == 0)
            engine::log::info("Server drone tick {} position ({}, {}) at {} ms",
                ctx.tick, position.x, position.y,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
          for (const auto& event : server.drain()) {
            if (event.kind == engine::networking::ClientEvent::Kind::Joined)
              engine::log::info("Client {} joined; {} connected", event.client, server.connectedClients());
            else if (event.kind == engine::networking::ClientEvent::Kind::Left)
              engine::log::info("Client {} left; {} connected", event.client, server.connectedClients());
          }
        });
    simulation.start();
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    engine::log::info("Headless {} coordinator listening on {} port {}",
                      config.relayPlayers ? "client-server" : "peer-to-peer",
                      config.bindAddress, server.port());
    while (!stopping && !simulation.failure())
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    simulation.stop();
    server.stop();
    if (const auto error = simulation.failure()) std::rethrow_exception(error);
  } catch (const std::exception& error) {
    engine::log::error("Coordinator: {}", error.what());
    return 1;
  }
  return 0;
}
