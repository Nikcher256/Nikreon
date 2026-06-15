#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"

namespace Engine {

using SceneObjectId = std::uint32_t;
inline constexpr SceneObjectId InvalidSceneObjectId = 0U;

struct TransformComponent {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationRadians{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

struct Sprite2DComponent {
    std::string texturePath;
    TextureHandle textureHandle{};
    glm::vec2 size{32.0f, 32.0f};
    glm::vec2 origin{0.5f, 0.5f};
    glm::vec2 uvMin{0.0f, 0.0f};
    glm::vec2 uvMax{1.0f, 1.0f};
    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    int layer{0};
    WorldSpriteRenderState renderState{};
};

struct SceneObject {
    SceneObjectId id{InvalidSceneObjectId};
    std::string name;
    TransformComponent transform{};
    std::optional<Sprite2DComponent> sprite2D{};

    [[nodiscard]] bool hasSprite2D() const noexcept
    {
        return sprite2D.has_value();
    }
};

class Scene {
public:
    SceneObject& createObject(std::string name);
    SceneObject& createSprite2D(std::string name, std::string texturePath);
    bool destroyObject(SceneObjectId id);
    void clear();

    [[nodiscard]] std::span<SceneObject> objects();
    [[nodiscard]] std::span<const SceneObject> objects() const;

    [[nodiscard]] SceneObject* findObject(SceneObjectId id);
    [[nodiscard]] const SceneObject* findObject(SceneObjectId id) const;

    [[nodiscard]] std::size_t objectCount() const noexcept;
    [[nodiscard]] std::size_t sprite2DCount() const noexcept;

private:
    SceneObjectId m_nextObjectId{1U};
    std::vector<SceneObject> m_objects;
};

} //namespace Engine
