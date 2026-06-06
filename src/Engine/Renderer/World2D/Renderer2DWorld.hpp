#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Renderer/Camera/Camera3D.hpp"
#include "Engine/Renderer/Core/RenderModuleBase.hpp"

namespace Engine {

using WorldTextureId = std::uintptr_t;

enum class WorldSpritePipeline {
    Sprite, // normal world sprites
    Debug,
};

enum class WorldBlendMode {
    Opaque, // for solid sprites, no transparency blending
    Alpha, // normal sprite transparency
    Additive, // glow/fire/light/particles style blending
};

enum class WorldSamplerMode {
    Nearest, // pixel-art sharp edges
    Linear, // smooth texture filtering
};

struct WorldSpriteRenderState {
    WorldSpritePipeline pipeline{WorldSpritePipeline::Sprite};
    WorldBlendMode blendMode{WorldBlendMode::Alpha};
    WorldSamplerMode samplerMode{WorldSamplerMode::Linear};
};

struct WorldBatchKey {
    WorldSpritePipeline pipeline{WorldSpritePipeline::Sprite};
    WorldBlendMode blendMode{WorldBlendMode::Alpha};
    WorldSamplerMode samplerMode{WorldSamplerMode::Linear};
    int layer{0};

    [[nodiscard]] friend bool operator==(const WorldBatchKey& left, const WorldBatchKey& right)
    {
        return left.pipeline == right.pipeline &&
            left.blendMode == right.blendMode &&
            left.samplerMode == right.samplerMode &&
            left.layer == right.layer;
    }
};

struct WorldSpriteTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec2 size{1.0f, 1.0f};
    float rotationRadians{0.0f};
    glm::vec2 origin{0.5f, 0.5f};
    int layer{0};
};

struct WorldSpriteUV {
    glm::vec2 minimum{0.0f, 0.0f};
    glm::vec2 maximum{1.0f, 1.0f};
};

enum class Renderer2DWorldCameraMode {
    Orthographic2D,
    Perspective3D,
};

struct Renderer2DWorldCamera {
    Renderer2DWorldCameraMode mode{Renderer2DWorldCameraMode::Orthographic2D};
    Camera2D camera2D{};
    Camera3D camera3D{};
};

struct WorldQuadVertex {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 uv{0.0f, 0.0f};
    float textureIndex{0.0f};
    int entityId{-1};
};

struct WorldDrawBatch {
    WorldBatchKey key{};
    std::size_t firstQuad{0};
    std::size_t quadCount{0};
    std::vector<WorldTextureId> textures;
};

struct WorldTile {
    WorldTextureId texture{0};
    WorldSpriteUV uv{};
    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    WorldSpriteRenderState renderState{};
    int entityId{-1};
    bool solid{true};
};

struct WorldTilemap {
    std::uint32_t columns{0};
    std::uint32_t rows{0};
    glm::vec2 tileSize{1.0f, 1.0f};
    std::vector<WorldTile> tiles;
};

struct WorldParticle {
    WorldTextureId texture{0};
    WorldSpriteTransform transform{};
    WorldSpriteUV uv{};
    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    WorldSpriteRenderState renderState{};
    int entityId{-1};
};

struct WorldSpriteAnimation {
    WorldTextureId texture{0};
    glm::uvec2 frameGrid{1U, 1U};
    std::uint32_t frameCount{1U};
    float framesPerSecond{12.0f};
    WorldSpriteRenderState renderState{};

    [[nodiscard]] WorldSpriteUV frameUV(float elapsedSeconds) const;
};

struct WorldDebugLine {
    glm::vec3 start{0.0f, 0.0f, 0.0f};
    glm::vec3 end{0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float thickness{1.0f};
};

struct Renderer2DWorldStats {
    std::size_t spriteCount{0};
    std::size_t quadCount{0};
    std::size_t vertexCount{0};
    std::size_t indexCount{0};
    std::size_t drawBatchCount{0};
    std::size_t debugLineCount{0};
    std::size_t textureSlotFlushCount{0};
    std::size_t maxTextureSlots{0};
    std::size_t maxQuadsPerBatch{0};
};

class Renderer2DWorld final : public RenderModule {
public:
    explicit Renderer2DWorld(std::size_t maxQuadsPerBatch = 4096, std::size_t maxTextureSlots = 16);

    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] RenderStage stage() const override;
    void beginFrame(const RenderFrameContext& context) override;
    void resize(const glm::uvec2& swapchainSize) override;
    void recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context) override;
    void endFrame() override;
    void releaseResources() override;

    void begin(const Renderer2DWorldCamera& camera);
    void drawSprite(
        WorldTextureId texture,
        const WorldSpriteTransform& transform,
        const WorldSpriteUV& uv = {},
        const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f},
        int entityId = -1,
        const WorldSpriteRenderState& renderState = {});
    void drawParallaxSprite(
        WorldTextureId texture,
        const WorldSpriteTransform& transform,
        const glm::vec2& parallaxFactor,
        const WorldSpriteUV& uv = {},
        const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f},
        int entityId = -1,
        const WorldSpriteRenderState& renderState = {});
    void drawAnimatedSprite(
        const WorldSpriteAnimation& animation,
        float elapsedSeconds,
        const WorldSpriteTransform& transform,
        const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f},
        int entityId = -1);
    void drawTilemap(const WorldTilemap& tilemap, const WorldSpriteTransform& transform);
    void drawParticles(std::span<const WorldParticle> particles);
    void drawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness = 1.0f);
    void drawDebugRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float thickness = 1.0f);
    void drawDebugFilledRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
    void drawDebugCircle(const glm::vec2& center, float radius, const glm::vec4& color, float thickness = 1.0f, std::uint32_t segments = 24U);
    void end();

    [[nodiscard]] const Renderer2DWorldCamera& camera() const;
    [[nodiscard]] std::span<const WorldQuadVertex> vertices() const;
    [[nodiscard]] std::span<const std::uint32_t> indices() const;
    [[nodiscard]] std::span<const WorldDrawBatch> batches() const;
    [[nodiscard]] std::span<const WorldDebugLine> debugLines() const;
    [[nodiscard]] Renderer2DWorldStats stats() const;

private:
    struct PendingQuad {
        WorldTextureId texture{0};
        WorldSpriteTransform transform{};
        WorldSpriteUV uv{};
        glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec2 parallaxFactor{1.0f, 1.0f};
        WorldSpriteRenderState renderState{};
        int entityId{-1};
        std::uint64_t sequence{0};
    };

    void queueQuad(
        WorldTextureId texture,
        const WorldSpriteTransform& transform,
        const WorldSpriteUV& uv,
        const glm::vec4& tint,
        const glm::vec2& parallaxFactor,
        int entityId,
        const WorldSpriteRenderState& renderState);
    void rebuildBatches();
    void appendQuadVertices(const PendingQuad& quad, float textureIndex);
    [[nodiscard]] WorldBatchKey batchKeyFor(const PendingQuad& quad) const;
    [[nodiscard]] float resolveTextureSlot(WorldDrawBatch& batch, WorldTextureId texture) const;
    [[nodiscard]] bool currentBatchCanFit(const WorldDrawBatch& batch, const PendingQuad& quad) const;

    Renderer2DWorldCamera m_camera{};
    glm::uvec2 m_lastResize{0U, 0U};
    std::size_t m_maxQuadsPerBatch{0};
    std::size_t m_maxTextureSlots{0};
    std::vector<PendingQuad> m_pendingQuads;
    std::vector<WorldQuadVertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    std::vector<WorldDrawBatch> m_batches;
    std::vector<WorldDebugLine> m_debugLines;
    std::size_t m_textureSlotFlushCount{0};
    std::uint64_t m_nextSequence{0};
    bool m_recording{false};
};

} // namespace Engine