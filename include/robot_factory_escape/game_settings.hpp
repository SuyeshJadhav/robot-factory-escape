#pragma once

// Game-specific tuning; the shared engine only applies the supplied values.
namespace gameSettings {
inline constexpr float gravity = 1800.f; // Logical pixels per second squared.
inline constexpr float jumpSpeed = 880.f; // Upward speed in logical pixels per second.
} // namespace gameSettings
