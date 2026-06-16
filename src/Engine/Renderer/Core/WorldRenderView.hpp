#pragma once

#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Renderer/Camera/Camera3D.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

enum class WorldRenderProjection {
    Perspective,
    Orthographic,
};

struct WorldRenderView {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::vec3 cameraPosition{0.0f, 0.0f, 0.0f};
    glm::vec2 viewportSize{1.0f, 1.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};
    WorldRenderProjection projectionMode{WorldRenderProjection::Orthographic};
};

[[nodiscard]] inline WorldRenderView worldRenderViewFromCamera3D(
    const Camera3D& camera,
    const glm::vec2& viewportSize)
{
    const glm::mat4 view = camera.view();
    const glm::mat4 projection = camera.projection();
    return {
        view,
        projection,
        projection * view,
        camera.position,
        viewportSize,
        camera.nearPlane,
        camera.farPlane,
        camera.projectionMode == Camera3DProjection::Perspective
            ? WorldRenderProjection::Perspective
            : WorldRenderProjection::Orthographic,
    };
}

[[nodiscard]] inline WorldRenderView worldRenderViewFromCamera2D(const Camera2D& camera)
{
    const glm::mat4 view = camera.view();
    const glm::mat4 projection = camera.projection();
    return {
        view,
        projection,
        projection * view,
        {camera.position.x, camera.position.y, 0.0f},
        camera.viewportSize,
        camera.nearPlane,
        camera.farPlane,
        WorldRenderProjection::Orthographic,
    };
}

} // namespace Engine
