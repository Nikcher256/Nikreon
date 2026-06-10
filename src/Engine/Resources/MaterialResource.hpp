#pragma once

#include "Engine/Resources/ResourceHandle.hpp"

#include <string>

#include <glm/vec4.hpp>

namespace Engine {

struct MaterialResource {
    MaterialHandle handle{};
    std::string name;
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
};

} // namespace Engine