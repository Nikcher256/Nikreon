#include "Engine/Resources/ResourceManager.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <cstring>
#include <limits>

#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#ifdef NIKREON_HAS_TINYGLTF
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace Engine {

#ifdef NIKREON_HAS_TINYGLTF
namespace {

bool skipImageLoad(
    tinygltf::Image*,
    int,
    std::string*,
    std::string*,
    int,
    int,
    const unsigned char*,
    int,
    void*)
{
    return true;
}

void expandBounds(MeshBounds& bounds, const glm::vec3& point)
{
    if (!bounds.valid) {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.valid = true;
        return;
    }

    bounds.minimum = glm::min(bounds.minimum, point);
    bounds.maximum = glm::max(bounds.maximum, point);
}

const tinygltf::Accessor* accessorAt(const tinygltf::Model& model, const int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= model.accessors.size()) {
        return nullptr;
    }

    return &model.accessors[static_cast<std::size_t>(index)];
}

const unsigned char* accessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    const tinygltf::BufferView*& bufferView,
    int& stride)
{
    if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size()) {
        return nullptr;
    }

    bufferView = &model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    if (bufferView->buffer < 0 || static_cast<std::size_t>(bufferView->buffer) >= model.buffers.size()) {
        return nullptr;
    }

    const tinygltf::Buffer& buffer = model.buffers[static_cast<std::size_t>(bufferView->buffer)];
    stride = accessor.ByteStride(*bufferView);
    if (stride <= 0) {
        return nullptr;
    }

    const std::size_t offset = bufferView->byteOffset + accessor.byteOffset;
    if (offset >= buffer.data.size()) {
        return nullptr;
    }

    return buffer.data.data() + offset;
}

bool readFloatComponents(
    const tinygltf::Model& model,
    const int accessorIndex,
    const std::size_t element,
    float* out,
    const int expectedComponents)
{
    const tinygltf::Accessor* accessor = accessorAt(model, accessorIndex);
    if (accessor == nullptr ||
        accessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        tinygltf::GetNumComponentsInType(static_cast<std::uint32_t>(accessor->type)) < expectedComponents ||
        element >= accessor->count) {
        return false;
    }

    const tinygltf::BufferView* bufferView = nullptr;
    int stride = 0;
    const unsigned char* data = accessorData(model, *accessor, bufferView, stride);
    if (data == nullptr) {
        return false;
    }

    std::memcpy(out, data + element * static_cast<std::size_t>(stride), sizeof(float) * static_cast<std::size_t>(expectedComponents));
    return true;
}

glm::vec3 readVec3(
    const tinygltf::Model& model,
    const int accessorIndex,
    const std::size_t element,
    const glm::vec3& fallback)
{
    float values[3]{};
    return readFloatComponents(model, accessorIndex, element, values, 3)
        ? glm::vec3{values[0], values[1], values[2]}
        : fallback;
}

glm::vec4 readVec4(
    const tinygltf::Model& model,
    const int accessorIndex,
    const std::size_t element,
    const glm::vec4& fallback)
{
    float values[4]{};
    return readFloatComponents(model, accessorIndex, element, values, 4)
        ? glm::vec4{values[0], values[1], values[2], values[3]}
        : fallback;
}

glm::vec2 readVec2(
    const tinygltf::Model& model,
    const int accessorIndex,
    const std::size_t element,
    const glm::vec2& fallback)
{
    float values[2]{};
    return readFloatComponents(model, accessorIndex, element, values, 2)
        ? glm::vec2{values[0], values[1]}
        : fallback;
}

std::uint32_t readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, const std::size_t element)
{
    const tinygltf::BufferView* bufferView = nullptr;
    int stride = 0;
    const unsigned char* data = accessorData(model, accessor, bufferView, stride);
    if (data == nullptr || element >= accessor.count) {
        return 0U;
    }

    const unsigned char* value = data + element * static_cast<std::size_t>(stride);
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return static_cast<std::uint32_t>(*value);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        std::uint16_t index = 0;
        std::memcpy(&index, value, sizeof(index));
        return index;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        std::uint32_t index = 0;
        std::memcpy(&index, value, sizeof(index));
        return index;
    }
    default:
        return 0U;
    }
}

MaterialResource materialFromGltf(const tinygltf::Material& gltfMaterial, const std::size_t index)
{
    MaterialResource material;
    material.name = gltfMaterial.name.empty()
        ? "Material " + std::to_string(index + 1U)
        : gltfMaterial.name;

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 4U) {
        material.baseColorFactor = {
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3]),
        };
    }

    material.metallicFactor = static_cast<float>(pbr.metallicFactor);
    material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
    return material;
}

} // namespace
#endif

ResourceManager::ResourceManager()
{
    m_whiteTexture = createFallbackTexture(
        "__builtin/white-texture",
        TextureFallbackKind::White,
        {255U, 255U, 255U, 255U},
        1U,
        1U);

    m_missingTexture = createFallbackTexture(
        "__builtin/missing-texture",
        TextureFallbackKind::Missing,
        {
            255U, 0U, 255U, 255U, 24U, 24U, 24U, 255U,
            24U, 24U, 24U, 255U, 255U, 0U, 255U, 255U,
        },
        2U,
        2U);
}

TextureHandle ResourceManager::loadTexture(const std::filesystem::path& path)
{
    const std::string requestPath = path.lexically_normal().generic_string();
    if (!requestPath.empty()) {
        if (const auto found = m_textureRequestCache.find(requestPath); found != m_textureRequestCache.end()) {
            return found->second;
        }
    }

    const std::string normalizedPath = normalizePath(path);
    if (normalizedPath.empty()) {
        return m_missingTexture;
    }

    if (const auto found = m_texturesByPath.find(normalizedPath); found != m_texturesByPath.end()) {
        if (!requestPath.empty()) {
            m_textureRequestCache[requestPath] = found->second;
        }
        return found->second;
    }

    if (!supportedImagePath(std::filesystem::path{normalizedPath})) {
        return m_missingTexture;
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    stbi_uc* loadedPixels = stbi_load(normalizedPath.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (loadedPixels == nullptr || width <= 0 || height <= 0) {
        if (loadedPixels != nullptr) {
            stbi_image_free(loadedPixels);
        }
        return m_missingTexture;
    }

    const std::size_t byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    std::vector<std::uint8_t> rgba8(byteCount);
    std::copy(loadedPixels, loadedPixels + byteCount, rgba8.begin());
    stbi_image_free(loadedPixels);

    TextureResource resource;
    resource.normalizedPath = normalizedPath;
    resource.pixels.width = static_cast<std::uint32_t>(width);
    resource.pixels.height = static_cast<std::uint32_t>(height);
    resource.pixels.channels = 4U;
    resource.pixels.rgba8 = std::move(rgba8);
    resource.pixels.loadedFromDisk = true;

    const TextureHandle handle = registerTexture(std::move(resource));
    if (!requestPath.empty()) {
        m_textureRequestCache[requestPath] = handle;
    }
    return handle;
}

ModelHandle ResourceManager::loadModel(const std::filesystem::path& path)
{
    const std::string normalizedPath = normalizePath(path);
    if (normalizedPath.empty()) {
        return ModelHandle::invalid();
    }

    if (const auto found = m_modelsByPath.find(normalizedPath); found != m_modelsByPath.end()) {
        return found->second;
    }

    if (!supportedModelPath(std::filesystem::path{normalizedPath})) {
        return ModelHandle::invalid();
    }

#ifndef NIKREON_HAS_TINYGLTF
    spdlog::warn("Model loading requested, but tinygltf is not available: {}", normalizedPath);
    return ModelHandle::invalid();
#else
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(skipImageLoad, nullptr);

    tinygltf::Model gltfModel;
    std::string error;
    std::string warning;

    const std::filesystem::path normalizedFilesystemPath{normalizedPath};
    std::string extension = normalizedFilesystemPath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    const bool loaded = extension == ".glb"
        ? loader.LoadBinaryFromFile(&gltfModel, &error, &warning, normalizedPath)
        : loader.LoadASCIIFromFile(&gltfModel, &error, &warning, normalizedPath);

    if (!warning.empty()) {
        spdlog::warn("tinygltf warning while loading '{}': {}", normalizedPath, warning);
    }

    if (!loaded) {
        spdlog::warn("Failed to load model '{}': {}", normalizedPath, error);
        return ModelHandle::invalid();
    }

    std::vector<MaterialHandle> materialHandles;
    materialHandles.reserve(gltfModel.materials.size());
    for (std::size_t materialIndex = 0; materialIndex < gltfModel.materials.size(); ++materialIndex) {
        materialHandles.push_back(registerMaterial(materialFromGltf(gltfModel.materials[materialIndex], materialIndex)));
    }

    ModelResource model;
    model.normalizedPath = normalizedPath;
    model.name = normalizedFilesystemPath.stem().string();

    for (std::size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh& gltfMesh = gltfModel.meshes[meshIndex];

        MeshResource mesh;
        mesh.name = gltfMesh.name.empty()
            ? model.name + " Mesh " + std::to_string(meshIndex + 1U)
            : gltfMesh.name;

        for (const tinygltf::Primitive& primitive : gltfMesh.primitives) {
            if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            const auto positionAttribute = primitive.attributes.find("POSITION");
            if (positionAttribute == primitive.attributes.end()) {
                continue;
            }

            const tinygltf::Accessor* positionAccessor = accessorAt(gltfModel, positionAttribute->second);
            if (positionAccessor == nullptr || positionAccessor->type != TINYGLTF_TYPE_VEC3) {
                continue;
            }

            const auto normalAttribute = primitive.attributes.find("NORMAL");
            const auto tangentAttribute = primitive.attributes.find("TANGENT");
            const auto uvAttribute = primitive.attributes.find("TEXCOORD_0");

            const int normalAccessor = normalAttribute == primitive.attributes.end() ? -1 : normalAttribute->second;
            const int tangentAccessor = tangentAttribute == primitive.attributes.end() ? -1 : tangentAttribute->second;
            const int uvAccessor = uvAttribute == primitive.attributes.end() ? -1 : uvAttribute->second;

            const std::uint32_t firstVertex = static_cast<std::uint32_t>(mesh.vertices.size());
            const std::uint32_t firstIndex = static_cast<std::uint32_t>(mesh.indices.size());

            MeshBounds submeshBounds;
            for (std::size_t vertexIndex = 0; vertexIndex < positionAccessor->count; ++vertexIndex) {
                MeshVertex vertex;
                vertex.position = readVec3(gltfModel, positionAttribute->second, vertexIndex, {});
                vertex.normal = readVec3(gltfModel, normalAccessor, vertexIndex, {0.0f, 0.0f, 1.0f});
                vertex.tangent = readVec4(gltfModel, tangentAccessor, vertexIndex, {1.0f, 0.0f, 0.0f, 1.0f});
                vertex.uv0 = readVec2(gltfModel, uvAccessor, vertexIndex, {0.0f, 0.0f});

                expandBounds(submeshBounds, vertex.position);
                expandBounds(mesh.bounds, vertex.position);
                mesh.vertices.push_back(vertex);
            }

            if (primitive.indices >= 0) {
                const tinygltf::Accessor* indexAccessor = accessorAt(gltfModel, primitive.indices);
                if (indexAccessor != nullptr) {
                    for (std::size_t index = 0; index < indexAccessor->count; ++index) {
                        mesh.indices.push_back(firstVertex + readIndex(gltfModel, *indexAccessor, index));
                    }
                }
            } else {
                for (std::size_t index = 0; index < positionAccessor->count; ++index) {
                    mesh.indices.push_back(firstVertex + static_cast<std::uint32_t>(index));
                }
            }

            MeshSubmesh submesh;
            submesh.firstIndex = firstIndex;
            submesh.indexCount = static_cast<std::uint32_t>(mesh.indices.size()) - firstIndex;
            submesh.bounds = submeshBounds;
            if (primitive.material >= 0 && static_cast<std::size_t>(primitive.material) < materialHandles.size()) {
                submesh.material = materialHandles[static_cast<std::size_t>(primitive.material)];
            }

            if (submesh.indexCount > 0U) {
                mesh.submeshes.push_back(submesh);
            }
        }

        if (!mesh.vertices.empty() && !mesh.indices.empty()) {
            expandBounds(model.bounds, mesh.bounds.minimum);
            expandBounds(model.bounds, mesh.bounds.maximum);

            const MeshHandle meshHandle = registerMesh(std::move(mesh));
            model.meshes.push_back({meshHandle});
        }
    }

    model.materials = std::move(materialHandles);

    if (model.meshes.empty()) {
        spdlog::warn("Model '{}' loaded but contained no triangle meshes.", normalizedPath);
        return ModelHandle::invalid();
    }

    const ModelHandle handle = registerModel(std::move(model));
    spdlog::info("Loaded model '{}'. Models: {}, meshes: {}, materials: {}", normalizedPath, modelCount(), meshCount(), materialCount());
    return handle;
#endif
}

std::vector<TextureAssetInfo> ResourceManager::scanTextureAssets(const std::filesystem::path& root) const
{
    std::vector<TextureAssetInfo> assets;

    std::error_code error;
    const std::filesystem::path absoluteRoot = std::filesystem::absolute(root, error);
    if (error || !std::filesystem::exists(absoluteRoot, error) || !std::filesystem::is_directory(absoluteRoot, error)) {
        return assets;
    }

    const std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator{absoluteRoot, options, error}, end; iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }

        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(error) || error || !supportedImagePath(entry.path())) {
            error.clear();
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), absoluteRoot, error);
        if (error) {
            relativePath = entry.path().filename();
            error.clear();
        }

        const std::string normalizedPath = normalizePath(entry.path());
        const auto found = m_texturesByPath.find(normalizedPath);

        assets.push_back({
            entry.path(),
            relativePath.generic_string(),
            found != m_texturesByPath.end(),
            found != m_texturesByPath.end() ? found->second : TextureHandle::invalid(),
        });
    }

    std::sort(assets.begin(), assets.end(), [](const TextureAssetInfo& left, const TextureAssetInfo& right) {
        return left.displayPath < right.displayPath;
    });

    return assets;
}

std::vector<ModelAssetInfo> ResourceManager::scanModelAssets(const std::filesystem::path& root) const
{
    std::vector<ModelAssetInfo> assets;

    std::error_code error;
    const std::filesystem::path absoluteRoot = std::filesystem::absolute(root, error);
    if (error || !std::filesystem::exists(absoluteRoot, error) || !std::filesystem::is_directory(absoluteRoot, error)) {
        return assets;
    }

    const std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator{absoluteRoot, options, error}, end; iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }

        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(error) || error || !supportedModelPath(entry.path())) {
            error.clear();
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), absoluteRoot, error);
        if (error) {
            relativePath = entry.path().filename();
            error.clear();
        }

        const std::string normalizedPath = normalizePath(entry.path());
        const auto found = m_modelsByPath.find(normalizedPath);

        assets.push_back({
            entry.path(),
            relativePath.generic_string(),
            found != m_modelsByPath.end(),
            found != m_modelsByPath.end() ? found->second : ModelHandle::invalid(),
        });
    }

    std::sort(assets.begin(), assets.end(), [](const ModelAssetInfo& left, const ModelAssetInfo& right) {
        return left.displayPath < right.displayPath;
    });

    return assets;
}

TextureHandle ResourceManager::findTexture(const std::filesystem::path& path) const
{
    const std::string normalizedPath = normalizePath(path);
    const auto found = m_texturesByPath.find(normalizedPath);
    return found == m_texturesByPath.end() ? TextureHandle::invalid() : found->second;
}

ModelHandle ResourceManager::findModel(const std::filesystem::path& path) const
{
    const std::string normalizedPath = normalizePath(path);
    const auto found = m_modelsByPath.find(normalizedPath);
    return found == m_modelsByPath.end() ? ModelHandle::invalid() : found->second;
}

const TextureResource& ResourceManager::texture(TextureHandle handle) const
{
    if (const TextureResource* resource = tryTexture(handle)) {
        return *resource;
    }

    throw std::runtime_error("Invalid texture handle.");
}

const TextureResource* ResourceManager::tryTexture(TextureHandle handle) const
{
    const auto found = m_textureIndexByHandle.find(handle.value());
    if (found == m_textureIndexByHandle.end()) {
        return nullptr;
    }

    return &m_textures[found->second];
}

const MeshResource& ResourceManager::mesh(MeshHandle handle) const
{
    if (const MeshResource* resource = tryMesh(handle)) {
        return *resource;
    }

    throw std::runtime_error("Invalid mesh handle.");
}

const MeshResource* ResourceManager::tryMesh(MeshHandle handle) const
{
    const auto found = m_meshIndexByHandle.find(handle.value());
    if (found == m_meshIndexByHandle.end()) {
        return nullptr;
    }

    return &m_meshes[found->second];
}

const ModelResource& ResourceManager::model(ModelHandle handle) const
{
    if (const ModelResource* resource = tryModel(handle)) {
        return *resource;
    }

    throw std::runtime_error("Invalid model handle.");
}

const ModelResource* ResourceManager::tryModel(ModelHandle handle) const
{
    const auto found = m_modelIndexByHandle.find(handle.value());
    if (found == m_modelIndexByHandle.end()) {
        return nullptr;
    }

    return &m_models[found->second];
}

const MaterialResource& ResourceManager::material(MaterialHandle handle) const
{
    if (const MaterialResource* resource = tryMaterial(handle)) {
        return *resource;
    }

    throw std::runtime_error("Invalid material handle.");
}

const MaterialResource* ResourceManager::tryMaterial(MaterialHandle handle) const
{
    const auto found = m_materialIndexByHandle.find(handle.value());
    if (found == m_materialIndexByHandle.end()) {
        return nullptr;
    }

    return &m_materials[found->second];
}

TextureHandle ResourceManager::whiteTexture() const noexcept
{
    return m_whiteTexture;
}

TextureHandle ResourceManager::missingTexture() const noexcept
{
    return m_missingTexture;
}

std::string ResourceManager::normalizePath(const std::filesystem::path& path) const
{
    if (path.empty()) {
        return {};
    }

    std::error_code error;
    std::filesystem::path absolutePath = path;
    if (absolutePath.is_relative()) {
        absolutePath = std::filesystem::absolute(absolutePath, error);
        if (error) {
            absolutePath = path;
            error.clear();
        }
    }

    std::filesystem::path normalized = std::filesystem::weakly_canonical(absolutePath, error);
    if (error) {
        normalized = absolutePath.lexically_normal();
    }

    return normalized.generic_string();
}

std::size_t ResourceManager::textureCount() const noexcept
{
    return m_textures.size();
}

std::size_t ResourceManager::meshCount() const noexcept
{
    return m_meshes.size();
}

std::size_t ResourceManager::modelCount() const noexcept
{
    return m_models.size();
}

std::size_t ResourceManager::materialCount() const noexcept
{
    return m_materials.size();
}

TextureHandle ResourceManager::registerTexture(TextureResource resource)
{
    resource.handle = TextureHandle::fromValue(m_nextHandleValue++);
    const TextureHandle handle = resource.handle;
    const std::size_t index = m_textures.size();

    if (!resource.normalizedPath.empty()) {
        m_texturesByPath[resource.normalizedPath] = handle;
    }

    m_textures.push_back(std::move(resource));
    m_textureIndexByHandle.emplace(handle.value(), index);
    return handle;
}

MeshHandle ResourceManager::registerMesh(MeshResource resource)
{
    resource.handle = MeshHandle::fromValue(m_nextHandleValue++);
    const MeshHandle handle = resource.handle;
    const std::size_t index = m_meshes.size();

    m_meshes.push_back(std::move(resource));
    m_meshIndexByHandle.emplace(handle.value(), index);
    return handle;
}

ModelHandle ResourceManager::registerModel(ModelResource resource)
{
    resource.handle = ModelHandle::fromValue(m_nextHandleValue++);
    const ModelHandle handle = resource.handle;
    const std::size_t index = m_models.size();

    if (!resource.normalizedPath.empty()) {
        m_modelsByPath[resource.normalizedPath] = handle;
    }

    m_models.push_back(std::move(resource));
    m_modelIndexByHandle.emplace(handle.value(), index);
    return handle;
}

MaterialHandle ResourceManager::registerMaterial(MaterialResource resource)
{
    resource.handle = MaterialHandle::fromValue(m_nextHandleValue++);
    const MaterialHandle handle = resource.handle;
    const std::size_t index = m_materials.size();

    m_materials.push_back(std::move(resource));
    m_materialIndexByHandle.emplace(handle.value(), index);
    return handle;
}

TextureHandle ResourceManager::createFallbackTexture(
    std::string normalizedPath,
    TextureFallbackKind kind,
    std::vector<std::uint8_t> rgba8,
    std::uint32_t width,
    std::uint32_t height)
{
    const std::size_t expectedBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    if (rgba8.size() != expectedBytes) {
        throw std::runtime_error("Fallback texture pixel data has the wrong size.");
    }

    TextureResource resource;
    resource.normalizedPath = std::move(normalizedPath);
    resource.fallback = kind;
    resource.pixels.width = width;
    resource.pixels.height = height;
    resource.pixels.channels = 4U;
    resource.pixels.rgba8 = std::move(rgba8);
    return registerTexture(std::move(resource));
}

bool ResourceManager::supportedImagePath(const std::filesystem::path& path) const
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

bool ResourceManager::supportedModelPath(const std::filesystem::path& path) const
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    return extension == ".glb" || extension == ".gltf";
}

} // namespace Engine
