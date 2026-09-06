#include <robot_factory_escape/game.hpp>

#include <exception>

int main() {
    try {
        engine::log::init();
        Game game;
        game.run();
    } catch (const std::exception& error) {
        engine::log::error("Robot Factory Escape: {}", error.what());
        return 1;
    }
    return 0;
}
