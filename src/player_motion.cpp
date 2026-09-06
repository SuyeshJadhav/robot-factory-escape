#include <robot_factory_escape/player_motion.hpp>
#include <robot_factory_escape/game_settings.hpp>

#include <algorithm>
#include <cmath>

bool jumpIfGrounded(engine::RigidBody& body, bool& grounded, bool newPress) {
    if (!newPress || !grounded) {
        return false;
    }
    body.velocity.y = -gameSettings::jumpSpeed;
    grounded = false;
    return true;
}

void movePlayer(engine::Scene& scene, engine::PhysicsSystem& physics,
                engine::EntityId player, std::span<const engine::EntityId> solids,
                float deltaSeconds, bool& grounded) {
    auto& position = scene.transform(player).position;
    auto& velocity = scene.getRigidBody(player)->velocity;
    const auto size = scene.getCollider(player)->size;

    // Limit catch-up after a pause; short steps keep normal gameplay movement
    // smaller than the obstacles. This is not a general swept collision solver.
    float remaining = std::clamp(deltaSeconds, 0.f, 0.1f);
    while (remaining > 0.f) {
        const float step = std::min(remaining, 1.f / 120.f);
        remaining -= step;
        const auto previous = position;
        physics.step(scene, step); // The robot is the scene's only rigid body.
        const float nextY = position.y;

        // Resolve X at the old height, then resolve Y at the corrected X.
        position.y = previous.y;
        for (const auto solid : solids) {
            if (!physics.isCollision(scene, player, solid)) {
                continue;
            }
            const auto overlap = physics.GetCollisionOverlap(scene, player, solid);
            if (overlap.size.x <= 0.f || overlap.size.y <= 0.f) {
                continue; // Touching an edge is not penetration.
            }
            const auto solidPosition = scene.transform(solid).position;
            const auto solidSize = scene.getCollider(solid)->size;
            if (velocity.x > 0.f) {
                position.x = solidPosition.x - size.x;
            } else if (velocity.x < 0.f) {
                position.x = solidPosition.x + solidSize.x;
            }
            velocity.x = 0.f;
        }

        position.y = nextY;
        grounded = false;
        for (const auto solid : solids) {
            if (!physics.isCollision(scene, player, solid)) {
                continue;
            }
            const auto overlap = physics.GetCollisionOverlap(scene, player, solid);
            if (overlap.size.x <= 0.f || overlap.size.y <= 0.f) {
                continue;
            }
            const auto solidPosition = scene.transform(solid).position;
            const auto solidSize = scene.getCollider(solid)->size;
            if (velocity.y > 0.f) {
                position.y = solidPosition.y - size.y;
                grounded = true;
            } else if (velocity.y < 0.f) {
                position.y = solidPosition.y + solidSize.y;
            }
            velocity.y = 0.f;
        }
        // At very high frame rates gravity's displacement can round to zero.
        // Exact surface contact still supports the robot even without overlap.
        if (velocity.y >= 0.f) {
            for (const auto solid : solids) {
                const auto solidPosition = scene.transform(solid).position;
                const auto solidSize = scene.getCollider(solid)->size;
                const bool overSurface = position.x < solidPosition.x + solidSize.x &&
                                         position.x + size.x > solidPosition.x;
                if (overSurface && std::abs(position.y + size.y - solidPosition.y) < 0.001f) {
                    position.y = solidPosition.y - size.y;
                    velocity.y = 0.f;
                    grounded = true;
                }
            }
        }
    }
}
