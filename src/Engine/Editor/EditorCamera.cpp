#include "Engine/Editor/EditorCamera.hpp"

#include <algorithm>
#include <cmath>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace Engine {

void EditorCamera::update(const float deltaTime, const EditorCameraInput& input)
{
    const float safeDeltaTime = std::max(deltaTime, 0.0f);

    if (input.perspective3D) {
        m_yawRadians -= -input.lookDelta.x * m_lookSensitivity;
        m_pitchRadians -= input.lookDelta.y * m_lookSensitivity;
        m_pitchRadians = std::clamp(m_pitchRadians, -1.5f, 1.5f);

        const float sinYaw = std::sin(m_yawRadians);
        const float cosYaw = std::cos(m_yawRadians);
        const float cosPitch = std::cos(m_pitchRadians);

        const glm::vec3 forwardGround{sinYaw, cosYaw, 0.0f};
        const glm::vec3 rightGround{cosYaw, -sinYaw, 0.0f};
        const glm::vec3 up{0.0f, 0.0f, 1.0f};

        const glm::vec3 cameraForward{
            std::sin(m_yawRadians) * cosPitch,
            std::cos(m_yawRadians) * cosPitch,
            std::sin(m_pitchRadians),
        };
        const glm::vec3 cameraRight = rightGround;
        const glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraForward));

        m_position +=
            rightGround * input.moveRight * m_movementSpeed * safeDeltaTime +
            forwardGround * input.moveForward * m_movementSpeed * safeDeltaTime +
            up * input.moveUp * m_movementSpeed * safeDeltaTime;

        const float safeZoom = std::max(m_zoom, 0.001f);
        m_position -= cameraRight * input.panDelta.x * m_panSensitivity / safeZoom;
        m_position += cameraUp * input.panDelta.y * m_panSensitivity / safeZoom;

        if (input.zoomDelta != 0.0f) {
            const float zoomScale = std::pow(1.0f + m_zoomStep, input.zoomDelta);
            m_zoom = std::clamp(m_zoom * zoomScale, 0.15f, 12.0f);
        }

        return;
    }

    m_position += glm::vec3{
        input.moveRight,
        input.moveUp,
        -input.moveForward,
    } * m_movementSpeed *safeDeltaTime;

    const float safeZoom = std::max(m_zoom, 0.001f);
    m_position.x -= input.panDelta.x * m_panSensitivity /safeZoom;
    m_position.y += input.panDelta.y * m_panSensitivity / safeZoom;

    if (input.zoomDelta != 0.0f) {
        const float zoomScale = std::pow(1.0f + m_zoomStep, input.zoomDelta);
        m_zoom = std::clamp(m_zoom * zoomScale, 0.1f, 12.0f);
    }
}

void EditorCamera::setMovementSpeed(const float speed)
{
    m_movementSpeed = std::max(speed, 0.0f);
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

} //namespace Engine