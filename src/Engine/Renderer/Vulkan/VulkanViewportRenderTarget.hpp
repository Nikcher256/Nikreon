#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <volk.h>

namespace Engine {

class VulkanViewportRenderTarget {
public:
    VulkanViewportRenderTarget(VkDevice device, VkPhysicalDevice physicalDevice, VkFormat format);
    ~VulkanViewportRenderTarget();

    VulkanViewportRenderTarget(const VulkanViewportRenderTarget&) = delete;
    VulkanViewportRenderTarget& operator=(const VulkanViewportRenderTarget&) = delete;

    void resize(const glm::uvec2& size);
    void recordClearAndCopy(
        VkCommandBuffer commandBuffer,
        VkImage destinationImage,
        const glm::ivec2& destinationPosition,
        const glm::uvec2& destinationSize,
        const glm::vec4& clearColor) const;

    [[nodiscard]] glm::uvec2 size() const;

private:
    void create();
    void destroy();
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkFormat m_format{VK_FORMAT_UNDEFINED};
    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    glm::uvec2 m_size{1U, 1U};
};

} // namespace Engine
