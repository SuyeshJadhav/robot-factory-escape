#include <iostream>
#include <robot_factory_escape/graphics/game_sprites.hpp>
#include <robot_factory_escape/replay/game_replay.hpp>
#include <stdexcept>

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

int main() {
  try {
    engine::Window window("Sprite checks", 320, 240);
    engine::Renderer renderer(window);
    engine::Scene scene;
    const auto robot = scene.createEntity();
    const auto drone = scene.createEntity();
    scene.addRigidBody(robot);
    scene.addShape(robot, {.texture = std::nullopt});
    scene.addShape(drone, {.texture = std::nullopt});
    GameSprites sprites;
    sprites.load(renderer, GAME_ASSET_DIR);
    sprites.reset(scene, robot, drone);
    const auto idle = scene.getSpriteAnimation(robot)->sheet;
    scene.getRigidBody(robot)->velocity.x = 300.f;
    sprites.update(scene, robot, true, false, 0.05f);
    const auto walk = scene.getSpriteAnimation(robot)->sheet;
    require(walk != idle, "Movement did not select walking");
    const auto frame = scene.getSpriteAnimation(robot)->currentFrame;
    sprites.update(scene, robot, true, false, 0.05f);
    require(scene.getSpriteAnimation(robot)->currentFrame > frame, "Walking restarts each frame");
    scene.getRigidBody(robot)->velocity.x = -300.f;
    sprites.update(scene, robot, true, false, 0.f);
    require(scene.getSpriteAnimation(robot)->sheet != walk, "Facing did not change");
    sprites.update(scene, robot, false, false, 0.f);
    require(!scene.getSpriteAnimation(robot)->loop, "Jump should hold its final frame");
    const auto remote = scene.createEntity();
    scene.addShape(remote, {.texture = std::nullopt});
    auto animationBytes = robotNet::encode(sprites.state(scene, robot));
    require(sprites.applyRemote(scene, remote, *robotNet::decode(animationBytes)),
            "Remote animation state was not applied");
    require(scene.getSpriteAnimation(remote)->sheet == scene.getSpriteAnimation(robot)->sheet,
            "Remote robot used the wrong local sprite sheet");
    require(scene.getSpriteAnimation(remote)->currentFrame ==
                scene.getSpriteAnimation(robot)->currentFrame,
            "Remote robot used the wrong frame");
    require(!sprites.applyRemote(scene, remote, {5, 65535, 0}),
            "Out-of-range animation frame was accepted");
    const auto frozenFrame = scene.getSpriteAnimation(drone)->currentFrame;
    sprites.update(scene, robot, false, true, 0.1f);
    require(scene.getSpriteAnimation(drone)->currentFrame == frozenFrame,
            "Win did not freeze sprites");
    // Exercise each loaded animation frame through the real renderer.
    for (float direction : {300.f, -300.f}) {
      for (int state = 0; state < 3; ++state) {
        scene.getRigidBody(robot)->velocity.x = state == 0 ? 0.f : direction;
        for (int i = 0; i < 150; ++i) {
          sprites.update(scene, robot, state != 2, false, 0.025f);
          renderer.clear({0, 0, 0, 255});
          renderer.drawEntities(scene);
          renderer.present();
        }
      }
    }
    sprites.reset(scene, robot, drone);
    require(scene.getSpriteAnimation(robot)->sheet == idle &&
                scene.getSpriteAnimation(robot)->currentFrame == 0,
            "Restart did not reset sprites");
    scene.removeShape(robot);
    scene.removeSpriteAnimation(robot);
    const auto droneBeforeDeath = scene.getSpriteAnimation(drone)->currentFrame;
    sprites.update(scene, robot, true, false, 0.1f);
    require(!scene.getShape(robot) && !scene.getSpriteAnimation(robot),
            "Dead robot artwork was reattached by an animation update");
    require(scene.getSpriteAnimation(drone)->currentFrame != droneBeforeDeath,
            "Drone animation stopped while robot was dead");
    renderer.clear({0, 0, 0, 255});
    renderer.drawEntities(scene);
    renderer.present();
    scene.addShape(robot, {.texture = std::nullopt});
    sprites.reset(scene, robot, drone);
    require(scene.getSpriteAnimation(robot)->sheet == idle,
            "Respawn did not restore robot artwork after the hidden interval");
    engine::Scene replayLive;
    for (int i = 0; i < 25; ++i)
      replayLive.createEntity();
    const auto replayWorld = factory::createWorld(replayLive);
    const auto replayRobot = factory::createPlayer(replayLive, replayWorld);
    sprites.reset(replayLive, replayRobot, replayWorld.drone);
    const auto font = renderer.loadFont(GAME_FONT_PATH, 24.f);
    const auto backdrop = renderer.loadTexture(std::string(GAME_ASSET_DIR) +
        "background/cyberpunk_background_backdropa_idle.png");
    engine::events::eventManager events;
    factory::ReplaySession replay(events, font);
    events.raise(engine::events::replayRecordStart{}, 0);
    events.run(replayLive, 0);
    replay.update(replayLive, replayWorld, replayRobot, false, 1);
    events.raise(engine::events::replayRecordStop{}, 1);
    events.raise(engine::events::replayPlayStart{}, 2);
    events.run(replayLive, 2);
    const auto* displayed = replay.update(replayLive, replayWorld, replayRobot, false, 2);
    require(displayed && !displayed->hasEntity(replayRobot),
            "Replay render fixture must use different entity IDs");
    require(!factory::renderReplayFrame(renderer, replayLive, backdrop),
            "Live Scene must not take the replay-only rendering branch");
    renderer.setCamera({123.f, 45.f});
    require(factory::renderReplayFrame(renderer, *displayed, backdrop) &&
                renderer.camera() == glm::vec2{0.f, 0.f},
            "Reconstructed replay and real banner must render at camera zero without live IDs");
    std::cout << "Sprite loading, state transitions, playback, and rendering "
                 "passed.\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
