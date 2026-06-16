#include "Engine/Editor/EditorViewportOverlayController.hpp"

#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace Engine {

namespace {

constexpr glm::vec4 MinorGridColor{0.27f, 0.34f, 0.43f, 0.16f};
constexpr glm::vec4 MajorGridColor{0.40f, 0.50f, 0.62f, 0.30f};
constexpr glm::vec4 XAxisColor{0.95f, 0.28f, 0.28f, 0.85f};
constexpr glm::vec4 YAxisColor{0.30f, 0.85f, 0.45f, 0.85f};
constexpr glm::vec4 ZAxisColor{0.25f, 0.48f, 1.00f, 0.85f};

struct ViewGridDescription {
    glm::vec3 origin{0.0f, 0.0f, 0.0f};
    glm::vec3 axisA{1.0f, 0.0f, 0.0f};
    glm::vec3 axisB{0.0f, 1.0f, 0.0f};
    glm::vec3 viewDirection{0.0f, 0.0f, -1.0f};
    float spacing{10.0f};
    int majorLineInterval{10};
};

glm::vec4 colorForWorldAxis(const glm::vec3& axis)
{
    const glm::vec3 absoluteAxis = glm::abs(axis);
    if (absoluteAxis.x >= absoluteAxis.y && absoluteAxis.x >= absoluteAxis.z) {
        return XAxisColor;
    }
    if (absoluteAxis.y >= absoluteAxis.z) {
        return YAxisColor;
    }
    return ZAxisColor;
}

ViewGridDescription viewGridFor(const EditorViewOrientation orientation)
{
    // Nikreon editor camera math is Z-up: Top/Bottom use XY, Front/Back use XZ,
    // and Left/Right use YZ. Axis colors stay tied to world X/Y/Z, not screen direction.
    switch (orientation) {
    case EditorViewOrientation::Front:
        return {{}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}};
    case EditorViewOrientation::Back:
        return {{}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}};
    case EditorViewOrientation::Left:
        return {{}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}};
    case EditorViewOrientation::Right:
        return {{}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}};
    case EditorViewOrientation::Bottom:
        return {{}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    case EditorViewOrientation::Perspective:
    case EditorViewOrientation::Top:
        break;
    }

    return {{}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
}

void drawViewGrid(DebugRenderer& debugRenderer, const ViewGridDescription& grid, const Camera3D& camera)
{
    const float spacing = std::max(grid.spacing, 0.001f);
    const int majorInterval = std::max(grid.majorLineInterval, 1);
    const float halfB = std::max(camera.orthographicHeight * 0.5f, spacing);
    const float halfA = std::max(halfB * std::max(camera.aspectRatio, 0.001f), spacing);

    const float centerA = glm::dot(camera.position - grid.origin, grid.axisA);
    const float centerB = glm::dot(camera.position - grid.origin, grid.axisB);
    const float startA = std::floor((centerA - halfA) / spacing) * spacing;
    const float endA = std::ceil((centerA + halfA) / spacing) * spacing;
    const float startB = std::floor((centerB - halfB) / spacing) * spacing;
    const float endB = std::ceil((centerB + halfB) / spacing) * spacing;

    for (float a = startA; a <= endA; a += spacing) {
        const int index = static_cast<int>(std::round(a / spacing));
        const glm::vec4 color = (index % majorInterval) == 0 ? MajorGridColor : MinorGridColor;
        debugRenderer.drawLine(grid.origin + grid.axisA * a + grid.axisB * startB, grid.origin + grid.axisA * a + grid.axisB * endB, color);
    }

    for (float b = startB; b <= endB; b += spacing) {
        const int index = static_cast<int>(std::round(b / spacing));
        const glm::vec4 color = (index % majorInterval) == 0 ? MajorGridColor : MinorGridColor;
        debugRenderer.drawLine(grid.origin + grid.axisA * startA + grid.axisB * b, grid.origin + grid.axisA * endA + grid.axisB * b, color);
    }

    if (startB <= 0.0f && endB >= 0.0f) {
        debugRenderer.drawLine(grid.origin + grid.axisA * startA, grid.origin + grid.axisA * endA, colorForWorldAxis(grid.axisA));
    }
    if (startA <= 0.0f && endA >= 0.0f) {
        debugRenderer.drawLine(grid.origin + grid.axisB * startB, grid.origin + grid.axisB * endB, colorForWorldAxis(grid.axisB));
    }
}

} // namespace

void EditorViewportOverlayController::draw(
    DebugRenderer& debugRenderer,
    const EditorViewport& viewport,
    const SpriteRendererCamera& camera) const
{
    if (!viewport.gridVisible() || viewport.mode() == EditorViewportMode::Play) {
        return;
    }

    if (!editorViewOrientationIsPerspective(viewport.viewOrientation())) {
        drawViewGrid(debugRenderer, viewGridFor(viewport.viewOrientation()), camera.camera3D);
        return;
    }

    debugRenderer.drawGrid({
        .enabled = true,
        .step = 10.0f,
        .majorEvery = 10.0f,
        .fadeStart = 0.0f,
        .fadeEnd = 3100.0f,
        .fadeMinimumAlpha = 0.005f,
    });
}

} // namespace Engine
