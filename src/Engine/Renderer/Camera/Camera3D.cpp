#include "Engine/Renderer/Camera/Camera3D.hpp"

#include <algorithm>
#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_relational.hpp>
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

glm::vec3 forwardFromAngles(const float yawRadians, const float pitchRadians)
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

glm::vec3 rightFromYaw(const float yawRadians)
{
    return safeNormalize(
        {
            std::cos(yawRadians),
            -std::sin(yawRadians),
            0.0f,
        },
        {1.0f, 0.0f, 0.0f});
}

glm::vec3 upFromAngles(const float yawRadians, const float pitchRadians)
{
    const float sinPitch = std::sin(pitchRadians);
    return safeNormalize(
        {
            -std::sin(yawRadians) * sinPitch,
            -std::cos(yawRadians) * sinPitch,
            std::cos(pitchRadians),
        },
        WorldUp);
}

glm::mat4 infinitePerspectiveRH_ZO(const float fovRadians, const float aspectRatio, const float nearPlane)
{
    const float tanHalfFov = std::tan(fovRadians * 0.5f);
    glm::mat4 projection{0.0f};
    projection[0][0] = 1.0f / (aspectRatio * tanHalfFov);
    projection[1][1] = 1.0f / tanHalfFov;
    projection[2][2] = -1.0f;
    projection[2][3] = -1.0f;
    projection[3][2] = -nearPlane;
    return projection;
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
        return glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, safeNear, safeFar);
    }

    const float safeFov = std::clamp(verticalFovRadians, 0.05f, glm::pi<float>() - 0.05f);
    if (infiniteFarPlane) {
        return infinitePerspectiveRH_ZO(safeFov, safeAspect, safeNear);
    }

    return glm::perspectiveRH_ZO(safeFov, safeAspect, safeNear, safeFar);
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
    return forwardFromAngles(yawRadians, pitchRadians);
}

glm::vec3 Camera3D::right() const
{
    return rightFromYaw(yawRadians);
}

glm::vec3 Camera3D::up() const
{
    return upFromAngles(yawRadians, pitchRadians);
}

Ray3D Camera3D::screenPointToRay(const glm::vec2& screenPosition, const glm::vec2& viewportSize) const
{
    const glm::vec2 safeViewport{
        std::max(viewportSize.x, 1.0f),
        std::max(viewportSize.y, 1.0f),
    };

    const float x = (2.0f * screenPosition.x) / safeViewport.x - 1.0f;
    const float y = 1.0f - (2.0f * screenPosition.y) / safeViewport.y;

    if (projectionMode == Camera3DProjection::Perspective) {
        const float safeAspect = std::max(aspectRatio, 0.001f);
        const float safeFov = std::clamp(verticalFovRadians, 0.05f, glm::pi<float>() - 0.05f);
        const float tanHalfFov = std::tan(safeFov * 0.5f);
        const glm::vec3 rayDirection = safeNormalize(
            forward() +
                right() * (x * tanHalfFov * safeAspect) +
                up() * (y * tanHalfFov),
            forward());
        return {position, rayDirection};
    }

    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection());
    const glm::vec4 nearPoint = inverseViewProjection * glm::vec4{x, y, -1.0f, 1.0f};
    const glm::vec4 farPoint = inverseViewProjection * glm::vec4{x, y, 1.0f, 1.0f};

    const glm::vec3 nearWorld = glm::vec3{nearPoint} / std::max(nearPoint.w, 0.0001f);
    const glm::vec3 farWorld = glm::vec3{farPoint} / std::max(farPoint.w, 0.0001f);

    return {nearWorld, forward()};
}

} //namespace Engine
