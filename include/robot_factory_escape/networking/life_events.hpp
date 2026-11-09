#pragma once

#include <engine/events/simpleEvent.hpp>
#include <engine/events/eventManager.hpp>
#include <engine/networking/sessionServer.hpp>
#include <engine/networking/sceneReplicator.hpp>
#include <engine/ids.hpp>
#include <engine/entity.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <glm/vec2.hpp>

namespace robotNet {

enum class LifeKind : std::uint8_t { Death = 0, Spawn = 1 };

struct LifeEvent : engine::events::simpleEvent {
  static constexpr std::uint16_t kType = 0x4746;
  engine::NetId entity = 0;
  std::uint32_t revision = 0;
  LifeKind kind = LifeKind::Spawn;
  glm::vec2 position{0.f, 0.f};

  engine::networking::Bytes encode() const {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    engine::networking::Bytes out;
    out.reserve(18);
    out.push_back(std::byte{1});
    out.push_back(std::byte{static_cast<std::uint8_t>(kind)});
    const auto append = [&out](std::uint32_t value) {
      for (unsigned shift = 0; shift != 32; shift += 8)
        out.push_back(std::byte{static_cast<std::uint8_t>(value >> shift)});
    };
    append(entity);
    append(revision);
    append(std::bit_cast<std::uint32_t>(position.x));
    append(std::bit_cast<std::uint32_t>(position.y));
    return out;
  }

  static std::optional<LifeEvent> decode(engine::networking::ByteView in) {
    if (in.size() != 18 || in[0] != std::byte{1})
      return std::nullopt;
    const auto kind = std::to_integer<std::uint8_t>(in[1]);
    if (kind > 1)
      return std::nullopt;
    const auto read = [in](std::size_t offset) {
      std::uint32_t value = 0;
      for (unsigned i = 0; i != 4; ++i)
        value |= std::to_integer<std::uint32_t>(in[offset + i]) << (8 * i);
      return value;
    };
    LifeEvent event;
    event.entity = read(2);
    event.revision = read(6);
    event.kind = static_cast<LifeKind>(kind);
    event.position = {std::bit_cast<float>(read(10)), std::bit_cast<float>(read(14))};
    if (event.revision == 0 || !std::isfinite(event.position.x) ||
        !std::isfinite(event.position.y))
      return std::nullopt;
    return event;
  }
};

static_assert(engine::events::networkedEvent<LifeEvent>);

class LifeRelay {
public:
  bool accept(const LifeEvent&, bool senderConnected);
  void forget(engine::ClientId);
  std::vector<LifeEvent> latest() const;

private:
  std::unordered_map<engine::NetId, LifeEvent> latest_;
};

class LifeServer {
public:
  explicit LifeServer(engine::networking::sessionServer&);
  LifeServer(const LifeServer&) = delete;
  LifeServer& operator=(const LifeServer&) = delete;
  void pump(engine::Scene&, std::int64_t tick,
            const std::deque<engine::networking::rosterEvent>& changes);

private:
  engine::networking::sessionServer& server_;
  LifeRelay relay_;
  engine::events::eventManager events_;
};

class RemoteLives {
public:
  bool observe(const LifeEvent&);
  bool observeSnapshot(engine::NetId, std::uint32_t revision, bool alive);
  std::optional<bool> alive(engine::NetId) const;
  std::optional<glm::vec2> takeSpawnPosition(engine::NetId);
  void rememberPose(engine::NetId, const engine::Transform&);
  std::optional<engine::Transform> acceptedPose(engine::NetId) const;
  void forget(engine::ClientId);

private:
  struct Entry {
    std::uint32_t revision;
    bool alive;
    std::optional<glm::vec2> pendingSpawn;
    std::optional<engine::Transform> pose;
  };
  std::unordered_map<engine::NetId, Entry> entries_;
};

bool observeRemoteLife(RemoteLives&, const LifeEvent&, engine::ClientId self,
                        const std::unordered_set<engine::ClientId>& departed);
bool reconcileRemotePose(engine::Scene&, const engine::networking::sceneReplicator&,
                         RemoteLives&, engine::NetId);
bool enforceRemoteLife(engine::Scene&, engine::EntityId local,
                        const engine::networking::sceneReplicator&, RemoteLives&,
                        engine::NetId, const engine::Shape& fallback);

} // namespace robotNet
