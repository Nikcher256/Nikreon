#include "Engine/Renderer/Camera2D.hpp"

#include <algorithm>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Engine {

namespace {

glm::vec2 rotate(const glm::vec2& value, const float radians)
{
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);

    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine,
    };
}

} // namespace

glm::mat4 Camera2D::projection() const
{
    const float safeZoom = std::max(zoom, 0.001f);
    const float halfWidth = viewportSize.x * 0.5f / safeZoom;
    const float halfHeight = viewportSize.y * 0.5f / safeZoom;
    return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

glm::mat4 Camera2D::view() const
{
    const glm::vec2 snappedPosition = pixelSnapping
    ? glm::vec2{std::round(position.x), std::round(position.y)}
    : position;

    glm::mat4 viewMatrix{1.0f};
    viewMatrix = glm::rotate(viewMatrix, - rotationRadians, {0.0f, 0.0f, 1.0f});
    viewMatrix = glm::translate(viewMatrix, {-snappedPosition.x, -snappedPosition.y, 0.0f});
    return viewMatrix;
}

glm::mat4 Camera2D::viewProjection() const
{
    return projection() * view();
}

glm::vec2 Camera2D::screenToWorld(const glm::vec2& screenPosition) const
{
    const float safeZoom = std::max(zoom, 0.001f);
    const glm::vec2 viewportCenter = viewportSize * 0.5f;
    const glm::vec2 centeredScreen{
        screenPosition.x - viewportCenter.x,
        screenPosition.y - viewportCenter.y,
    };

    return position + centeredScreen / safeZoom;
}

glm::vec2 Camera2D::worldToScreen(const glm::vec2& worldPosition) const 
{
    const float safeZoom = std::max(zoom, 0.001f);
    const glm::vec2 viewportCenter = viewportSize * 0.5f;
    const glm::vec2 cameraRelative = (worldPosition - position) * safeZoom;
    
    return{
        viewportCenter.x + cameraRelative.x,
        viewportCenter.y + cameraRelative.y,
    };
}

} //namespace Engine