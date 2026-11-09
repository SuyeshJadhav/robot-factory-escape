#include <robot_factory_escape/networking/life_events.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <algorithm>

namespace robotNet {
namespace {
bool isRobot(engine::NetId entity) {
  return engine::ownerOf(entity) != engine::kServerId &&
         entity == robotId(engine::ownerOf(entity));
}
bool validLife(const LifeEvent& event) {
  return isRobot(event.entity) && event.revision > 0 &&
      (event.kind == LifeKind::Death || event.kind == LifeKind::Spawn) &&
      std::isfinite(event.position.x) && std::isfinite(event.position.y);
}
}

bool LifeRelay::accept(const LifeEvent& event, bool senderConnected) {
  if (!senderConnected || !event.remote || event.from == engine::kServerId ||
      event.from > engine::kMaxClientIdPerRun || !validLife(event) ||
      event.entity != robotId(event.from))
    return false;
  const auto found = latest_.find(event.entity);
  if (found != latest_.end() && event.revision <= found->second.revision)
    return false;
  latest_[event.entity] = event;
  return true;
}

void LifeRelay::forget(engine::ClientId owner) {
  std::erase_if(latest_, [owner](const auto& entry) { return engine::ownerOf(entry.first) == owner; });
}

std::vector<LifeEvent> LifeRelay::latest() const {
  std::vector<LifeEvent> events;
  events.reserve(latest_.size());
  for (const auto& [id, event] : latest_)
    events.push_back(event);
  std::sort(events.begin(), events.end(),
            [](const auto& a, const auto& b) { return a.entity < b.entity; });
  return events;
}

LifeServer::LifeServer(engine::networking::sessionServer& server) : server_(server) {
  events_.subscribe<LifeEvent>([this](const LifeEvent& event, engine::Scene&) {
    const auto roster = server_.roster();
    const bool connected = std::any_of(roster.begin(), roster.end(),
        [&](const auto& client) { return client.id == event.from; });
    if (relay_.accept(event, connected))
      server_.broadcast(LifeEvent::kType, event.encode(), events_.now());
  }, true);
}

void LifeServer::pump(engine::Scene& scene, std::int64_t tick,
                      const std::deque<engine::networking::rosterEvent>& changes) {
  for (const auto& change : changes) {
    if (change.change == engine::networking::RosterChange::Left) {
      relay_.forget(change.client.id);
    } else {
      for (const auto& event : relay_.latest())
        server_.send(change.client.id, LifeEvent::kType, event.encode(), tick);
    }
  }
  events_.receive(server_.drainMessages(), tick);
  events_.run(scene, tick);
}

bool RemoteLives::observe(const LifeEvent& event) {
  if (!validLife(event))
    return false;
  const auto found = entries_.find(event.entity);
  if (found != entries_.end() && event.revision <= found->second.revision)
    return false;
  observeSnapshot(event.entity, event.revision, event.kind == LifeKind::Spawn);
  if (event.kind == LifeKind::Spawn)
    entries_.at(event.entity).pendingSpawn = event.position;
  return true;
}

bool RemoteLives::observeSnapshot(engine::NetId entity, std::uint32_t revision, bool alive) {
  if (!isRobot(entity))
    return false;
  auto found = entries_.find(entity);
  if (found == entries_.end()) {
    entries_.emplace(entity, Entry{revision, alive, std::nullopt, std::nullopt});
    return true;
  }
  auto& entry = found->second;
  if (revision < entry.revision || (revision == entry.revision && alive != entry.alive))
    return false;
  entry.revision = revision;
  entry.alive = alive;
  entry.pendingSpawn.reset();
  return true;
}

std::optional<bool> RemoteLives::alive(engine::NetId entity) const {
  const auto found = entries_.find(entity);
  if (found == entries_.end())
    return std::nullopt;
  return found->second.alive;
}

std::optional<glm::vec2> RemoteLives::takeSpawnPosition(engine::NetId entity) {
  const auto found = entries_.find(entity);
  if (found == entries_.end())
    return std::nullopt;
  const auto position = found->second.pendingSpawn;
  found->second.pendingSpawn.reset();
  return position;
}

void RemoteLives::rememberPose(engine::NetId entity, const engine::Transform& pose) {
  const auto found = entries_.find(entity);
  if (found != entries_.end() && std::isfinite(pose.position.x) &&
      std::isfinite(pose.position.y) &&
      std::isfinite(pose.scale.x) && std::isfinite(pose.scale.y))
    found->second.pose = pose;
}

std::optional<engine::Transform> RemoteLives::acceptedPose(engine::NetId entity) const {
  const auto found = entries_.find(entity);
  if (found == entries_.end())
    return std::nullopt;
  return found->second.pose;
}

void RemoteLives::forget(engine::ClientId owner) {
  std::erase_if(entries_, [owner](const auto& entry) { return engine::ownerOf(entry.first) == owner; });
}

bool observeRemoteLife(RemoteLives& cache, const LifeEvent& event, engine::ClientId self,
                        const std::unordered_set<engine::ClientId>& departed) {
  const auto owner = engine::ownerOf(event.entity);
  return event.remote && event.from == engine::kServerId && owner != self &&
         !departed.contains(owner) && cache.observe(event);
}

bool reconcileRemotePose(engine::Scene& scene,
                         const engine::networking::sceneReplicator& replicator,
                         RemoteLives& cache, engine::NetId id) {
  const auto entity = replicator.entityOf(id);
  if (!isRobot(id) || !entity || !scene.hasEntity(*entity))
    return false;
  const auto* bytes = replicator.extraOf(id);
  const auto state = bytes ? decode(*bytes) : std::nullopt;
  const auto& pose = scene.transform(*entity);
  if (state && std::isfinite(pose.position.x) && std::isfinite(pose.position.y) &&
      std::isfinite(pose.scale.x) && std::isfinite(pose.scale.y) &&
      cache.observeSnapshot(id, state->lifeRevision, state->alive)) {
    cache.rememberPose(id, pose);
    return true;
  }
  if (const auto accepted = cache.acceptedPose(id))
    scene.transform(*entity) = *accepted;
  return false;
}

bool enforceRemoteLife(engine::Scene& scene, engine::EntityId local,
                        const engine::networking::sceneReplicator& replicator,
                        RemoteLives& cache, engine::NetId id, const engine::Shape& fallback) {
  const auto entity = replicator.entityOf(id);
  if (!isRobot(id) || !entity || *entity == local || !scene.hasEntity(*entity))
    return false;
  makeRemoteVisual(scene, *entity);
  const auto alive = cache.alive(id);
  if (alive == false) {
    scene.removeShape(*entity);
    scene.removeSpriteAnimation(*entity);
  } else if (alive == true) {
    if (!scene.getShape(*entity))
      scene.addShape(*entity, fallback);
    if (const auto position = cache.takeSpawnPosition(id)) {
      scene.transform(*entity).position = *position;
      cache.rememberPose(id, scene.transform(*entity));
    }
  }
  return alive.has_value();
}

} // namespace robotNet
