#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace Engine {

class Camera2D {
public:
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 viewportSize{1280.0f, 720.0f};
    float rotationRadians{0.0f};
    float zoom{1.0f};
    float nearPlane{-1000.f};
    float farPlane{1000.0f};
    bool pixelSnapping{false};

    [[nodiscard]] glm::mat4 projection() const;
    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 viewProjection() const;

    [[nodiscard]] glm::vec2 screenToWorld(const glm::vec2& screenPosition) const;
    [[nodiscard]] glm::vec2 worldToScreen(const glm::vec2& worldPosition) const;
};

} // namespace Engine