#include <robot_factory_escape/game_sprites.hpp>

#include <algorithm>
#include <numeric>

namespace {
engine::SpriteSheetLayout croppedGrid(int cellSize, int columns, int frameCount,
                                      glm::vec2 cropOrigin, glm::vec2 cropSize) {
    engine::SpriteSheetLayout layout;
    layout.frames.reserve(static_cast<std::size_t>(frameCount));
    for (int index = 0; index < frameCount; ++index) {
        const glm::vec2 cell{static_cast<float>(index % columns),
                             static_cast<float>(index / columns)};
        layout.frames.push_back({cell * static_cast<float>(cellSize) + cropOrigin, cropSize});
    }
    return layout;
}

engine::SpriteAnimation loadClip(engine::Renderer& renderer, const std::string& path,
                                 int cellSize, int columns, int frameCount,
                                 glm::vec2 cropOrigin, glm::vec2 cropSize,
                                 float frameSeconds, bool loop = true) {
    const auto texture = renderer.loadTexture(path);
    const auto sheet = renderer.createSpriteSheet(
        texture, croppedGrid(cellSize, columns, frameCount, cropOrigin, cropSize));
    std::vector<std::uint32_t> indices(frameCount);
    std::iota(indices.begin(), indices.end(), 0u);
    return engine::SpriteAnimation::uniform(sheet, indices, frameSeconds, loop);
}
} // namespace

void GameSprites::load(engine::Renderer& renderer, const std::string& directory) {
    const auto robot = directory + "robot_sprite/cyberpunk_character_cyborg_";
    // Each family uses a tighter crop because its pose occupies a different
    // portion of the 512px source cell.
    constexpr glm::vec2 idleCropOrigin{180.f, 96.f};
    constexpr glm::vec2 idleCropSize{164.f, 416.f};
    constexpr glm::vec2 walkCropOrigin{156.f, 96.f};
    constexpr glm::vec2 walkCropSize{208.f, 416.f};
    constexpr glm::vec2 jumpCropOrigin{176.f, 64.f};
    constexpr glm::vec2 jumpCropSize{248.f, 448.f};
    robotClips_[IdleRight] = loadClip(renderer, robot + "idleright.png", 512, 8, 56,
                                      idleCropOrigin, idleCropSize, 0.05f);
    robotClips_[IdleLeft] = loadClip(renderer, robot + "idleleft.png", 512, 8, 56,
                                     idleCropOrigin, idleCropSize, 0.05f);
    robotClips_[WalkRight] = loadClip(renderer, robot + "right.png", 512, 6, 33,
                                      walkCropOrigin, walkCropSize, 0.025f);
    robotClips_[WalkLeft] = loadClip(renderer, robot + "left.png", 512, 6, 33,
                                     walkCropOrigin, walkCropSize, 0.025f);
    robotClips_[JumpRight] = loadClip(renderer, robot + "jumpright.png", 512, 9, 45,
                                      jumpCropOrigin, jumpCropSize, 0.022f, false);
    robotClips_[JumpLeft] = loadClip(renderer, robot + "jumpleft.png", 512, 9, 45,
                                     jumpCropOrigin, jumpCropSize, 0.022f, false);
    droneClip_ = loadClip(renderer, directory + "drone/cyberpunk_enemy_drone_move.png",
                          256, 8, 63, {0.f, 48.f}, {256.f, 208.f}, 0.035f);
}

void GameSprites::reset(engine::Scene& scene, engine::EntityId robot, engine::EntityId drone) {
    facingLeft_ = false;
    current_ = IdleRight;
    scene.addSpriteAnimation(robot, robotClips_[current_]);
    scene.addSpriteAnimation(drone, droneClip_);
}

void GameSprites::update(engine::Scene& scene, engine::EntityId robot, bool grounded,
                         bool won, float dt) {
    if (won) return; // Freeze artwork along with gameplay at the exit.
    const float velocityX = scene.getRigidBody(robot)->velocity.x;
    if (velocityX != 0.f) facingLeft_ = velocityX < 0.f;
    const Clip desired = !grounded ? (facingLeft_ ? JumpLeft : JumpRight)
                         : velocityX != 0.f ? (facingLeft_ ? WalkLeft : WalkRight)
                                           : (facingLeft_ ? IdleLeft : IdleRight);
    if (desired != current_) {
        current_ = desired;
        scene.addSpriteAnimation(robot, robotClips_[current_]);
    }
    engine::advanceAnimations(scene, std::clamp(dt, 0.f, 0.1f));
}
