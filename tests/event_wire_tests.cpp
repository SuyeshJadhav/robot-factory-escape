#include <robot_factory_escape/networking/life_events.hpp>
#include "test_support.hpp"

#include <iostream>
#include <limits>

int main() {
  robotNet::LifeEvent sent;
  sent.entity = engine::makeNetId(2, 0);
  sent.revision = 7;
  sent.kind = robotNet::LifeKind::Spawn;
  sent.position = {360.f, 300.f};
  const auto received = robotNet::LifeEvent::decode(sent.encode());
  test::require(received.has_value(), "valid life event decodes");
  test::require(received->entity == sent.entity && received->revision == 7,
                "NetId and revision roundtrip");
  test::require(received->kind == sent.kind && received->position == sent.position,
                "kind and coordinates roundtrip");
  test::require(!received->remote && received->from == engine::kServerId,
                "transport metadata is not supplied by the payload");
  const auto valid = sent.encode();
  test::require(valid.size() == 19 && valid[0] == std::byte{2} &&
                    valid[1] == std::byte{1} && valid[5] == std::byte{2} &&
                    valid[6] == std::byte{7},
                "version, kind, owner and revision have the agreed byte layout");
  for (std::size_t size = 0; size < valid.size(); ++size)
    test::require(!robotNet::LifeEvent::decode({valid.data(), size}),
                  "every truncated payload is rejected");
  auto bytes = valid;
  bytes.push_back(std::byte{0});
  test::require(!robotNet::LifeEvent::decode(bytes), "trailing data is rejected");
  bytes = valid;
  bytes[0] = std::byte{3};
  test::require(!robotNet::LifeEvent::decode(bytes), "unknown version is rejected");
  bytes = valid;
  bytes[18] = std::byte{2};
  test::require(!robotNet::LifeEvent::decode(bytes), "unknown repair flag is rejected");
  bytes = valid;
  bytes[1] = std::byte{2};
  test::require(!robotNet::LifeEvent::decode(bytes), "unknown lifecycle kind is rejected");
  auto invalid = sent;
  invalid.revision = 0;
  test::require(!robotNet::LifeEvent::decode(invalid.encode()), "zero revision is rejected");
  invalid = sent;
  invalid.position.x = std::numeric_limits<float>::quiet_NaN();
  test::require(!robotNet::LifeEvent::decode(invalid.encode()), "NaN coordinate is rejected");
  invalid = sent;
  invalid.position.y = std::numeric_limits<float>::infinity();
  test::require(!robotNet::LifeEvent::decode(invalid.encode()), "infinite coordinate is rejected");
  auto death = sent;
  death.kind = robotNet::LifeKind::Death;
  death.position = {-10.f, -20.f};
  const auto decodedDeath = robotNet::LifeEvent::decode(death.encode());
  test::require(decodedDeath && decodedDeath->kind == robotNet::LifeKind::Death &&
                    decodedDeath->position == death.position,
                "death event and finite negative coordinates are supported");
  std::cout << "event wire checks passed\n";
}
