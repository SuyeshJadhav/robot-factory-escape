#pragma once

#include <glm/vec2.hpp>

// Shared geometry for level construction, server replication, and rendering.
namespace levelLayout {
inline constexpr glm::vec2 levelSize{1920.f, 1080.f}; // logical viewport
inline constexpr glm::vec2 worldSize{3200.f, 1080.f};
inline constexpr glm::vec2 robotSpawn{120.f, 300.f};
inline constexpr glm::vec2 robotSize{96.f, 128.f};
inline constexpr glm::vec2 robotColliderSize{80.f, 128.f};
inline constexpr glm::vec2 droneSize{130.f, 80.f};
inline constexpr glm::vec2 platformSize{300.f, 66.f};
inline constexpr glm::vec2 platformColliderSize{300.f, 24.f};
} // namespace levelLayout
