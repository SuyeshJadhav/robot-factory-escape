#include <robot_factory_escape/networking/life_events.hpp>
#include "test_support.hpp"

#include <iostream>

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
  std::cout << "event wire checks passed\n";
}
