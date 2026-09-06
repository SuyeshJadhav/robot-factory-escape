#include <robot_factory_escape/player_motion.hpp>
#include <robot_factory_escape/game_settings.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
bool near(float actual, float expected) {
    return std::abs(actual - expected) < 0.01f;
}
struct Fixture {
    engine::Scene scene;
    engine::PhysicsSystem physics{gameSettings::gravity};
    engine::EntityId player = box({0.f, 0.f}, {20.f, 20.f});
    bool grounded = false;

    Fixture() { scene.addRigidBody(player); }
    engine::EntityId box(glm::vec2 position, glm::vec2 size) {
        const auto id = scene.createEntity();
        scene.transform(id).position = position;
        scene.addCollider(id, {.size = size});
        return id;
    }
    void step(engine::EntityId solid, float dt = 1.f / 60.f) {
        const std::array solids{solid};
        movePlayer(scene, physics, player, solids, dt, grounded);
    }
    glm::vec2& position() { return scene.transform(player).position; }
    engine::RigidBody& body() { return *scene.getRigidBody(player); }
};
} // namespace

int main() {
    try {
        {
            Fixture f;
            const auto floor = f.box({-100.f, 300.f}, {1000.f, 100.f});
            for (int i = 0; i < 180; ++i) f.step(floor);
            require(near(f.position().y, 280.f) && f.grounded, "Landing or stable grounding failed");
            require(near(f.body().velocity.y, 0.f), "Landing did not stop falling");
            f.step(floor, 0.00001f);
            require(f.grounded, "Tiny frame lost surface contact");
            require(!jumpIfGrounded(f.body(), f.grounded, false), "Held key triggered a jump");
            require(jumpIfGrounded(f.body(), f.grounded, true), "Grounded jump rejected");
            f.step(floor);
            require(f.position().y < 280.f && !f.grounded, "Jump did not leave the floor");
            require(!jumpIfGrounded(f.body(), f.grounded, true), "Midair jump accepted");
        }
        {
            Fixture f;
            const auto wall = f.box({100.f, -100.f}, {40.f, 600.f});
            f.position() = {79.f, 0.f};
            f.body().velocity.x = 300.f;
            f.step(wall);
            require(near(f.position().x, 80.f) && near(f.body().velocity.x, 0.f), "Left wall face failed");
            f.position() = {141.f, 0.f};
            f.body().velocity.x = -300.f;
            f.step(wall);
            require(near(f.position().x, 140.f) && near(f.body().velocity.x, 0.f), "Right wall face failed");
            require(!f.grounded, "Wall contact counted as ground");
        }
        {
            Fixture f;
            const auto platform = f.box({-100.f, 100.f}, {300.f, 40.f});
            f.position() = {0.f, 141.f};
            f.body().velocity.y = -gameSettings::jumpSpeed;
            f.step(platform, 1.f / 120.f);
            require(near(f.position().y, 140.f) && near(f.body().velocity.y, 0.f), "Underside response failed");
            require(!f.grounded, "Underside counted as ground");
            f.position() = {0.f, 79.f};
            f.body().velocity.y = 100.f;
            f.step(platform);
            require(near(f.position().y, 80.f) && f.grounded, "Platform landing failed");
            f.position().x = 199.f;
            f.body().velocity.x = 300.f;
            f.step(platform);
            require(!f.grounded && f.position().y > 80.f, "Walking off retained ground state");
        }
        {
            Fixture f;
            const auto floor = f.box({-100.f, 300.f}, {1000.f, 100.f});
            f.position() = {0.f, 270.f};
            f.body().velocity.y = 1000.f;
            f.step(floor, 2.f);
            require(near(f.position().y, 280.f) && f.grounded, "Slow frame crossed the floor");
        }
        {
            Fixture f;
            const auto floor = f.box({-100.f, 300.f}, {1000.f, 100.f});
            f.position() = {0.f, 280.f};
            f.grounded = true;
            jumpIfGrounded(f.body(), f.grounded, true);
            float peakHeight = 0.f;
            float airtime = 0.f;
            while (!f.grounded && airtime < 2.f) {
                f.step(floor, 1.f / 120.f);
                airtime += 1.f / 120.f;
                peakHeight = std::max(peakHeight, 280.f - f.position().y);
            }
            require(f.grounded && airtime > 0.9f && airtime < 1.05f, "Jump airtime outside tuned range");
            require(peakHeight > 200.f && peakHeight < 225.f, "Jump cannot comfortably clear the crate");
        }
        std::cout << "Movement, collision, and tuned jump height/airtime checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
