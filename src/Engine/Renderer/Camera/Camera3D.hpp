#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

struct Ray3D {
    glm::vec3 origin{0.0f, 0.0f, 0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

enum class Camera3DProjection {
    Perspective,
    Orthographic,
};

class Camera3D {
public:
    glm::vec3 position{0.0f, -6.0f, 4.0f};
    float yawRadians{0.0f};
    float pitchRadians{-0.55f};
    float verticalFovRadians{1.0471975512f};
    float orthographicHeight{10.0f};
    float aspectRatio{16.0f / 9.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};
    bool infiniteFarPlane{false};
    Camera3DProjection projectionMode{Camera3DProjection::Perspective};

    [[nodiscard]] glm::mat4 projection() const;
    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 viewProjection() const;

    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up() const;

    [[nodiscard]] Ray3D screenPointToRay(const glm::vec2& screenPosition, const glm::vec2& viewportSize) const;
};

} // namespace Engine
