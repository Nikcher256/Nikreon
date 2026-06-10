#pragma once

#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <volk.h>

namespace Engine {

class VulkanDebugRenderer final {
public:
    VulkanDebugRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, std::size_t maxVertices = 65536);
    ~VulkanDebugRenderer();

    VulkanDebugRenderer(const VulkanDebugRenderer&) = delete;
    VulkanDebugRenderer& operator=(const VulkanDebugRenderer&) = delete;
    VulkanDebugRenderer(VulkanDebugRenderer&&) = delete;
    VulkanDebugRenderer& operator=(VulkanDebugRenderer&&) = delete;

    void begin(const glm::uvec2& viewportSize);
    void submit(const DebugRenderer& debugRenderer, const Renderer2DWorldCamera& camera);
    void end();
    void record(VkCommandBuffer commandBuffer) const;

    [[nodiscard]] std::size_t vertexCount() const;

private:
    struct GridPushConstants {
        glm::mat4 inverseViewProjection{1.0f};
        glm::vec4 cameraPosition{0.0f};
        glm::vec4 gridSettings{10.0f, 10.0f, 1.0f, 1.0f};
        glm::vec4 fadeSettings{350.0f, 850.0f, 0.0f, 0.0f};
    };

    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    void recordGrid(VkCommandBuffer commandBuffer) const;
    void recordLines(VkCommandBuffer commandBuffer) const;
    void createVertexBuffer();
    void uploadVertices();
    void destroy();

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& memory);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& bytecode) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] glm::mat4 viewProjectionFor(const Renderer2DWorldCamera& camera) const;
    [[nodiscard]] GridPushConstants gridPushConstantsFor(const Renderer2DWorldCamera& camera, const DebugGridRequest& request) const;

    static std::vector<char> readFile(const char* path);
    static VkVertexInputBindingDescription vertexBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions();

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_gridPipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_gridPipeline{VK_NULL_HANDLE};
    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vertexBufferMemory{VK_NULL_HANDLE};
    VkDeviceSize m_vertexBufferSize{0};
    glm::uvec2 m_viewportSize{1U, 1U};
    glm::mat4 m_viewProjection{1.0f};
    GridPushConstants m_gridPushConstants;
    DebugGridRequest m_gridRequest;
    std::size_t m_maxVertices{0};
    std::vector<DebugLineVertex> m_vertices;
};

} // namespace Engine
