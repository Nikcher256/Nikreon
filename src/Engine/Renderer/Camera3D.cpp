#include "Engine/Renderer/Camera3D.hpp"

#include <algorithm>
#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_relational.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace Engine {

namespace {

constexpr glm::vec3 WorldUp{0.0f, 0.0f, 1.0f};

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length = glm::length(value);
    if (length <= 0.0001f) {
        return fallback;
    }

    return value / length;
}

} //namespace

glm::mat4 Camera3D::projection() const
{
    const float safeAspect = std::max(aspectRatio, 0.001f);
    const float safeNear = std::max(nearPlane, 0.001f);
    const float safeFar = std::max(farPlane, safeNear + 0.001f);

    if (projectionMode == Camera3DProjection::Orthographic) {
        const float halfHeight = std::max(orthographicHeight, 0.001f) * 0.5f;
        const float halfWidth = halfHeight * safeAspect;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, safeNear, safeFar);
    }

    const float safeFov = std::clamp(verticalFovRadians, 0.05f, glm::pi<float>() - 0.05f);
    return glm::perspective(safeFov, safeAspect, safeNear, safeFar);
}

glm::mat4 Camera3D::view() const
{
    return glm::lookAt(position, position + forward(), up());
}

glm::mat4 Camera3D::viewProjection() const
{
    return projection() * view();
}

glm::vec3 Camera3D::forward() const
{
    const float cosPitch = std::cos(pitchRadians);
    return safeNormalize(
        {
            std::sin(yawRadians) * cosPitch,
            std::cos(yawRadians) * cosPitch,
            std::sin(pitchRadians),
        },
        {0.0f, 1.0f, 0.0f});
}

glm::vec3 Camera3D::right() const
{
    return safeNormalize(glm::cross(forward(), WorldUp), {1.0f, 0.0f, 0.0f});
}

glm::vec3 Camera3D::up() const
{
    return safeNormalize(glm::cross(right(), forward()), WorldUp);
}

Ray3D Camera3D::screenPointToRay(const glm::vec2& screenPosition, const glm::vec2& viewportSize) const
{
    const glm::vec2 safeViewport{
        std::max(viewportSize.x, 1.0f),
        std::max(viewportSize.y, 1.0f),
    };

    const float x = (2.0f * screenPosition.x) / safeViewport.x - 1.0f;
    const float y = 1.0f - (2.0f * screenPosition.y) / safeViewport.y;

    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection());
    const glm::vec4 nearPoint = inverseViewProjection * glm::vec4{x, y, -1.0f, 1.0f};
    const glm::vec4 farPoint = inverseViewProjection * glm::vec4{x, y, 1.0f, 1.0f};

    const glm::vec3 nearWorld = glm::vec3{nearPoint} / std::max(nearPoint.w, 0.0001f);
    const glm::vec3 farWorld = glm::vec3{farPoint} / std::max(farPoint.w, 0.0001f);

    if (projectionMode == Camera3DProjection::Orthographic) {
        return {nearWorld, forward()};
    }

    return {position, safeNormalize(farWorld - nearWorld, forward())};
}

} //namespace Engine