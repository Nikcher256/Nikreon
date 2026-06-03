#include "Engine/Renderer/Renderer2DWorld.hpp"

#include <cassert>
#include <vector>

using namespace Engine;

int main()
{
    Renderer2DWorld renderer(4, 2);
    renderer.begin({.position = {10.0f, 20.0f}, .viewportSize = {320.0f, 180.0f}, .zoom = 2.0f});

    for (int index = 0; index < 6; ++index) {
        renderer.drawSprite(
            static_cast<WorldTextureId>(1 + index % 2),
            {
                .position = {static_cast<float>(index) * 8.0f, 0.0f, static_cast<float>(index % 3)},
                .size = {4.0f, 6.0f},
                .layer = index % 2,
            },
            {},
            {1.0f, 0.5f, 0.25f, 1.0f},
            index);
    }

    renderer.end();
    assert(renderer.stats().quadCount == 6);
    assert(renderer.stats().vertexCount == 24);
    assert(renderer.stats().indexCount == 36);
    assert(renderer.stats().drawBatchCount == 2);
    assert(renderer.batches()[0].quadCount == 4);
    assert(renderer.batches()[1].quadCount == 2);
    assert(renderer.vertices()[0].entityId == 0);
    assert(renderer.vertices()[0].textureIndex == 0.0f);

    WorldTilemap tilemap;
    tilemap.columns = 2;
    tilemap.rows = 2;
    tilemap.tileSize = {16.0f, 16.0f};
    tilemap.tiles = {
        {.texture = 5, .entityId = 100},
        {.texture = 5, .entityId = 101},
        {.texture = 6, .entityId = 102, .solid = false},
        {.texture = 6, .entityId = 103},
    };

    renderer.begin({});
    renderer.drawTilemap(tilemap, {.position = {32.0f, 48.0f, 0.0f}, .origin = {0.0f, 0.0f}});

    const WorldSpriteAnimation animation{.texture = 9, .frameGrid = {4U, 2U}, .frameCount = 8U, .framesPerSecond = 4.0f};
    renderer.drawAnimatedSprite(animation, 0.75f, {.position = {0.0f, 0.0f, 2.0f}, .size = {8.0f, 8.0f}}, {1.0f, 1.0f, 1.0f, 0.75f}, 400);

    const std::vector<WorldParticle> particles{
        {.texture = 11, .transform = {.position = {2.0f, 3.0f, 1.0f}, .size = {2.0f, 2.0f}}, .entityId = 500},
        {.texture = 11, .transform = {.position = {4.0f, 6.0f, 1.0f}, .size = {2.0f, 2.0f}}, .entityId = 501},
    };
    renderer.drawParticles(particles);
    renderer.drawParallaxSprite(
        12,
        {.position = {100.0f, 100.0f, -10.0f}, .size = {32.0f, 32.0f}},
        {0.5f, 0.25f},
        {},
        {0.8f, 0.9f, 1.0f, 1.0f},
        600);
    renderer.drawDebugLine({0.0f, 0.0f, 0.0f}, {4.0f, 4.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, 2.0f);
    renderer.drawDebugRect({0.0f, 0.0f}, {8.0f, 8.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
    renderer.drawDebugFilledRect({0.0f, 0.0f}, {3.0f, 5.0f}, {0.0f, 0.0f, 1.0f, 0.5f});
    renderer.drawDebugCircle({0.0f, 0.0f}, 2.0f, {1.0f, 1.0f, 0.0f, 1.0f}, 1.0f, 8U);
    renderer.end();

    assert(renderer.stats().quadCount == 8);
    assert(renderer.stats().debugLineCount == 13);
    assert(renderer.vertices()[0].entityId == 600);

    bool foundAnimatedSprite = false;
    for (const WorldQuadVertex& vertex : renderer.vertices()) {
        if (vertex.entityId == 400) {
            foundAnimatedSprite = true;
            assert(vertex.uv.x >= 0.75f || vertex.uv.x <= 0.25f);
        }
    }
    assert(foundAnimatedSprite);

    return 0;
}
