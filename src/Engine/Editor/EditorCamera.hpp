#pragma once

#include "Engine/Renderer/Camera/Camera3D.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

struct EditorCameraInput {
    glm::vec2 panDelta{0.0f, 0.0f};
    glm::vec2 lookDelta{0.0f, 0.0f};
    glm::vec2 orbitDelta{0.0f, 0.0f};
    glm::vec2 trackDelta{0.0f, 0.0f};
    float zoomDelta{0.0f};
    float dollyDelta{0.0f};
    float moveRight{0.0f};
    float moveUp{0.0f};
    float moveForward{0.0f};
    float speedScale{1.0f};
    bool focusRequested{false};
    bool perspective3D{false};
    glm::vec3 focusPosition{0.0f, 0.0f, 0.0f};
    float focusRadius{64.0f};
};

class EditorCamera {
public:
    void update(float deltaTime, const EditorCameraInput& input);

    void setMovementSpeed(float speed);
    [[nodiscard]] Camera3D camera3D(float aspectRatio) const;
    [[nodiscard]] float movementSpeed() const;
    [[nodiscard]] const glm::vec3& position() const;
    [[nodiscard]] float zoom() const;
    [[nodiscard]] float yawRadians() const;
    [[nodiscard]] float pitchRadians() const;

private:
    glm::vec3 m_position{0.0f, 0.0f, 5.0f};
    glm::vec3 m_perspectivePosition{0.0f, -217.0f, 148.0f};
    glm::vec3 m_perspectivePivot{0.0f, 0.0f, 5.0f};
    float m_perspectiveOrbitDistance{260.0f};
    float m_zoom{1.0f};
    float m_movementSpeed{5.0f};
    float m_panSensitivity{1.0f};
    float m_zoomStep{0.12f};
    float m_yawRadians{0.0f};
    float m_pitchRadians{-0.58f};
    float m_lookSensitivity{0.006f};
};
    
} //namespace Engine
