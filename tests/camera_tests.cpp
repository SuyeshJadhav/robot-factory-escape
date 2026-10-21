#include "test_support.hpp"
#include <robot_factory_escape/gameplay/factory_world.hpp>
#include <robot_factory_escape/graphics/camera.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

int main() {
  return test::run([] {
    engine::Scene scene;
    const auto world = factory::createWorld(scene);
    const auto robot = factory::createPlayer(scene, world);
    const auto hud = scene.createEntity();
    scene.transform(hud).position = {32.f, 126.f};
    scene.addText(hud, {.val = "STATUS", .font = 0});
    engine::networking::sceneReplicator rep(1);
    rep.track(robot);
    test::require(factory::cameraForPlayer(scene, robot, 1920.f).x == 0.f, "left clamp");
    scene.transform(robot).position.x = 2000.f;
    test::require(factory::cameraForPlayer(scene, robot, 1920.f).x == 1080.f,
                  "follow collider center");
    scene.transform(robot).position.x = 3000.f;
    const auto camera = factory::cameraForPlayer(scene, robot, 1920.f);
    test::require(camera.x == 1280.f && camera.y == 0.f, "right clamp");
    test::require(factory::cameraForPlayer(scene, robot, 1280.f).x == 1920.f,
                  "constant viewport follows without using proportional width");
    test::require(factory::cameraForPlayer(scene, robot, 4000.f).x == 0.f,
                  "viewport larger than world does not invert clamp");
    const auto packet = rep.encodeOwned(scene);
    const auto render = factory::cameraRenderScene(scene, camera);
    test::require(render.transform(robot).position.x == 3000.f,
                  "renderer applies world offset once");
    test::require(render.transform(hud).position - camera == glm::vec2{32.f, 126.f},
                  "HUD stays screen anchored");
    test::require(scene.transform(hud).position == glm::vec2{32.f, 126.f} &&
                      scene.transform(robot).position.x == 3000.f &&
                      rep.encodeOwned(scene) == packet,
                  "camera never mutates simulation or outgoing snapshots");
    const auto again = factory::cameraRenderScene(scene, camera);
    test::require(again.transform(hud).position == render.transform(hud).position,
                  "HUD offset cannot accumulate across frames");
  });
}
