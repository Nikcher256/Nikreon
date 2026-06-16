#include "Engine/Scene/SceneSpriteSubmitter.hpp"

#include "Engine/Renderer/Sprite/SpriteRenderer.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"

namespace Engine {

void SceneSpriteSubmitter::submit(const Scene& scene, SpriteRenderer& spriteRenderer, ResourceManager& resources) const
{
    for (const SceneObject& object : scene.objects()) {
        if (!object.spriteRenderer) {
            continue;
        }

        const SpriteRendererComponent& sprite = *object.spriteRenderer;
        TextureHandle texture = sprite.textureHandle;
        if (!texture) {
            texture = sprite.texturePath.empty()
                ? resources.whiteTexture()
                : resources.loadTexture(sprite.texturePath);
        }

        if (!texture) {
            texture = resources.missingTexture();
        }

        spriteRenderer.drawSprite(
            worldTextureId(texture),
            {
                .position = object.transform.position,
                .size = sprite.size,
                .rotationRadians = object.transform.rotationRadians,
                .scale = object.transform.scale,
                .origin = sprite.origin,
                .fixedPlane = sprite.fixedPlane,
                .renderOrder = sprite.renderOrder,
                .layer = sprite.layer,
            },
            {
                .minimum = sprite.uvMin,
                .maximum = sprite.uvMax,
            },
            sprite.tint,
            static_cast<int>(object.id),
            sprite.renderState);
    }
}

} // namespace Engine
