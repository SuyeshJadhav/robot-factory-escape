#include <robot_factory_escape/gameplay/game_events.hpp>
#include <robot_factory_escape/game_settings.hpp>
#include <robot_factory_escape/gameplay/player_motion.hpp>
#include <robot_factory_escape/networking/network_state.hpp>

#include <algorithm>
#include <utility>

namespace factory {

GameEvents::GameEvents(engine::events::eventManager& events, const World& world,
                       engine::EntityId robot, engine::ClientId owner, bool ownsWorld,
                       bool& grounded, GameProgress& progress, Controls controls)
    : events_(events), world_(world), robot_(robot), owner_(owner), ownsWorld_(ownsWorld),
      grounded_(grounded), progress_(progress), controls_(std::move(controls)) {
  events_.subscribe<engine::events::inputEvent>(
      [this](const engine::events::inputEvent& e, engine::Scene& scene) {
        held_.setKey(e.key, e.pressed);
        if (e.pressed && e.key == engine::SC::SDL_SCANCODE_SPACE && alive_ && !blocked_ &&
            !progress_.won)
          jumpRequested_ = true;
        if (e.pressed && e.key == engine::SC::SDL_SCANCODE_R && !blocked_)
          events_.raise(RestartEvent{}, events_.now(), -30);
        if (controls_)
          controls_(e, scene);
      });
  events_.subscribe<engine::events::spawnEvent>(
      [this](const engine::events::spawnEvent& e, engine::Scene& scene) {
        if (!e.remote && e.entity == robot_)
          spawn(scene);
      });
  events_.subscribe<engine::events::collisionEvent>(
      [this](const engine::events::collisionEvent& e, engine::Scene& scene) {
        const auto other = e.a == robot_ ? e.b : e.b == robot_ ? e.a : 0;
        if (!e.remote && other != 0 &&
            (other == world_.drone || scene.has<DeathZone>(other)))
          events_.raise(engine::events::deathEvent{{}, robot_, other}, events_.now(), -10);
      });
  events_.subscribe<engine::events::deathEvent>(
      [this](const engine::events::deathEvent& e, engine::Scene& scene) {
        if (!e.remote && e.entity == robot_)
          die(scene);
      });
  events_.subscribe<RespawnDue>(
      [this](const RespawnDue& e, engine::Scene&) {
        if (e.generation == generation_ && !alive_)
          events_.raise(engine::events::spawnEvent{{}, robot_, owner_, {}}, events_.now());
      });
  events_.subscribe<RestartEvent>(
      [this](const RestartEvent&, engine::Scene& scene) {
        ++generation_;
        jumpRequested_ = false;
        progress_.won = false;
        if (ownsWorld_)
          resetWorld(scene);
        alive_ = false;
        events_.raise(engine::events::spawnEvent{{}, robot_, owner_, {}}, events_.now());
      });
}

void GameEvents::postStart(engine::SimulationThread& simulation) {
  simulation.post([this](engine::Scene& scene, engine::Timeline&) { start(scene, 0); });
}

void GameEvents::start(engine::Scene& scene, std::int64_t tick) {
  if (const auto* shape = scene.getShape(robot_))
    shape_ = *shape;
  if (const auto* animation = scene.getSpriteAnimation(robot_))
    animation_ = *animation;
  events_.raise(engine::events::spawnEvent{{}, robot_, owner_, {}}, tick);
  events_.run(scene, tick);
}

void GameEvents::spawn(engine::Scene& scene) {
  if (alive_)
    return;
  respawnPlayer(scene, robot_, grounded_);
  if (shape_)
    scene.addShape(robot_, *shape_);
  if (animation_)
    scene.addSpriteAnimation(robot_, *animation_);
  alive_ = true;
  jumpRequested_ = false;
  ++revision_;
  robotNet::LifeEvent notice;
  notice.entity = robotNet::robotId(owner_);
  notice.revision = revision_;
  notice.kind = robotNet::LifeKind::Spawn;
  notice.position = scene.transform(robot_).position;
  outbox_.push_back(notice);
}

void GameEvents::input(engine::Scene& scene, const engine::KeyboardState& now,
                       const engine::KeyboardState& previous, std::int64_t tick) {
  engine::events::raiseInputEvents(events_, now, previous, tick);
  events_.run(scene, tick);
}

void GameEvents::die(engine::Scene& scene) {
  if (!alive_)
    return;
  if (const auto* shape = scene.getShape(robot_))
    shape_ = *shape;
  if (const auto* animation = scene.getSpriteAnimation(robot_))
    animation_ = *animation;
  scene.removeShape(robot_);
  scene.removeSpriteAnimation(robot_);
  scene.getRigidBody(robot_)->velocity = {0.f, 0.f};
  grounded_ = false;
  jumpRequested_ = false;
  alive_ = false;
  ++revision_;
  ++generation_;
  robotNet::LifeEvent notice;
  notice.entity = robotNet::robotId(owner_);
  notice.revision = revision_;
  notice.kind = robotNet::LifeKind::Death;
  notice.position = scene.transform(robot_).position;
  outbox_.push_back(notice);
  events_.raise(RespawnDue{{}, generation_}, events_.now() + 60);
}

void GameEvents::setGameplayBlocked(bool blocked) noexcept {
  if (blocked_ != blocked)
    jumpRequested_ = false;
  blocked_ = blocked;
}

TickResult GameEvents::step(engine::Scene& scene, engine::PhysicsSystem& physics,
                            std::int64_t tick, float dt) {
  events_.run(scene, tick);
  if (blocked_ || progress_.won) {
    jumpRequested_ = false;
    return TickResult::None;
  }
  if (!alive_) {
    jumpRequested_ = false;
    if (ownsWorld_)
      advanceWorld(scene, std::clamp(dt, 0.f, 0.1f));
    return TickResult::None;
  }
  using namespace engine::SC;
  const bool left = held_.isKeyPressed(SDL_SCANCODE_A) || held_.isKeyPressed(SDL_SCANCODE_LEFT);
  const bool right = held_.isKeyPressed(SDL_SCANCODE_D) || held_.isKeyPressed(SDL_SCANCODE_RIGHT);
  scene.getRigidBody(robot_)->velocity.x =
      (static_cast<float>(right) - static_cast<float>(left)) * gameSettings::robotSpeed;
  grounded_ = playerIsSupported(scene, robot_, world_.solids);
  jumpIfGrounded(*scene.getRigidBody(robot_), grounded_, jumpRequested_);
  jumpRequested_ = false;
  engine::EntityId contact = 0;
  const auto result = tickPlayer(scene, physics, world_, robot_, grounded_, progress_, dt,
                                  ownsWorld_, &contact);
  if (result == TickResult::DroneHit || result == TickResult::DeathZone) {
    events_.raise(engine::events::collisionEvent{{}, robot_, contact}, tick, -20);
    events_.run(scene, tick);
  }
  return result;
}

std::vector<robotNet::LifeEvent> GameEvents::takeLifeEvents() {
  return std::exchange(outbox_, std::vector<robotNet::LifeEvent>{});
}

} // namespace factory
