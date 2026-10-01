#pragma once

#include <array>
#include <robot_factory_escape/gameplay/moving_platform.hpp>

// Shared geometry for level construction, server replication, and rendering.
namespace levelLayout {
inline constexpr glm::vec2 levelSize{1920.f, 1080.f};
inline constexpr glm::vec2 robotSpawn{120.f, 300.f};
inline constexpr glm::vec2 robotSize{96.f, 128.f};
inline constexpr glm::vec2 robotColliderSize{80.f, 128.f};
inline constexpr glm::vec2 droneSize{130.f, 80.f};
inline constexpr glm::vec2 platformSize{300.f, 66.f};
inline constexpr glm::vec2 platformColliderSize{300.f, 24.f};
// Uphill steps rise 190 pixels, just below the robot's jump height.
inline constexpr std::array platformPositions{
    glm::vec2{MovingPlatformPatrol::left, MovingPlatformPatrol::height},
    glm::vec2{650.f, 560.f},
    glm::vec2{980.f, 740.f},
    glm::vec2{1280.f, 550.f},
    glm::vec2{1560.f, 360.f},
};
} // namespace levelLayout
