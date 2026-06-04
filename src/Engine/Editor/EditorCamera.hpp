#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

struct EditorCameraInput {
    glm::vec2 panDelta{0.0f, 0.0f};
    float zoomDelta{0.0f};
    float moveRight{0.0f};
    float moveUp{0.0f};
    float moveForward{0.0f};
};

class EditorCamera {
public:
    void update(float deltaTime, const EditorCameraInput& input);

    void setMovementSpeed(float speed);
    [[nodiscard]] float movementSpeed() const;
    [[nodiscard]] const glm::vec3& position() const;
    [[nodiscard]] float zoom() const;

private:
    glm::vec3 m_position{0.0f, 0.0f, 5.0f};
    float m_zoom{1.0f};
    float m_movementSpeed{5.0f};
    float m_panSensitivity{1.0f};
    float m_zoomStep{0.12f};
};
    
} //namespace Engine