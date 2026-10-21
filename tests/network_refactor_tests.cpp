#include <robot_factory_escape/networking/network_state.hpp>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void roundTrip(engine::networking::SnapshotFormat format) {
  using engine::networking::SnapshotFormat;
  engine::Scene source;
  const auto robot = source.createEntity();
  source.transform(robot).position = {50.f, 70.f};
  source.addShape(robot, {.size = {96.f, 128.f}, .color = {255, 210, 60, 255}});
  source.addRigidBody(robot, {.velocity = {1.f, 0.f}});
  source.addCollider(robot, {.size = {80.f, 128.f}});

  engine::networking::sceneReplicator sender(2);
  require(sender.format() == SnapshotFormat::Binary, "Binary remains the default");
  sender.setFormat(format);
  require(sender.format() == format, "snapshot format selection");
  const auto netId = sender.track(robot);
  require(netId == robotNet::robotId(2), "game robot NetId unchanged");
  const auto extra = robotNet::encode({5, 32, 12});
  sender.setExtra(netId, extra);
  const auto packet = sender.encodeOwned(source);
  require(!packet.empty(), "snapshot is not empty");
  require((packet.front() == std::byte{'{'}) == (format == SnapshotFormat::Json),
          "snapshot representation marker");

  engine::Scene destination;
  const auto visual = destination.createEntity();
  destination.addShape(visual, {.size = {1.f, 1.f}, .texture = 77});
  engine::networking::sceneReplicator receiver(1);
  receiver.bind(netId, visual);
  require(receiver.apply(destination, packet, 2), "game snapshot applies");
  require(receiver.entityOf(netId) == visual, "bound entity reused, not duplicated");
  require(destination.transform(visual).position.x == 50.f &&
              destination.transform(visual).position.y == 70.f,
          "robot position survives representation change");
  const auto *shape = destination.getShape(visual);
  require(shape && shape->texture == 77, "local texture handle preserved");
  require(destination.getRigidBody(visual) && destination.getCollider(visual),
          "replicated components available before visual-only conversion");
  const auto *received = receiver.extraOf(netId);
  require(received && *received == extra, "animation bytes preserved exactly");
  const auto state = robotNet::decode(*received);
  require(state && state->clip == 5 && state->frame == 32 &&
              state->elapsedMilliseconds == 12,
          "game animation metadata decodes");

  auto invalid = packet;
  invalid.pop_back();
  require(!receiver.apply(destination, invalid, 2), "truncated snapshot rejected");
  require(destination.hasEntity(visual) &&
              destination.transform(visual).position.x == 50.f &&
              destination.transform(visual).position.y == 70.f,
          "invalid snapshot leaves existing robot intact");
  require(receiver.extraOf(netId) && *receiver.extraOf(netId) == extra,
          "invalid snapshot leaves animation metadata intact");

  robotNet::makeRemoteVisual(destination, visual);
  require(!destination.getRigidBody(visual) && !destination.getCollider(visual),
          "remote robot excluded from local physics");
  receiver.dropOwner(destination, 2);
  require(!destination.hasEntity(visual) && !receiver.entityOf(netId),
          "departed owner's remote robot removed");
}
} // namespace

int main() {
  try {
    roundTrip(engine::networking::SnapshotFormat::Binary);
    roundTrip(engine::networking::SnapshotFormat::Json);
    std::cout << "Networking refactor compatibility checks passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
