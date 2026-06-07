#pragma once

#include "Engine/Resources/ResourceHandle.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {

enum class TextureFallbackKind {
    None,
    White,
    Missing,
};

struct TexturePixels {
    std::uint32_t width{1};
    std::uint32_t height{1};
    std::uint32_t channels{4};
    std::vector<std::uint8_t> rgba8;
    bool loadedFromDisk{false};
};

struct TextureResource {
    TextureHandle handle{};
    std::string normalizedPath;
    TextureFallbackKind fallback{TextureFallbackKind::None};
    TexturePixels pixels{};

    bool isFallback() const noexcept
    {
        return fallback != TextureFallbackKind::None;
    }
};

} // namespace Engine