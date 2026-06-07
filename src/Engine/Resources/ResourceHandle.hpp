#pragma once

#include <cstdint>
#include <functional>

namespace Engine {

struct AssetHandle {
    std::uint64_t value{0};

    static constexpr AssetHandle invalid() noexcept{ return {}; }
    constexpr explicit operator bool() const noexcept { return value != 0; }

    friend constexpr bool operator==(AssetHandle left, AssetHandle right) noexcept
    {
        return left.value == right.value;
    }

    friend constexpr bool operator!=(AssetHandle left, AssetHandle right) noexcept
    {
        return !(left == right);
    }
};

struct TextureHandle {
    AssetHandle asset{};

    static constexpr TextureHandle invalid() noexcept { return {}; }
    static constexpr TextureHandle fromValue(std::uint64_t value) noexcept { return {{value}}; }

    constexpr explicit operator bool() const noexcept { return static_cast<bool>(asset); }
    constexpr std::uint64_t value() const noexcept { return asset.value; }

    friend constexpr bool operator==(TextureHandle left, TextureHandle right) noexcept
    {
        return left.asset == right.asset;
    }

    friend constexpr bool operator!=(TextureHandle left, TextureHandle right) noexcept
    {
        return !(left == right);
    }
};

} // namespace Engine

namespace std {

template<>
struct hash<Engine::TextureHandle> {
    std::size_t operator()(Engine::TextureHandle handle) const noexcept
    {
        return std::hash<std::uint64_t>{}(handle.value());
    }
};

} // namespace std