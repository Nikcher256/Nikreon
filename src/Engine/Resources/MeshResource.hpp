#pragma once

#include "Engine/Resources/ResourceHandle.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec2 uv0{0.0f};
};

struct MeshBounds {
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    bool valid{false};
};

struct MeshSubmesh {
    std::uint32_t firstIndex{0};
    std::uint32_t indexCount{0};
    MaterialHandle material{};
    MeshBounds bounds{};
};

struct MeshResource {
    MeshHandle handle{};
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshSubmesh> submeshes;
    MeshBounds bounds{};
};

} // namespace Engine