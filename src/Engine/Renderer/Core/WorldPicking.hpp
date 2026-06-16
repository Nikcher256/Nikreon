#pragma once

#include "Engine/Renderer/Core/WorldRenderView.hpp"

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

[[nodiscard]] inline Ray3D worldRayFromScreenPoint(const WorldRenderView& view, const glm::vec2& screenPosition)
{
    const glm::vec2 safeViewport{
        std::max(view.viewportSize.x, 1.0f),
        std::max(view.viewportSize.y, 1.0f),
    };

    const float x = (2.0f * screenPosition.x) / safeViewport.x - 1.0f;

    // The Vulkan viewport uses top-left screen coordinates with positive
    // height, so screen Y maps directly to clip-space Y here. Camera3D applies
    // the matching projection Y flip so world +Z still appears upward.
    const float y = (2.0f * screenPosition.y) / safeViewport.y - 1.0f;

    const glm::mat4 inverseViewProjection = glm::inverse(view.viewProjection);
    const glm::vec4 nearPoint = inverseViewProjection * glm::vec4{x, y, 0.0f, 1.0f};
    const glm::vec4 farPoint = inverseViewProjection * glm::vec4{x, y, 1.0f, 1.0f};

    const glm::vec3 nearWorld = glm::vec3{nearPoint} / std::max(std::abs(nearPoint.w), 0.0001f);
    const glm::vec3 farWorld = glm::vec3{farPoint} / std::max(std::abs(farPoint.w), 0.0001f);

    if (view.projectionMode == WorldRenderProjection::Perspective) {
        const glm::vec3 direction = glm::normalize(nearWorld - view.cameraPosition);
        return {view.cameraPosition, direction};
    }

    return {nearWorld, glm::normalize(farWorld - nearWorld)};
}

}//engine
