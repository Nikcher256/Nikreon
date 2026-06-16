#include "Engine/Editor/PickingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Engine {

namespace {

constexpr float RayEpsilon = 0.00001f;

glm::mat4 transformMatrix(const TransformComponent& transform)
{
    glm::mat4 matrix{1.0f};
    matrix = glm::translate(matrix, transform.position);
    matrix = glm::rotate(matrix, transform.rotationRadians.x, {1.0f, 0.0f, 0.0f});
    matrix = glm::rotate(matrix, transform.rotationRadians.y, {0.0f, 1.0f, 0.0f});
    matrix = glm::rotate(matrix, transform.rotationRadians.z, {0.0f, 0.0f, 1.0f});
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point)
{
    const glm::vec4 transformed = matrix * glm::vec4{point, 1.0f};
    return glm::vec3{transformed} / std::max(std::abs(transformed.w), 0.0001f);
}

} // namespace

std::optional<PickHit> PickingSystem::pickScene(const Scene& scene, const Ray3D& ray)
{
    std::optional<PickHit> nearestHit;
    const std::span<const SceneObject> objects = scene.objects();

    for (const SceneObject& object : objects) {
        std::optional<PickHit> hit;

        if (object.spriteRenderer) {
            hit = pickSprite(object, ray);
        }

        if (!hit) {
            continue;
        }

        if (!nearestHit || hit->distance <= nearestHit->distance) {
            nearestHit = hit;
        }
    }

    return nearestHit;
}

PickingSystem::SpriteQuadCorners PickingSystem::spriteQuadCorners(const SceneObject& object)
{
    if (!object.spriteRenderer) {
        return {};
    }

    const SpriteRendererComponent& sprite = *object.spriteRenderer;
    const glm::vec2 originOffset = sprite.size * sprite.origin;

    const glm::vec3 localCorners[4] = {
        {-originOffset.x, -originOffset.y, 0.0f},
        {sprite.size.x - originOffset.x, -originOffset.y, 0.0f},
        {sprite.size.x - originOffset.x, sprite.size.y - originOffset.y, 0.0f},
        {-originOffset.x, sprite.size.y - originOffset.y, 0.0f},
    };

    const glm::mat4 matrix = transformMatrix(object.transform);
    return {
        transformPoint(matrix, localCorners[0]),
        transformPoint(matrix, localCorners[1]),
        transformPoint(matrix, localCorners[2]),
        transformPoint(matrix, localCorners[3]),
    };
}

std::optional<PickHit> PickingSystem::pickSprite(const SceneObject& object, const Ray3D& ray)
{
    const SpriteQuadCorners corners = spriteQuadCorners(object);

    const std::optional<float> firstTriangle = rayTriangleDistance(ray, corners[0], corners[1], corners[2]);
    const std::optional<float> secondTriangle = rayTriangleDistance(ray, corners[2], corners[3], corners[0]);

    if (!firstTriangle && !secondTriangle) {
        return std::nullopt;
    }

    float distance = 0.0f;
    if (firstTriangle && secondTriangle) {
        distance = std::min(*firstTriangle, *secondTriangle);
    } else if (firstTriangle) {
        distance = *firstTriangle;
    } else {
        distance = *secondTriangle;
    }

    return PickHit{
        object.id,
        distance,
        ray.origin + ray.direction * distance,
    };
}

std::optional<float> PickingSystem::rayTriangleDistance(
    const Ray3D& ray,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
{
    const glm::vec3 edge1 = b - a;
    const glm::vec3 edge2 = c - a;
    const glm::vec3 p = glm::cross(ray.direction, edge2);
    const float determinant = glm::dot(edge1, p);

    if (std::abs(determinant) < RayEpsilon) {
        return std::nullopt;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const glm::vec3 t = ray.origin - a;
    const float u = glm::dot(t, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    const glm::vec3 q = glm::cross(t, edge1);
    const float v = glm::dot(ray.direction, q) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }

    const float distance = glm::dot(edge2, q) * inverseDeterminant;
    if (distance < 0.0f) {
        return std::nullopt;
    }

    return distance;
}

} // namespace Engine