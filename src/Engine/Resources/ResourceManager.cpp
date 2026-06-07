#include "Engine/Resources/ResourceManager.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace Engine {

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
    const std::string normalizedPath = normalizePath(path);
    if (normalizedPath.empty()) {
        return m_missingTexture;
    }

    if (const auto found = m_texturesByPath.find(normalizedPath); found != m_texturesByPath.end()) {
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

    return registerTexture(std::move(resource));
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

TextureHandle ResourceManager::findTexture(const std::filesystem::path& path) const
{
    const std::string normalizedPath = normalizePath(path);
    const auto found = m_texturesByPath.find(normalizedPath);
    return found == m_texturesByPath.end() ? TextureHandle::invalid() : found->second;
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

} // namespace Engine