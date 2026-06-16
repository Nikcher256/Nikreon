#include "Engine/Renderer/Sprite/SpriteRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

namespace Engine {

namespace {

constexpr std::size_t VerticesPerQuad = 4;
constexpr std::size_t IndicesPerQuad = 6;
constexpr WorldTextureId WhiteTexture = 0;

[[nodiscard]] glm::vec3 localSpriteCorner(const WorldSpriteFixedPlane fixedPlane, const glm::vec2& corner)
{
    switch (fixedPlane) {
    case WorldSpriteFixedPlane::FixedXY:
        return {corner.x, corner.y, 0.0f};
    case WorldSpriteFixedPlane::FixedXZ:
        return {corner.x, 0.0f, corner.y};
    case WorldSpriteFixedPlane::FixedYZ:
        return {0.0f, corner.x, corner.y};
    }

    return {corner.x, corner.y, 0.0f};
}

[[nodiscard]] glm::vec3 spritePlaneOffset(const WorldSpriteFixedPlane fixedPlane, const glm::vec2& offset)
{
    return localSpriteCorner(fixedPlane, offset);
}

[[nodiscard]] bool isTransparentBlend(const WorldBlendMode blendMode)
{
    return blendMode == WorldBlendMode::Alpha || blendMode == WorldBlendMode::Additive;
}

[[nodiscard]] glm::mat4 spriteTransformMatrix(const WorldSpriteTransform& transform)
{
    glm::mat4 matrix{1.0f};
    matrix = glm::translate(matrix, transform.position);
    matrix = glm::rotate(matrix, transform.rotationRadians.x, {1.0f, 0.0f, 0.0f});
    matrix = glm::rotate(matrix, transform.rotationRadians.y, {0.0f, 1.0f, 0.0f});
    matrix = glm::rotate(matrix, transform.rotationRadians.z, {0.0f, 0.0f, 1.0f});
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

[[nodiscard]] glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point)
{
    const glm::vec4 transformed = matrix * glm::vec4{point, 1.0f};
    return glm::vec3{transformed} / std::max(std::abs(transformed.w), 0.0001f);
}

[[nodiscard]] float viewDepthFor(const WorldRenderView& view, const WorldSpriteTransform& transform)
{
    const WorldSpriteQuadCorners corners = buildWorldSpriteQuadCorners(transform);
    const glm::vec3 center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
    const glm::vec4 viewPosition = view.view * glm::vec4{center, 1.0f};
    const float depth = -viewPosition.z;
    return std::isfinite(depth) ? depth : 0.0f;
}

} // namespace

WorldSpriteQuadCorners buildWorldSpriteQuadCorners(const WorldSpriteTransform& transform)
{
    const glm::vec2 originOffset = transform.size * transform.origin;
    const glm::vec2 localCorners[4] = {
        {-originOffset.x, -originOffset.y},
        {transform.size.x - originOffset.x, -originOffset.y},
        {transform.size.x - originOffset.x, transform.size.y - originOffset.y},
        {-originOffset.x, transform.size.y - originOffset.y},
    };

    const glm::mat4 matrix = spriteTransformMatrix(transform);
    return {
        transformPoint(matrix, localSpriteCorner(transform.fixedPlane, localCorners[0])),
        transformPoint(matrix, localSpriteCorner(transform.fixedPlane, localCorners[1])),
        transformPoint(matrix, localSpriteCorner(transform.fixedPlane, localCorners[2])),
        transformPoint(matrix, localSpriteCorner(transform.fixedPlane, localCorners[3])),
    };
}

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

SpriteRenderer::SpriteRenderer(const std::size_t maxQuadsPerBatch, const std::size_t maxTextureSlots)
    : m_maxQuadsPerBatch(std::max(maxQuadsPerBatch, std::size_t{1}))
    , m_maxTextureSlots(std::max(maxTextureSlots, std::size_t{1}))
{
    m_pendingQuads.reserve(m_maxQuadsPerBatch);
    m_vertices.reserve(m_maxQuadsPerBatch * VerticesPerQuad);
    m_indices.reserve(m_maxQuadsPerBatch * IndicesPerQuad);
}

std::string_view SpriteRenderer::name() const
{
    return "SpriteRenderer";
}

RenderStage SpriteRenderer::stage() const
{
    return RenderStage::Sprites;
}

void SpriteRenderer::beginFrame(const RenderFrameContext& context)
{
    SpriteRendererCamera frameCamera = m_camera;
    const glm::vec2 viewportSize = {
        std::max(context.editorViewport.size.x, 1.0f),
        std::max(context.editorViewport.size.y, 1.0f),
    };

    frameCamera.camera2D.viewportSize = viewportSize;
    frameCamera.camera3D.aspectRatio = viewportSize.x / viewportSize.y;
    if (frameCamera.mode == SpriteRendererCameraMode::Perspective3D) {
        frameCamera.renderView = worldRenderViewFromCamera3D(frameCamera.camera3D, viewportSize);
    } else {
        frameCamera.renderView = worldRenderViewFromCamera2D(frameCamera.camera2D);
    }
    frameCamera.hasRenderView = true;

    begin(frameCamera);
}

void SpriteRenderer::resize(const glm::uvec2& swapchainSize)
{
    m_lastResize = swapchainSize;
}

void SpriteRenderer::recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context)
{
    (void)context;
    if (m_recording) {
        end();
    }

    recorder.beginStage(stage());
    recorder.endStage(stage());
}

void SpriteRenderer::endFrame()
{
    if (m_recording) {
        end();
    }
}

void SpriteRenderer::releaseResources()
{
    m_pendingQuads.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_debugLines.clear();
    m_textureSlotFlushCount = 0;
    m_recording = false;
}

void SpriteRenderer::begin(const SpriteRendererCamera& camera)
{
    m_camera = camera;
    if (!m_camera.hasRenderView) {
        m_camera.renderView = m_camera.mode == SpriteRendererCameraMode::Perspective3D
            ? worldRenderViewFromCamera3D(m_camera.camera3D, m_camera.camera2D.viewportSize)
            : worldRenderViewFromCamera2D(m_camera.camera2D);
        m_camera.hasRenderView = true;
    }
    m_pendingQuads.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_debugLines.clear();
    m_textureSlotFlushCount = 0;
    m_nextSequence = 0;
    m_recording = true;
}

void SpriteRenderer::begin(const WorldRenderView& view)
{
    SpriteRendererCamera camera;
    camera.mode = view.projectionMode == WorldRenderProjection::Perspective
        ? SpriteRendererCameraMode::Perspective3D
        : SpriteRendererCameraMode::Orthographic2D;
    camera.camera2D.viewportSize = view.viewportSize;
    camera.renderView = view;
    camera.hasRenderView = true;
    begin(camera);
}

void SpriteRenderer::drawSprite(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const int entityId,
    const WorldSpriteRenderState& renderState)
{
    queueQuad(texture, transform, uv, tint, {1.0f, 1.0f}, entityId, renderState);
}

void SpriteRenderer::drawParallaxSprite(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const glm::vec2& parallaxFactor,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const int entityId,
    const WorldSpriteRenderState& renderState)
{
    queueQuad(texture, transform, uv, tint, parallaxFactor, entityId, renderState);
}

void SpriteRenderer::drawAnimatedSprite(
    const WorldSpriteAnimation& animation,
    const float elapsedSeconds,
    const WorldSpriteTransform& transform,
    const glm::vec4& tint,
    const int entityId)
{
    drawSprite(animation.texture, transform, animation.frameUV(elapsedSeconds), tint, entityId, animation.renderState);
}

void SpriteRenderer::drawTilemap(const WorldTilemap& tilemap, const WorldSpriteTransform& transform)
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
        drawSprite(tile.texture, tileTransform, tile.uv, tile.tint, tile.entityId, tile.renderState);
    }
}

void SpriteRenderer::drawParticles(const std::span<const WorldParticle> particles)
{
    for (const WorldParticle& particle : particles) {
        drawSprite(particle.texture, particle.transform, particle.uv, particle.tint, particle.entityId, particle.renderState);
    }
}

void SpriteRenderer::drawDebugLine(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec4& color,
    const float thickness)
{
    const float safeThickness = std::max(thickness, 1.0f);
    m_debugLines.push_back({start, end, color, safeThickness});

    const glm::vec2 delta{end.x - start.x, end.y - start.y};
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.0001f) {
        return;
    }

    drawSprite(
        WhiteTexture,
        {
            .position = start,
            .size = {length, safeThickness},
            .rotationRadians = {0.0f, 0.0f, std::atan2(delta.y, delta.x)},
            .origin = {0.0f, 0.5f},
        },
        {},
        color,
        -1,
        {
            .pipeline = WorldSpritePipeline::Debug,
            .blendMode = WorldBlendMode::Alpha,
            .samplerMode = WorldSamplerMode::Nearest,
        });
}

void SpriteRenderer::drawDebugRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, const float thickness)
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

void SpriteRenderer::drawDebugFilledRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
    drawSprite(WhiteTexture, {.position = {position, 0.0f}, .size = size, .origin = {0.0f, 0.0f}}, {}, color);
}

void SpriteRenderer::drawDebugCircle(
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

void SpriteRenderer::end()
{
    rebuildBatches();
    m_recording = false;
}

const SpriteRendererCamera& SpriteRenderer::camera() const
{
    return m_camera;
}

const WorldRenderView& SpriteRenderer::renderView() const
{
    return m_camera.renderView;
}

std::span<const WorldQuadVertex> SpriteRenderer::vertices() const
{
    return m_vertices;
}

std::span<const std::uint32_t> SpriteRenderer::indices() const
{
    return m_indices;
}

std::span<const WorldDrawBatch> SpriteRenderer::batches() const
{
    return m_batches;
}

std::span<const WorldDebugLine> SpriteRenderer::debugLines() const
{
    return m_debugLines;
}

SpriteRendererStats SpriteRenderer::stats() const
{
    return {
        m_pendingQuads.size(),
        m_vertices.size() / VerticesPerQuad,
        m_vertices.size(),
        m_indices.size(),
        m_batches.size(),
        m_debugLines.size(),
        m_textureSlotFlushCount,
        m_maxTextureSlots,
        m_maxQuadsPerBatch,
    };
}

void SpriteRenderer::queueQuad(
    const WorldTextureId texture,
    const WorldSpriteTransform& transform,
    const WorldSpriteUV& uv,
    const glm::vec4& tint,
    const glm::vec2& parallaxFactor,
    const int entityId,
    const WorldSpriteRenderState& renderState)
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
        renderState,
        entityId,
        m_nextSequence++,
    });
}

void SpriteRenderer::rebuildBatches()
{
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_textureSlotFlushCount = 0;

    const auto adjustedTransformFor = [this](const PendingQuad& quad) {
        WorldSpriteTransform adjustedTransform = quad.transform;
        const glm::vec2 scaledCameraOffset = m_camera.camera2D.position * (glm::vec2{1.0f, 1.0f} - quad.parallaxFactor);
        adjustedTransform.position += spritePlaneOffset(adjustedTransform.fixedPlane, scaledCameraOffset);
        return adjustedTransform;
    };

    const auto viewDepth = [this, &adjustedTransformFor](const PendingQuad& quad) {
        return viewDepthFor(m_camera.renderView, adjustedTransformFor(quad));
    };

    // Sprite ordering is view-aware instead of being a separate 2D z model.
    // Opaque sprites submit before translucent/additive sprites and use normal
    // depth test/write in the viewport world target. Translucent/additive
    // sprites depth-test without writing depth and are sorted back-to-front
    // from the active WorldRenderView. layer remains only as a legacy 2D
    // compatibility tie-break; renderOrder is the future manual ordering knob.
    std::stable_sort(m_pendingQuads.begin(), m_pendingQuads.end(), [&viewDepth](const PendingQuad& left, const PendingQuad& right) {
        const bool leftTransparent = isTransparentBlend(left.renderState.blendMode);
        const bool rightTransparent = isTransparentBlend(right.renderState.blendMode);
        if (leftTransparent != rightTransparent) {
            return !leftTransparent;
        }

        const float leftDepth = viewDepth(left);
        const float rightDepth = viewDepth(right);
        if (leftDepth != rightDepth) {
            return leftTransparent
                ? leftDepth > rightDepth
                : leftDepth < rightDepth;
        }

        if (left.transform.layer != right.transform.layer) {
            return left.transform.layer < right.transform.layer;
        }

        if (left.transform.renderOrder != right.transform.renderOrder) {
            return left.transform.renderOrder < right.transform.renderOrder;
        }

        return left.sequence < right.sequence;
    });

    for (const PendingQuad& quad : m_pendingQuads) {
        const bool hadBatch = !m_batches.empty();
        const bool canFit = hadBatch && currentBatchCanFit(m_batches.back(), quad);
        if (!canFit) {
            if (hadBatch &&
                m_batches.back().key == batchKeyFor(quad) &&
                std::find(m_batches.back().textures.begin(), m_batches.back().textures.end(), quad.texture) == m_batches.back().textures.end() &&
                m_batches.back().textures.size() >= m_maxTextureSlots) {
                ++m_textureSlotFlushCount;
            }

            m_batches.push_back({batchKeyFor(quad), m_vertices.size() / VerticesPerQuad, 0, {}});
        }

        WorldDrawBatch& batch = m_batches.back();
        const float textureIndex = resolveTextureSlot(batch, quad.texture);
        appendQuadVertices(quad, textureIndex);
        ++batch.quadCount;
    }
}

void SpriteRenderer::appendQuadVertices(const PendingQuad& quad, const float textureIndex)
{
    const std::uint32_t firstVertex = static_cast<std::uint32_t>(m_vertices.size());
    const glm::vec2 scaledCameraOffset = m_camera.camera2D.position * (glm::vec2{1.0f, 1.0f} - quad.parallaxFactor);
    WorldSpriteTransform adjustedTransform = quad.transform;
    adjustedTransform.position += spritePlaneOffset(adjustedTransform.fixedPlane, scaledCameraOffset);
    const WorldSpriteQuadCorners corners = buildWorldSpriteQuadCorners(adjustedTransform);
    const bool flipSpriteV = m_camera.mode == SpriteRendererCameraMode::Perspective3D;
    const glm::vec2 uvs[4] = {
        {quad.uv.minimum.x, flipSpriteV ? quad.uv.maximum.y : quad.uv.minimum.y},
        {quad.uv.maximum.x, flipSpriteV ? quad.uv.maximum.y : quad.uv.minimum.y},
        {quad.uv.maximum.x, flipSpriteV ? quad.uv.minimum.y : quad.uv.maximum.y},
        {quad.uv.minimum.x, flipSpriteV ? quad.uv.minimum.y : quad.uv.maximum.y},
    };

    for (std::size_t index = 0; index < 4; ++index) {
        m_vertices.push_back({
            corners[index],
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

WorldBatchKey SpriteRenderer::batchKeyFor(const PendingQuad& quad) const
{
    return {
        quad.renderState.pipeline,
        quad.renderState.blendMode,
        quad.renderState.samplerMode,
        quad.transform.layer,
    };
}

float SpriteRenderer::resolveTextureSlot(WorldDrawBatch& batch, const WorldTextureId texture) const
{
    const auto found = std::find(batch.textures.begin(), batch.textures.end(), texture);
    if (found != batch.textures.end()) {
        return static_cast<float>(std::distance(batch.textures.begin(), found));
    }

    batch.textures.push_back(texture);
    return static_cast<float>(batch.textures.size() - 1U);
}

bool SpriteRenderer::currentBatchCanFit(const WorldDrawBatch& batch, const PendingQuad& quad) const
{
    if (batch.key != batchKeyFor(quad)) {
        return false;
    }

    if (batch.quadCount >= m_maxQuadsPerBatch) {
        return false;
    }

    return std::find(batch.textures.begin(), batch.textures.end(), quad.texture) != batch.textures.end() ||
        batch.textures.size() < m_maxTextureSlots;
}

} // namespace Engine
