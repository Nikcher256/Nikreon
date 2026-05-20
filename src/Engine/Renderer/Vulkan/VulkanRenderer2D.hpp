#pragma once

#include "Engine/Renderer/Renderer2D.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <volk.h>

namespace Engine {

class VulkanRenderer2D final : public Renderer2D {
public:
    struct Vertex {
        glm::vec2 position{};
        glm::vec4 color{};
    };

    VulkanRenderer2D(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, std::size_t maxQuads = 4096);
    ~VulkanRenderer2D() override;

    VulkanRenderer2D(const VulkanRenderer2D&) = delete;
    VulkanRenderer2D& operator=(const VulkanRenderer2D&) = delete;
    VulkanRenderer2D(VulkanRenderer2D&&) = delete;
    VulkanRenderer2D& operator=(VulkanRenderer2D&&) = delete;

    void begin(const glm::uvec2& viewportSize) override;
    void drawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) override;
    void drawRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float thickness = 1.0f) override;
    void end() override;
    void record(VkCommandBuffer commandBuffer);

    [[nodiscard]] std::size_t quadCount() const;
    [[nodiscard]] std::size_t maxQuads() const;

private:
    void createPipeline(VkRenderPass renderPass);
    void createVertexBuffer();
    void destroy();
    void uploadVertices();

    [[nodiscard]] glm::vec2 toNdc(const glm::vec2& pixelPosition) const;
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& bytecode) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    static std::vector<char> readFile(const char* path);
    static VkVertexInputBindingDescription vertexBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions();

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vertexBufferMemory{VK_NULL_HANDLE};
    VkDeviceSize m_vertexBufferSize{0};
    glm::uvec2 m_viewportSize{};
    std::size_t m_maxQuads{0};
    std::vector<Vertex> m_vertices;
};

} // namespace Engine
