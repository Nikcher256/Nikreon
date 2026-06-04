#include "Engine/Editor/EditorCamera.hpp"

#include <algorithm>
#include <cmath>

namespace Engine {

void EditorCamera::update(const float deltaTime, const EditorCameraInput& input)
{
    const float safeDeltaTime = std::max(deltaTime, 0.0f);
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

} //namespace Engine