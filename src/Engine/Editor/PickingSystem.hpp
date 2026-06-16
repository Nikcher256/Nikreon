#pragma once

#include "Engine/Renderer/Camera/Camera3D.hpp"
#include "Engine/Scene/Scene.hpp"

#include <array>
#include <optional>

#include <glm/vec3.hpp>

namespace Engine {

struct PickHit {
    SceneObjectId objectId{InvalidSceneObjectId};
    float distance{0.0f};
    glm::vec3 worldPosition{0.0f, 0.0f, 0.0f};
};

// Shared editor picking entry point. Today it only knows sprites, but mesh
// bounds, gizmos, lights, and physics raycasts can plug into this same path.
class PickingSystem {
public:
    using SpriteQuadCorners = std::array<glm::vec3, 4>;

    [[nodiscard]] static std::optional<PickHit> pickScene(const Scene& scene, const Ray3D& ray);
    [[nodiscard]] static SpriteQuadCorners spriteQuadCorners(const SceneObject& object);

private:
    [[nodiscard]] static std::optional<PickHit> pickSprite(const SceneObject& object, const Ray3D& ray);
    [[nodiscard]] static std::optional<float> rayTriangleDistance(
        const Ray3D& ray,
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c);
};

} // namespace Engine