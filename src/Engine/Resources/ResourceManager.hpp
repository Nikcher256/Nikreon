#pragma once

#include "Engine/Resources/MaterialResource.hpp"
#include "Engine/Resources/MeshResource.hpp"
#include "Engine/Resources/ModelResource.hpp"
#include "Engine/Resources/TextureResource.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

struct TextureAssetInfo {
    std::filesystem::path path;
    std::string displayPath;
    bool loaded{false};
    TextureHandle handle{};
};

struct ModelAssetInfo {
    std::filesystem::path path;
    std::string displayPath;
    bool loaded{false};
    ModelHandle handle{};
};

class ResourceManager final {
public:
    ResourceManager();

    TextureHandle loadTexture(const std::filesystem::path& path);
    ModelHandle loadModel(const std::filesystem::path& path);

    [[nodiscard]] TextureHandle findTexture(const std::filesystem::path& path) const;
    [[nodiscard]] ModelHandle findModel(const std::filesystem::path& path) const;

    [[nodiscard]] const TextureResource& texture(TextureHandle handle) const;
    [[nodiscard]] const TextureResource* tryTexture(TextureHandle handle) const;
    [[nodiscard]] const MeshResource& mesh(MeshHandle handle) const;
    [[nodiscard]] const MeshResource* tryMesh(MeshHandle handle) const;
    [[nodiscard]] const ModelResource& model(ModelHandle handle) const;
    [[nodiscard]] const ModelResource* tryModel(ModelHandle handle) const;
    [[nodiscard]] const MaterialResource& material(MaterialHandle handle) const;
    [[nodiscard]] const MaterialResource* tryMaterial(MaterialHandle handle) const;

    [[nodiscard]] std::vector<TextureAssetInfo> scanTextureAssets(const std::filesystem::path& root) const;
    [[nodiscard]] std::vector<ModelAssetInfo> scanModelAssets(const std::filesystem::path& root) const;

    [[nodiscard]] TextureHandle whiteTexture() const noexcept;
    [[nodiscard]] TextureHandle missingTexture() const noexcept;
    [[nodiscard]] std::string normalizePath(const std::filesystem::path& path) const;
    [[nodiscard]] std::size_t textureCount() const noexcept;
    [[nodiscard]] std::size_t meshCount() const noexcept;
    [[nodiscard]] std::size_t modelCount() const noexcept;
    [[nodiscard]] std::size_t materialCount() const noexcept;

private:
    TextureHandle registerTexture(TextureResource resource);
    MeshHandle registerMesh(MeshResource resource);
    ModelHandle registerModel(ModelResource resource);
    MaterialHandle registerMaterial(MaterialResource resource);

    TextureHandle createFallbackTexture(
        std::string normalizedPath,
        TextureFallbackKind kind,
        std::vector<std::uint8_t> rgba8,
        std::uint32_t width,
        std::uint32_t height);

    [[nodiscard]] bool supportedImagePath(const std::filesystem::path& path) const;
    [[nodiscard]] bool supportedModelPath(const std::filesystem::path& path) const;

    std::uint64_t m_nextHandleValue{1};

    std::vector<TextureResource> m_textures;
    std::unordered_map<std::uint64_t, std::size_t> m_textureIndexByHandle;
    std::unordered_map<std::string, TextureHandle> m_texturesByPath;

    std::vector<MeshResource> m_meshes;
    std::unordered_map<std::uint64_t, std::size_t> m_meshIndexByHandle;

    std::vector<ModelResource> m_models;
    std::unordered_map<std::uint64_t, std::size_t> m_modelIndexByHandle;
    std::unordered_map<std::string, ModelHandle> m_modelsByPath;

    std::vector<MaterialResource> m_materials;
    std::unordered_map<std::uint64_t, std::size_t> m_materialIndexByHandle;

    TextureHandle m_whiteTexture{};
    TextureHandle m_missingTexture{};
};

} // namespace Engine
