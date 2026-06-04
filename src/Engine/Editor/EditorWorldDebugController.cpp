#include "Engine/Editor/EditorWorldDebugController.hpp"

#include "Engine/Renderer/Renderer2DWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace Engine {
EditorWorldDebugController::EditorWorldDebugController(std::string defaultSpritePath)
    : m_defaultSpritePath(std::move(defaultSpritePath))
{
}

void EditorWorldDebugController::update(const float deltaTime)
{
    m_elapsedSeconds += std::max(deltaTime, 0.0f);
}

void EditorWorldDebugController::setSpritePath(std::string path)
{
    m_spritePath = std::move(path);
    m_spriteLoaded = !m_spritePath.empty();
}

std::string_view EditorWorldDebugController::spritePath() const
{
    return m_spritePath.empty() ? std::string_view{m_defaultSpritePath} : std::string_view{m_spritePath};
}

void EditorWorldDebugController::addSprite()
{
    ensureSpritePath();
    ++m_spriteCount;
}

void EditorWorldDebugController::addManySprites()
{
    ensureSpritePath();
    m_spriteCount += 96;
}

void EditorWorldDebugController::toggleTilemap()
{
    m_tilemapEnabled = !m_tilemapEnabled;
}

void EditorWorldDebugController::toggleAnimatedSprite()
{
    m_animatedSpriteEnabled = !m_animatedSpriteEnabled;
}

void EditorWorldDebugController::toggleParallax()
{
    m_parallaxEnabled = !m_parallaxEnabled;
}

void EditorWorldDebugController::toggleParticles()
{
    m_particlesEnabled = !m_particlesEnabled;
}

void EditorWorldDebugController::toggleDebugShapes()
{
    m_debugShapesEnabled = !m_debugShapesEnabled;
}

void EditorWorldDebugController::clear()
{
    m_spriteCount = 0;
    m_tilemapEnabled = false;
    m_animatedSpriteEnabled = false;
    m_parallaxEnabled = false;
    m_particlesEnabled = false;
    m_debugShapesEnabled = false;
}

void EditorWorldDebugController::submit(Renderer2DWorld& renderer2DWorld, const Camera2D& camera) const
{
    const std::string path{spritePath()};
    WorldTextureId spriteTexture = static_cast<WorldTextureId>(std::hash<std::string>{}(path));
    if (spriteTexture == 0) {
        spriteTexture = 1;
    }

    Camera2D safeCamera = camera;
    safeCamera.viewportSize = glm::max(safeCamera.viewportSize, glm::vec2{1.0f, 1.0f});
    renderer2DWorld.begin(safeCamera);

    const std::size_t spriteCount = std::min(m_spriteCount, std::size_t{512});
    for (std::size_t index = 0; index < spriteCount; ++index) {
        const float column = static_cast<float>(index % 16U);
        const float row = static_cast<float>(index / 16U);
        const glm::vec4 tint{
            0.25f + 0.12f * static_cast<float>(index % 5U),
            0.58f + 0.08f * static_cast<float>(index % 3U),
            0.90f - 0.08f * static_cast<float>(index % 4U),
            1.0f,
        };

        renderer2DWorld.drawSprite(
            spriteTexture,
            {
                .position = {-220.0f + column * 28.0f, 120.0f - row * 28.0f, static_cast<float>(index % 6U)},
                .size = {22.0f, 22.0f},
                .rotationRadians = (m_animatedSpriteEnabled ? m_elapsedSeconds * 0.6f : 0.0f) + static_cast<float>(index % 4U) * 0.08f,
                .layer = static_cast<int>(index % 3U),
            },
            {},
            tint,
            static_cast<int>(index));
    }

    if (m_tilemapEnabled) {
        WorldTilemap tilemap;
        tilemap.columns = 8;
        tilemap.rows = 5;
        tilemap.tileSize = {24.0f, 24.0f};
        tilemap.tiles.reserve(static_cast<std::size_t>(tilemap.columns) * tilemap.rows);

        for (std::uint32_t index = 0; index < tilemap.columns * tilemap.rows; ++index) {
            const bool gap = index % 7U == 0U;
            tilemap.tiles.push_back({
                .texture = spriteTexture + 10U,
                .tint = gap ? glm::vec4{0.0f, 0.0f, 0.0f, 0.0f} : glm::vec4{0.26f, 0.72f, 0.42f, 1.0f},
                .entityId = static_cast<int>(1000U + index),
                .solid = !gap,
            });
        }

        renderer2DWorld.drawTilemap(tilemap, {.position = {-260.0f, -80.0f, -3.0f}, .origin = {0.0f, 0.0f}, .layer = -2});
    }

    if (m_animatedSpriteEnabled) {
        renderer2DWorld.drawAnimatedSprite(
            {.texture = spriteTexture + 20U, .frameGrid = {4U, 2U}, .frameCount = 8U, .framesPerSecond = 8.0f},
            m_elapsedSeconds,
            {.position = {0.0f, 0.0f, 8.0f}, .size = {54.0f, 54.0f}, .rotationRadians = std::sin(m_elapsedSeconds) * 0.35f, .layer = 8},
            {1.0f, 0.78f, 0.28f, 1.0f},
            2000);
    }

    if (m_parallaxEnabled) {
        renderer2DWorld.drawParallaxSprite(
            spriteTexture + 30U,
            {.position = {-150.0f + std::sin(m_elapsedSeconds * 0.7f) * 40.0f, -150.0f, -20.0f}, .size = {180.0f, 42.0f}, .layer = -10},
            {0.35f, 0.2f},
            {},
            {0.24f, 0.34f, 0.70f, 0.85f},
            3000);
    }

    if (m_particlesEnabled) {
        std::array<WorldParticle, 24> particles{};
        for (std::size_t index = 0; index < particles.size(); ++index) {
            const float angle = m_elapsedSeconds * 1.8f + static_cast<float>(index) * 0.45f;
            const float radius = 42.0f + static_cast<float>(index % 5U) * 8.0f;
            particles[index] = {
                .texture = spriteTexture + 40U,
                .transform = {
                    .position = {std::cos(angle) * radius + 160.0f, std::sin(angle) * radius - 10.0f, 5.0f},
                    .size = {8.0f, 8.0f},
                    .layer = 6,
                },
                .tint = {1.0f, 0.38f + 0.02f * static_cast<float>(index), 0.24f, 0.85f},
                .entityId = static_cast<int>(4000U + index),
            };
        }

        renderer2DWorld.drawParticles(particles);
    }

    if (m_debugShapesEnabled) {
        renderer2DWorld.drawDebugRect({-280.0f, -140.0f}, {220.0f, 120.0f}, {0.2f, 0.85f, 1.0f, 1.0f}, 2.0f);
        renderer2DWorld.drawDebugCircle({110.0f, 96.0f}, 42.0f, {1.0f, 0.95f, 0.25f, 1.0f}, 2.0f, 28U);
        renderer2DWorld.drawDebugLine({-32.0f, -32.0f, 0.0f}, {84.0f, 46.0f, 0.0f}, {1.0f, 0.22f, 0.35f, 1.0f}, 2.0f);
    }

    renderer2DWorld.end();
}

bool EditorWorldDebugController::spriteLoaded() const
{
    return m_spriteLoaded;
}

std::size_t EditorWorldDebugController::spriteCount() const
{
    return m_spriteCount;
}

bool EditorWorldDebugController::tilemapEnabled() const
{
    return m_tilemapEnabled;
}

bool EditorWorldDebugController::animatedSpriteEnabled() const
{
    return m_animatedSpriteEnabled;
}

bool EditorWorldDebugController::parallaxEnabled() const
{
    return m_parallaxEnabled;
}

bool EditorWorldDebugController::particlesEnabled() const
{
    return m_particlesEnabled;
}

bool EditorWorldDebugController::debugShapesEnabled() const
{
    return m_debugShapesEnabled;
}

void EditorWorldDebugController::ensureSpritePath()
{
    if (m_spritePath.empty()) {
        m_spritePath = m_defaultSpritePath;
    }

    m_spriteLoaded = true;
}

} // namespace Engine