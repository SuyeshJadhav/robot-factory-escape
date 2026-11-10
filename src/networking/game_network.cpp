#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>
#include <robot_factory_escape/gameplay/level_layout.hpp>
#include <robot_factory_escape/networking/world_binding.hpp>

void Game::connectNetwork(const NetworkOptions &network) {
  peerToPeer_ = network.peerToPeer;
  const std::string endpoint =
      "tcp://" + network.serverHost + ":" + std::to_string(network.serverPort);
  if (peerToPeer_) {
    peerSession_ =
        std::make_unique<engine::networking::peerSession>(endpoint, network.advertisedHost);
    if (!peerSession_->join())
      throw std::runtime_error("could not join peer coordinator at " + endpoint);
    localClientId_ = peerSession_->id();
    robotNet::bindWorld(peerSession_->replicator(), world_);
    if (peerSession_->replicator().track(robot_) != robotNet::robotId(localClientId_))
      throw std::runtime_error("unexpected robot network ID");
    peerSession_->start();
  } else {
    clientSession_ = std::make_unique<engine::networking::sessionClient>(endpoint);
    if (!clientSession_->join())
      throw std::runtime_error("could not join server at " + endpoint);
    localClientId_ = clientSession_->id();
    robotNet::bindWorld(clientSession_->replicator(), world_);
    if (clientSession_->replicator().track(robot_) != robotNet::robotId(localClientId_))
      throw std::runtime_error("unexpected robot network ID");
    clientSession_->setHeartbeat(100);
    clientSession_->start();
  }
  factory::assignSpawn(scene_,world_,robot_,localClientId_);
  scene_.getText(networkText_)->val = std::string("NETWORK: ") + (peerToPeer_ ? "PEER" : "SERVER") +
                                      "  PLAYER " + std::to_string(localClientId_) + "  CONNECTED";
  engine::log::info("Joined as client {} ({})", localClientId_,
                    peerToPeer_ ? "peer-to-peer" : "client-server");
}

void Game::publishLifeEvents(std::int64_t tick) {
  const auto notices = rules_->takeLifeEvents();
  if (peerSession_)
    robotNet::publishLifeEvents(*peerSession_, notices, tick);
  else if (clientSession_)
    robotNet::publishLifeEvents(*clientSession_, notices, tick);
}

void Game::publishRobot(engine::Scene &scene, std::int64_t tick) {
  auto &replicator = peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  auto state = sprites_.state(scene, robot_);
  state.lifeRevision = rules_->revision();
  state.alive = rules_->alive();
  replicator.setExtra(robotNet::robotId(localClientId_), robotNet::encode(state));
  if (peerToPeer_)
    peerSession_->publishScene(scene, tick);
  else
    clientSession_->submitScene(scene, tick);
}

void Game::pumpNetwork(engine::Scene &scene, bool paused) {
  const auto previousPlatform = scene.transform(world_.movingPlatform).position;
  auto &replicator = peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  const auto forget = [this](engine::ClientId owner) {
    departedIds_.insert(owner);
    remoteIds_.erase(owner);
    remoteLives_.forget(owner);
    lifeNotices_.forget(owner);
    remoteAnimation_.erase(robotNet::robotId(owner));
  };
  if (peerToPeer_) {
    const auto previousIds = remoteIds_;
    peerSession_->applyUpdates(scene);
    remoteIds_.clear();
    for (const auto &client : peerSession_->roster())
      if (client.id != localClientId_)
        remoteIds_.insert(client.id);
    for (auto owner : previousIds)
      if (!remoteIds_.contains(owner))
        forget(owner);
  } else {
    clientSession_->applySnapshot(scene);
    for (const auto &event : clientSession_->drainRosterEvents()) {
      if (event.client.id == localClientId_)
        continue;
      if (event.change == engine::networking::RosterChange::Joined)
        remoteIds_.insert(event.client.id);
      else
        forget(event.client.id);
    }
  }
  // Snapshots follow the server clock, independently of the local tick rate.
  // Applying each observed delta here also handles multiple polls per tick.
  if (!paused && !progress_.won && rules_->alive())
    carryPlayerWithPlatform(scene, robot_, world_.movingPlatform, previousPlatform, world_.solids, grounded_);
  for (const auto owner : departedIds_)
    if (replicator.entityOf(robotNet::robotId(owner)))
      replicator.dropOwner(scene, owner);
  // A snapshot may precede its roster event: reconcile every materialized robot.
  std::vector<engine::NetId> foreign;
  for (const auto &[entity, transform] : scene.transforms()) {
    if (entity == robot_)
      continue;
    const auto netId = replicator.netIdOf(entity);
    if (!netId || engine::ownerOf(*netId) == engine::kServerId ||
        *netId != robotNet::robotId(engine::ownerOf(*netId)))
      continue;
    foreign.push_back(*netId);
    if (robotNet::reconcileRemotePose(scene, replicator, remoteLives_, *netId)) {
      if (const auto* bytes = replicator.extraOf(*netId))
        if (const auto state = robotNet::decode(*bytes))
          remoteAnimation_[*netId] = *state;
    }
  }
  events_.receive(peerToPeer_ ? peerSession_->drainMessages() : clientSession_->drainMessages(),
                  lastSimulationTick_);
  events_.run(scene, lastSimulationTick_);
  const engine::Shape fallback{.size = levelLayout::robotSize, .color = {255, 210, 60, 255}};
  for (const auto id : foreign) {
    const auto remote = *replicator.entityOf(id);
    if (const auto animation = remoteAnimation_.find(id);
        animation != remoteAnimation_.end() && remoteLives_.alive(id) != false)
      sprites_.applyRemote(scene, remote, animation->second);
    robotNet::enforceRemoteLife(scene, robot_, replicator, remoteLives_, id, fallback);
    if (auto *shape = scene.getShape(remote))
      shape->color = {255, 210, 60, 255};
  }
}
