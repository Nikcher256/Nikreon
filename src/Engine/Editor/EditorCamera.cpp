#include "Engine/Editor/EditorCamera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace Engine {

namespace {

constexpr glm::vec3 WorldUp{0.0f, 0.0f, 1.0f};
constexpr float FreeLookPitchLimit = 1.5f;

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

bool hasDelta(const glm::vec2& value)
{
    return std::abs(value.x) > 0.0001f || std::abs(value.y) > 0.0001f;
}

float wrapRadians(const float value)
{
    return std::remainder(value, glm::pi<float>() * 2.0f);
}

} // namespace

void EditorCamera::update(const float deltaTime, const EditorCameraInput& input)
{
    const float safeDeltaTime = std::max(deltaTime, 0.0f);
    const float safeFocusRadius = std::max(input.focusRadius, 1.0f);

    if (input.focusRequested) {
        if (input.perspective3D) {
            m_perspectivePivot = input.focusPosition;
            m_perspectiveOrbitDistance = std::clamp(safeFocusRadius * 3.0f, 80.0f, 2000.0f);
            const glm::vec3 forward = forwardFromAngles(m_yawRadians, m_pitchRadians);
            m_perspectivePosition = m_perspectivePivot - forward * m_perspectiveOrbitDistance;
        } else {
            m_position = input.focusPosition;
            m_zoom = std::clamp(160.0f / safeFocusRadius, 0.1f, 12.0f);
        }
    }

    if (input.perspective3D) {
        const bool orbiting = hasDelta(input.orbitDelta);
        const bool looking = hasDelta(input.lookDelta);

        if (orbiting) {
            m_yawRadians = wrapRadians(m_yawRadians - input.orbitDelta.x * m_lookSensitivity);
            m_pitchRadians = wrapRadians(m_pitchRadians - input.orbitDelta.y * m_lookSensitivity);
        } else if (looking) {
            m_yawRadians = wrapRadians(m_yawRadians + input.lookDelta.x * m_lookSensitivity);
            m_pitchRadians = std::clamp(
                m_pitchRadians - input.lookDelta.y * m_lookSensitivity,
                -FreeLookPitchLimit,
                FreeLookPitchLimit);
        }

        const glm::vec3 forward = forwardFromAngles(m_yawRadians, m_pitchRadians);
        const glm::vec3 right = rightFromYaw(m_yawRadians);
        const glm::vec3 up = upFromAngles(m_yawRadians, m_pitchRadians);

        if (orbiting) {
            m_perspectivePosition = m_perspectivePivot - forward * m_perspectiveOrbitDistance;
        }

        if (looking) {
            m_perspectivePivot = m_perspectivePosition + forward * m_perspectiveOrbitDistance;
        }

        const float safeSpeedScale = std::max(input.speedScale, 0.1f);
        const float flySpeed = std::max(m_movementSpeed, m_perspectiveOrbitDistance) * safeSpeedScale;
        const glm::vec3 flyDelta =
            right * input.moveRight +
            forward * input.moveForward +
            WorldUp * input.moveUp;

        if (glm::length(flyDelta) > 0.0001f) {
            const glm::vec3 movement = safeNormalize(flyDelta, {}) * flySpeed * safeDeltaTime;
            m_perspectivePosition += movement;
            m_perspectivePivot += movement;
        }

        if (hasDelta(input.trackDelta)) {
            const float trackScale = std::max(m_perspectiveOrbitDistance * 0.0025f, 0.05f);
            const glm::vec3 movement =
                -right * input.trackDelta.x * trackScale +
                up * input.trackDelta.y * trackScale;
            m_perspectivePosition += movement;
            m_perspectivePivot += movement;
        }

        if (input.dollyDelta != 0.0f) {
            const float scale = std::pow(1.01f, input.dollyDelta);
            m_perspectiveOrbitDistance = std::clamp(m_perspectiveOrbitDistance * scale, 5.0f, 5000.0f);
            m_perspectivePosition = m_perspectivePivot - forward * m_perspectiveOrbitDistance;
        }

        if (input.zoomDelta != 0.0f) {
            const float moveDistance = input.zoomDelta * std::max(m_perspectiveOrbitDistance * 0.12f, 5.0f);
            const glm::vec3 movement = forward * moveDistance;
            m_perspectivePosition += movement;
            m_perspectivePivot += movement;
        }

        m_perspectiveOrbitDistance = std::max(glm::length(m_perspectivePivot - m_perspectivePosition), 5.0f);
        return;
    }

    const glm::vec3 orthographicRight = safeNormalize(input.orthographicRightAxis, {1.0f, 0.0f, 0.0f});
    const glm::vec3 orthographicUp = safeNormalize(input.orthographicUpAxis, {0.0f, 1.0f, 0.0f});

    m_position +=
        orthographicRight * input.moveRight * m_movementSpeed * safeDeltaTime +
        orthographicUp * input.moveUp * m_movementSpeed * safeDeltaTime;

    const float safeZoom = std::max(m_zoom, 0.001f);
    m_position -= orthographicRight * input.panDelta.x * m_panSensitivity / safeZoom;
    m_position += orthographicUp * input.panDelta.y * m_panSensitivity / safeZoom;

    if (input.zoomDelta != 0.0f) {
        const float zoomScale = std::pow(1.0f + m_zoomStep, input.zoomDelta);
        m_zoom = std::clamp(m_zoom * zoomScale, 0.1f, 12.0f);
    }
}

void EditorCamera::setMovementSpeed(const float speed)
{
    m_movementSpeed = std::max(speed, 0.0f);
}

Camera3D EditorCamera::camera3D(const float aspectRatio) const
{
    Camera3D camera;
    camera.position = m_perspectivePosition;
    camera.yawRadians = m_yawRadians;
    camera.pitchRadians = m_pitchRadians;
    camera.aspectRatio = std::max(aspectRatio, 0.001f);
    camera.verticalFovRadians = 0.55f;
    camera.nearPlane = 0.1f;
    camera.farPlane = std::max(2000.0f, m_perspectiveOrbitDistance * 4.0f);
    camera.projectionMode = Camera3DProjection::Perspective;
    return camera;
}

float EditorCamera::movementSpeed() const
{
    return m_movementSpeed;
}

const glm::vec3& EditorCamera::position() const
{
    return m_position;
}

float EditorCamera::zoom() const
{
    return m_zoom;
}

float EditorCamera::yawRadians() const
{
    return m_yawRadians;
}

float EditorCamera::pitchRadians() const
{
    return m_pitchRadians;
}

} // namespace Engine
