#include "Engine/Scene/Scene2DSubmitter.hpp"

#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"

namespace Engine {

void Scene2DSubmitter::submit(const Scene& scene, Renderer2DWorld& renderer2DWorld, ResourceManager& resources) const
{
    for (const SceneObject& object : scene.objects()) {
        if (!object.sprite2D) {
            continue;
        }

        const Sprite2DComponent& sprite = *object.sprite2D;
        TextureHandle texture = sprite.texturePath.empty()
            ? resources.whiteTexture()
            : resources.loadTexture(sprite.texturePath);

        if (!texture) {
            texture = resources.missingTexture();
        }

        renderer2DWorld.drawSprite(
            worldTextureId(texture),
            {
                .position = object.transform.position,
                .size = sprite.size * glm::vec2{object.transform.scale.x, object.transform.scale.y},
                .rotationRadians = object.transform.rotationRadians.z,
                .origin = sprite.origin,
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