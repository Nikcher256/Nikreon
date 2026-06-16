#pragma once

#include <cstdint>

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
    void recordClear(VkCommandBuffer commandBuffer, const glm::vec4& clearColor) const;
    void recordBeginRenderPass(VkCommandBuffer commandBuffer, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void recordEndRenderPass(VkCommandBuffer commandBuffer) const;
    void recordCopyTo(
        VkCommandBuffer commandBuffer,
        VkImage destinationImage,
        const glm::ivec2& destinationPosition,
        const glm::uvec2& destinationSize) const;

    [[nodiscard]] glm::uvec2 size() const;
    [[nodiscard]] VkRenderPass renderPass() const;
    [[nodiscard]] VkFramebuffer framebuffer() const;

private:
    void createRenderPass();
    void create();
    void destroy();
    void destroyRenderPass();
    [[nodiscard]] VkFormat findDepthFormat() const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkFormat m_format{VK_FORMAT_UNDEFINED};
    VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
    VkImage m_depthImage{VK_NULL_HANDLE};
    VkDeviceMemory m_depthMemory{VK_NULL_HANDLE};
    VkImageView m_depthImageView{VK_NULL_HANDLE};
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    glm::uvec2 m_size{1U, 1U};
};

} // namespace Engine
