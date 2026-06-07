#pragma once

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

class ResourceManager final {
public:
    ResourceManager();

    TextureHandle loadTexture(const std::filesystem::path& path);

    [[nodiscard]] TextureHandle findTexture(const std::filesystem::path& path) const;
    [[nodiscard]] const TextureResource& texture(TextureHandle handle) const;
    [[nodiscard]] const TextureResource* tryTexture(TextureHandle handle) const;
    [[nodiscard]] std::vector<TextureAssetInfo> scanTextureAssets(const std::filesystem::path& root) const;

    [[nodiscard]] TextureHandle whiteTexture() const noexcept;
    [[nodiscard]] TextureHandle missingTexture() const noexcept;
    [[nodiscard]] std::string normalizePath(const std::filesystem::path& path) const;
    [[nodiscard]] std::size_t textureCount() const noexcept;

private:
    TextureHandle registerTexture(TextureResource resource);
    TextureHandle createFallbackTexture(
        std::string normalizedPath,
        TextureFallbackKind kind,
        std::vector<std::uint8_t> rgba8,
        std::uint32_t width,
        std::uint32_t height);

    [[nodiscard]] bool supportedImagePath(const std::filesystem::path& path) const;

    std::uint64_t m_nextHandleValue{1};
    std::vector<TextureResource> m_textures;
    std::unordered_map<std::uint64_t, std::size_t> m_textureIndexByHandle;
    std::unordered_map<std::string, TextureHandle> m_texturesByPath;
    TextureHandle m_whiteTexture{};
    TextureHandle m_missingTexture{};
};

} // namespace Engine