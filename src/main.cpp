#include <robot_factory_escape/game.hpp>

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

int main(int argc, char **argv) {
  try {
    engine::log::init();
    std::optional<NetworkOptions> network;
    NetworkOptions options;
    bool enabled = false;
    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--help") {
        engine::log::info("Usage: robot_factory_escape [--mode client-server|peer-to-peer] "
                          "[--server-host HOST] [--server-port PORT] "
                          "[--advertise-host HOST]");
        return 0;
      }
      if (i + 1 >= argc) throw std::invalid_argument("missing value for " + argument);
      const std::string value = argv[++i];
      if (argument == "--mode") {
        if (value == "peer-to-peer") options.peerToPeer = true;
        else if (value == "client-server") options.peerToPeer = false;
        else throw std::invalid_argument("mode must be client-server or peer-to-peer");
        enabled = true;
      } else if (argument == "--server-host") {
        options.serverHost = value;
        enabled = true;
      } else if (argument == "--server-port") {
        const int port = std::stoi(value);
        if (port < 1 || port > 65535) throw std::invalid_argument("invalid server port");
        options.serverPort = static_cast<std::uint16_t>(port);
        enabled = true;
      } else if (argument == "--advertise-host") {
        options.advertisedHost = value;
        enabled = true;
      } else throw std::invalid_argument("unknown option: " + argument);
    }
    if (enabled) network = options;
    Game game(network);
    game.run();
  } catch (const std::exception &error) {
    engine::log::error("Robot Factory Escape: {}", error.what());
    return 1;
  }
  return 0;
}
