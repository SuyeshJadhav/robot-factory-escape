#pragma once

#include <engine/networking/networking.hpp>
#include <engine/entity.hpp>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace robotNet {
struct Position { float x, y; };

inline engine::networking::Bytes encode(Position p) {
  engine::networking::Bytes bytes;
  for (float value : {p.x, p.y}) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    for (unsigned shift = 0; shift < 32; shift += 8)
      bytes.push_back(std::byte((bits >> shift) & 0xff));
  }
  return bytes;
}

inline std::optional<Position> decode(engine::networking::ByteView bytes) {
  if (bytes.size() != 8) return std::nullopt;
  float values[2];
  for (unsigned n = 0; n < 2; ++n) {
    std::uint32_t bits = 0;
    for (unsigned i = 0; i < 4; ++i)
      bits |= std::to_integer<std::uint32_t>(bytes[n * 4 + i]) << (8 * i);
    values[n] = std::bit_cast<float>(bits);
    if (!std::isfinite(values[n])) return std::nullopt;
  }
  return Position{values[0], values[1]};
}

class RemoteRobots {
public:
  explicit RemoteRobots(engine::ClientId self) : self_(self) {}

  bool apply(engine::Scene& scene, const engine::networking::ServerEvent& event) {
    using Kind = engine::networking::ServerEvent::Kind;
    if (event.client == 0 || event.client == self_) return false;
    if (event.kind == Kind::PlayerLeft) {
      if (const auto it = robots_.find(event.client); it != robots_.end()) {
        scene.destroyEntity(it->second.entity);
        robots_.erase(it);
        return true;
      }
      return false;
    }
    if (event.kind != Kind::Player) return false;
    const auto position = decode(event.payload);
    if (!position) return false;
    auto it = robots_.find(event.client);
    if (it != robots_.end() && event.tick < it->second.tick) return false;
    if (it == robots_.end()) {
      const auto entity = scene.createEntity();
      scene.addShape(entity, {.size = {96.f, 128.f},
                              .color = {255, 210, 60, 255},
                              .texture = std::nullopt});
      it = robots_.emplace(event.client, Record{entity, event.tick}).first;
    }
    it->second.tick = event.tick;
    scene.transform(it->second.entity).position = {position->x, position->y};
    return true;
  }

  std::optional<engine::EntityId> entity(engine::ClientId id) const {
    if (const auto it = robots_.find(id); it != robots_.end()) return it->second.entity;
    return std::nullopt;
  }
  std::size_t size() const { return robots_.size(); }

private:
  struct Record { engine::EntityId entity; std::int64_t tick; };
  engine::ClientId self_;
  std::unordered_map<engine::ClientId, Record> robots_;
};

class DroneState {
public:
  bool apply(engine::Scene& scene, engine::EntityId drone,
             const engine::networking::ServerEvent& event) {
    if (event.kind != engine::networking::ServerEvent::Kind::World ||
        event.tick < lastTick_) return false;
    const auto position = decode(event.payload);
    if (!position) return false;
    scene.transform(drone).position = {position->x, position->y};
    lastTick_ = event.tick;
    return true;
  }

  std::int64_t lastTick() const { return lastTick_; }

private:
  std::int64_t lastTick_ = -1;
};
} // namespace robotNet
