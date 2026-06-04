#include "Engine/Renderer/Renderer2DWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/trigonometric.hpp>

namespace Engine {

namespace {

constexpr std::size_t VerticesPerQuad = 4;
constexpr std::size_t IndicesPerQuad = 6;
constexpr WorldTextureId WhiteTexture = 0;

[[nodiscard]] glm::vec2 rotatedOffset(const glm::vec2& offset, const float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine,
    };
}

} // namespace


WorldSpriteUV WorldSpriteAnimation::frameUV(const float elapsedSeconds) const
{
    const std::uint32_t columns = std::max(frameGrid.x, 1U);
    const std::uint32_t rows = std::max(frameGrid.y, 1U);
    const std::uint32_t availableFrames = std::max(std::min(frameCount, columns * rows), 1U);
    const float safeFps = std::max(framesPerSecond, 0.0001f);
    const std::uint32_t frame = static_cast<std::uint32_t>(std::floor(std::max(elapsedSeconds, 0.0f) * safeFps)) % availableFrames;
    const std::uint32_t column = frame % columns;
    const std::uint32_t row = frame / columns;
    const glm::vec2 cellSize{1.0f / static_cast<float>(columns), 1.0f / static_cast<float>(rows)};

    return {
        {static_cast<float>(column) * cellSize.x, static_cast<float>(row) * cellSize.y},
        {static_cast<float>(column + 1U) * cellSize.x, static_cast<float>(row + 1U) * cellSize.y},
    };
}

Renderer2DWorld::Renderer2DWorld(const std::size_t maxQuadsPerBatch, const std::size_t maxTextureSlots)
    : m_maxQuadsPerBatch(std::max(maxQuadsPerBatch, std::size_t{1}))
    , m_maxTextureSlots(std::max(maxTextureSlots, std::size_t{1}))
{
    m_pendingQuads.reserve(m_maxQuadsPerBatch);
    m_vertices.reserve(m_maxQuadsPerBatch * VerticesPerQuad);
    m_indices.reserve(m_maxQuadsPerBatch * IndicesPerQuad);
}

std::string_view Renderer2DWorld::name() const
{
    return "Renderer2DWorld";
}

RenderStage Renderer2DWorld::stage() const
{
    return RenderStage::World2D;
}

void Renderer2DWorld::beginFrame(const RenderFrameContext& context)
{
    Renderer2DWorldCamera frameCamera = m_camera;
    frameCamera.viewportSize = {
        std::max(context.editorViewport.size.x, 1.0f),
        std::max(context.editorViewport.size.y, 1.0f),
    };
    begin(frameCamera);
}

void Renderer2DWorld::resize(const glm::uvec2& swapchainSize)
{
    m_lastResize = swapchainSize;
}

void Renderer2DWorld::recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context)
{
    (void)context;
    if (m_recording) {
        end();
    }

    recorder.beginStage(stage());
    recorder.endStage(stage());
}

void Renderer2DWorld::endFrame()
{
    if (m_recording) {
        end();
    }
}

void Renderer2DWorld::releaseResources()
{
    m_pendingQuads.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_debugLines.clear();
    m_recording = false;
}

void Renderer2DWorld::begin(const Renderer2DWorldCamera& camera)
{
    m_camera = camera;
    m_pendingQuads.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_debugLines.clear();
    m_nextSequence = 0;
    m_recording = true;
}

void Renderer2DWorld::drawSprite(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const int entityId)
{
    queueQuad(texture, transform, uv, tint, {1.0f, 1.0f}, entityId);
}

void Renderer2DWorld::drawParallaxSprite(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const glm::vec2& parallaxFactor,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const int entityId)
{
    queueQuad(texture, transform, uv, tint, parallaxFactor, entityId);
}

void Renderer2DWorld::drawAnimatedSprite(
    const WorldSpriteAnimation& animation,
    const float elapsedSeconds,
    const WorldSpriteTransform& transform,
    const glm::vec4& tint,
    const int entityId)
{
    drawSprite(animation.texture, transform, animation.frameUV(elapsedSeconds), tint, entityId);
}

void Renderer2DWorld::drawTilemap(const WorldTilemap& tilemap, const WorldSpriteTransform& transform)
{
    if (tilemap.columns == 0U || tilemap.rows == 0U || tilemap.tileSize.x <= 0.0f || tilemap.tileSize.y <= 0.0f) {
        return;
    }

    const std::size_t expectedTiles = static_cast<std::size_t>(tilemap.columns) * static_cast<std::size_t>(tilemap.rows);
    const std::size_t count = std::min(expectedTiles, tilemap.tiles.size());

    for (std::size_t index = 0; index < count; ++index) {
        const WorldTile& tile = tilemap.tiles[index];
        if (!tile.solid) {
            continue;
        }

        const std::uint32_t column = static_cast<std::uint32_t>(index % tilemap.columns);
        const std::uint32_t row = static_cast<std::uint32_t>(index / tilemap.columns);
        WorldSpriteTransform tileTransform = transform;
        tileTransform.position.x += static_cast<float>(column) * tilemap.tileSize.x;
        tileTransform.position.y += static_cast<float>(row) * tilemap.tileSize.y;
        tileTransform.size = tilemap.tileSize;
        tileTransform.origin = {0.0f, 0.0f};
        drawSprite(tile.texture, tileTransform, tile.uv, tile.tint, tile.entityId);
    }
}

void Renderer2DWorld::drawParticles(const std::span<const WorldParticle> particles)
{
    for (const WorldParticle& particle : particles) {
        drawSprite(particle.texture, particle.transform, particle.uv, particle.tint, particle.entityId);
    }
}

void Renderer2DWorld::drawDebugLine(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec4& color,
    const float thickness)
{
    m_debugLines.push_back({start, end, color, std::max(thickness, 1.0f)});
}

void Renderer2DWorld::drawDebugRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, const float thickness)
{
    const glm::vec3 topLeft{position, 0.0f};
    const glm::vec3 topRight{position.x + size.x, position.y, 0.0f};
    const glm::vec3 bottomRight{position.x + size.x, position.y + size.y, 0.0f};
    const glm::vec3 bottomLeft{position.x, position.y + size.y, 0.0f};
    drawDebugLine(topLeft, topRight, color, thickness);
    drawDebugLine(topRight, bottomRight, color, thickness);
    drawDebugLine(bottomRight, bottomLeft, color, thickness);
    drawDebugLine(bottomLeft, topLeft, color, thickness);
}

void Renderer2DWorld::drawDebugFilledRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
    drawSprite(WhiteTexture, {.position = {position, 0.0f}, .size = size, .origin = {0.0f, 0.0f}}, {}, color);
}

void Renderer2DWorld::drawDebugCircle(
    const glm::vec2& center,
    const float radius,
    const glm::vec4& color,
    const float thickness,
    const std::uint32_t segments)
{
    const std::uint32_t safeSegments = std::max(segments, 3U);
    const float step = 6.28318530718f / static_cast<float>(safeSegments);
    for (std::uint32_t index = 0; index < safeSegments; ++index) {
        const float a0 = static_cast<float>(index) * step;
        const float a1 = static_cast<float>(index + 1U) * step;
        drawDebugLine(
            {center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius, 0.0f},
            {center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, 0.0f},
            color,
            thickness);
    }
}

void Renderer2DWorld::end()
{
    rebuildBatches();
    m_recording = false;
}

const Renderer2DWorldCamera& Renderer2DWorld::camera() const
{
    return m_camera;
}

std::span<const WorldQuadVertex> Renderer2DWorld::vertices() const
{
    return m_vertices;
}

std::span<const std::uint32_t> Renderer2DWorld::indices() const
{
    return m_indices;
}

std::span<const WorldDrawBatch> Renderer2DWorld::batches() const
{
    return m_batches;
}

std::span<const WorldDebugLine> Renderer2DWorld::debugLines() const
{
    return m_debugLines;
}

Renderer2DWorldStats Renderer2DWorld::stats() const
{
    return {
        m_pendingQuads.size(),
        m_vertices.size() / VerticesPerQuad,
        m_vertices.size(),
        m_indices.size(),
        m_batches.size(),
        m_debugLines.size(),
    };
}

void Renderer2DWorld::queueQuad(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const glm::vec2& parallaxFactor,
    const int entityId)
{
    if (!m_recording || transform.size.x <= 0.0f || transform.size.y <= 0.0f || tint.a <= 0.0f) {
        return;
    }

    m_pendingQuads.push_back({
        texture,
        transform,
        uv,
        tint,
        parallaxFactor,
        entityId,
        m_nextSequence++,
    });
}

void Renderer2DWorld::rebuildBatches()
{
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();

    std::stable_sort(m_pendingQuads.begin(), m_pendingQuads.end(), [](const PendingQuad& left, const PendingQuad& right) {
        if (left.transform.layer != right.transform.layer) {
            return left.transform.layer < right.transform.layer;
        }
        if (left.transform.position.z != right.transform.position.z) {
            return left.transform.position.z < right.transform.position.z;
        }
        if (left.texture != right.texture) {
            return left.texture < right.texture;
        }
        return left.sequence < right.sequence;
    });

    for (const PendingQuad& quad : m_pendingQuads) {
        if (m_batches.empty() || !currentBatchCanFit(m_batches.back(), quad.texture)) {
            m_batches.push_back({m_vertices.size() / VerticesPerQuad, 0, {}});
        }

        WorldDrawBatch& batch = m_batches.back();
        const float textureIndex = resolveTextureSlot(batch, quad.texture);
        appendQuadVertices(quad, textureIndex);
        ++batch.quadCount;
    }
}

void Renderer2DWorld::appendQuadVertices(const PendingQuad& quad, const float textureIndex)
{
    const std::uint32_t firstVertex = static_cast<std::uint32_t>(m_vertices.size());
    const glm::vec2 scaledCameraOffset = m_camera.position * (glm::vec2{1.0f, 1.0f} - quad.parallaxFactor);
    const glm::vec2 basePosition{quad.transform.position.x + scaledCameraOffset.x, quad.transform.position.y + scaledCameraOffset.y};
    const glm::vec2 originOffset = quad.transform.size * quad.transform.origin;
    const glm::vec2 localCorners[4] = {
        {-originOffset.x, -originOffset.y},
        {quad.transform.size.x - originOffset.x, -originOffset.y},
        {quad.transform.size.x - originOffset.x, quad.transform.size.y - originOffset.y},
        {-originOffset.x, quad.transform.size.y - originOffset.y},
    };
    const glm::vec2 uvs[4] = {
        {quad.uv.minimum.x, quad.uv.minimum.y},
        {quad.uv.maximum.x, quad.uv.minimum.y},
        {quad.uv.maximum.x, quad.uv.maximum.y},
        {quad.uv.minimum.x, quad.uv.maximum.y},
    };

    for (std::size_t index = 0; index < 4; ++index) {
        const glm::vec2 worldPosition = basePosition + rotatedOffset(localCorners[index], quad.transform.rotationRadians);
        m_vertices.push_back({
            {worldPosition.x, worldPosition.y, quad.transform.position.z},
            quad.tint,
            uvs[index],
            textureIndex,
            quad.entityId,
        });
    }

    m_indices.push_back(firstVertex + 0U);
    m_indices.push_back(firstVertex + 1U);
    m_indices.push_back(firstVertex + 2U);
    m_indices.push_back(firstVertex + 2U);
    m_indices.push_back(firstVertex + 3U);
    m_indices.push_back(firstVertex + 0U);
}

float Renderer2DWorld::resolveTextureSlot(WorldDrawBatch& batch, const WorldTextureId texture) const
{
    const auto found = std::find(batch.textures.begin(), batch.textures.end(), texture);
    if (found != batch.textures.end()) {
        return static_cast<float>(std::distance(batch.textures.begin(), found));
    }

    batch.textures.push_back(texture);
    return static_cast<float>(batch.textures.size() - 1U);
}

bool Renderer2DWorld::currentBatchCanFit(const WorldDrawBatch& batch, const WorldTextureId texture) const
{
    if (batch.quadCount >= m_maxQuadsPerBatch) {
        return false;
    }

    return std::find(batch.textures.begin(), batch.textures.end(), texture) != batch.textures.end() ||
        batch.textures.size() < m_maxTextureSlots;
}

} // namespace Engine
