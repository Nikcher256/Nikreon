#pragma once

#include "Engine/Resources/ResourceHandle.hpp"
#include "Engine/Resources/MeshResource.hpp"

#include <string>
#include <vector>

namespace Engine {

struct ModelMeshRef {
    MeshHandle mesh{};
};

struct ModelResource {
    ModelHandle handle{};
    std::string normalizedPath;
    std::string name;
    std::vector<ModelMeshRef> meshes;
    std::vector<MaterialHandle> materials;
    MeshBounds bounds{};
};

} // namespace Engine