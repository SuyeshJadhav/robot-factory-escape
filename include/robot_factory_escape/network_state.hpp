#pragma once

#include <engine/engine.hpp>

#include <cstdint>
#include <optional>

namespace robotNet {

// The drone is the server's first entity and each client tracks its robot
// first.
inline constexpr engine::NetId droneId =
    engine::makeNetId(engine::kServerId, 0);
inline constexpr engine::NetId movingPlatformId =
    engine::makeNetId(engine::kServerId, 1);
inline engine::NetId robotId(engine::ClientId client) {
  return engine::makeNetId(client, 0);
}

struct AnimationState {
  std::uint8_t clip = 0;
  std::uint16_t frame = 0;
  std::uint16_t elapsedMilliseconds = 0;
};

inline engine::networking::Bytes encode(AnimationState state) {
  return {std::byte{1},
          std::byte{state.clip},
          std::byte(state.frame & 0xff),
          std::byte(state.frame >> 8),
          std::byte(state.elapsedMilliseconds & 0xff),
          std::byte(state.elapsedMilliseconds >> 8)};
}

inline std::optional<AnimationState> decode(engine::networking::ByteView data) {
  if (data.size() != 6 || data[0] != std::byte{1})
    return std::nullopt;
  AnimationState state;
  state.clip = std::to_integer<std::uint8_t>(data[1]);
  state.frame = std::to_integer<std::uint8_t>(data[2]) |
                (std::to_integer<std::uint16_t>(data[3]) << 8);
  state.elapsedMilliseconds = std::to_integer<std::uint8_t>(data[4]) |
                              (std::to_integer<std::uint16_t>(data[5]) << 8);
  if (state.clip > 5)
    return std::nullopt;
  return state;
}

// Remote robots are visual state; local physics must never move one.
inline void makeRemoteVisual(engine::Scene &scene, engine::EntityId robot) {
  scene.removeRigidBody(robot);
  scene.removeCollider(robot);
}

} // namespace robotNet
