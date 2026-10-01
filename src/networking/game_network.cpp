#include <robot_factory_escape/game.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>

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
    peerSession_->replicator().bind(robotNet::droneId, drone_);
    peerSession_->replicator().bind(robotNet::movingPlatformId, movingPlatform_);
    if (peerSession_->replicator().track(robot_) != robotNet::robotId(localClientId_))
      throw std::runtime_error("unexpected robot network ID");
    peerSession_->start();
  } else {
    clientSession_ = std::make_unique<engine::networking::sessionClient>(endpoint);
    if (!clientSession_->join())
      throw std::runtime_error("could not join server at " + endpoint);
    localClientId_ = clientSession_->id();
    clientSession_->replicator().bind(robotNet::droneId, drone_);
    clientSession_->replicator().bind(robotNet::movingPlatformId, movingPlatform_);
    if (clientSession_->replicator().track(robot_) != robotNet::robotId(localClientId_))
      throw std::runtime_error("unexpected robot network ID");
    clientSession_->setHeartbeat(100);
    clientSession_->start();
  }
  resetRobot(scene_, robot_, localSpawn_, grounded_);
  scene_.getText(networkText_)->val = std::string("NETWORK: ") + (peerToPeer_ ? "PEER" : "SERVER") +
                                      "  PLAYER " + std::to_string(localClientId_) + "  CONNECTED";
  engine::log::info("Joined as client {} ({})", localClientId_,
                    peerToPeer_ ? "peer-to-peer" : "client-server");
}

void Game::publishRobot(engine::Scene &scene, std::int64_t tick) {
  auto &replicator = peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  replicator.setExtra(robotNet::robotId(localClientId_),
                      robotNet::encode(sprites_.state(scene, robot_)));
  if (peerToPeer_)
    peerSession_->publishScene(scene, tick);
  else
    clientSession_->submitScene(scene, tick);
}

void Game::pumpNetwork(engine::Scene &scene, bool paused) {
  const auto previousPlatform = scene.transform(movingPlatform_).position;
  if (peerToPeer_) {
    peerSession_->applyUpdates(scene);
    remoteIds_.clear();
    for (const auto &client : peerSession_->roster())
      if (client.id != localClientId_)
        remoteIds_.insert(client.id);
  } else {
    clientSession_->applySnapshot(scene);
    for (const auto &event : clientSession_->drainRosterEvents()) {
      if (event.client.id == localClientId_)
        continue;
      if (event.change == engine::networking::RosterChange::Joined)
        remoteIds_.insert(event.client.id);
      else
        remoteIds_.erase(event.client.id);
    }
  }
  // Snapshots follow the server clock, independently of the local tick rate.
  // Applying each observed delta here also handles multiple polls per tick.
  if (!paused && !progress_.won)
    carryPlayerWithPlatform(scene, robot_, movingPlatform_, previousPlatform, solids_, grounded_);
  auto &replicator = peerToPeer_ ? peerSession_->replicator() : clientSession_->replicator();
  // A snapshot can arrive before its roster event. Exclude every foreign
  // robot from physics immediately, even if its visual setup waits one poll.
  std::vector<engine::EntityId> foreignBodies;
  for (const auto &[id, body] : scene.rigidBodies()) {
    if (id == robot_)
      continue;
    if (const auto netId = replicator.netIdOf(id);
        netId && engine::ownerOf(*netId) != engine::kServerId)
      foreignBodies.push_back(id);
  }
  for (const auto id : foreignBodies)
    robotNet::makeRemoteVisual(scene, id);
  for (auto id : remoteIds_) {
    const auto remote = replicator.entityOf(robotNet::robotId(id));
    if (!remote || !scene.hasEntity(*remote))
      continue;
    robotNet::makeRemoteVisual(scene, *remote);
    // The renderer's texture handles are local, so restore the local sprite
    // clip.
    if (const auto *bytes = replicator.extraOf(robotNet::robotId(id))) {
      if (const auto state = robotNet::decode(*bytes))
        sprites_.applyRemote(scene, *remote, *state);
    }
    if (auto *shape = scene.getShape(*remote))
      shape->color = {255, 210, 60, 255};
  }
}
