#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <volk.h>

namespace Engine {

class Renderer2DWorld;

class VulkanRenderer2DWorld final {
public:
    struct Instance {
        glm::vec3 corner0{};
        glm::vec3 corner1{};
        glm::vec3 corner2{};
        glm::vec3 corner3{};
        glm::vec4 color{};
    };

    VulkanRenderer2DWorld(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, std::size_t maxInstances = 4096);
    ~VulkanRenderer2DWorld();

    VulkanRenderer2DWorld(const VulkanRenderer2DWorld&) = delete;
    VulkanRenderer2DWorld& operator=(const VulkanRenderer2DWorld&) = delete;
    VulkanRenderer2DWorld(VulkanRenderer2DWorld&&) = delete;
    VulkanRenderer2DWorld& operator=(VulkanRenderer2DWorld&&) = delete;

    void begin(const glm::uvec2& viewportSize);
    void submit(const Renderer2DWorld& worldRenderer);
    void end();
    void record(VkCommandBuffer commandBuffer) const;

    [[nodiscard]] std::size_t instanceCount() const;
    [[nodiscard]] std::size_t maxInstances() const;

private:
    void createPipeline(VkRenderPass renderPass);
    void createInstanceBuffer();
    void uploadInstances();
    void destroy();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& bytecode) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] glm::mat4 worldViewProjection(const Renderer2DWorld& worldRenderer) const;

    static std::vector<char> readFile(const char* path);
    static VkVertexInputBindingDescription instanceBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 5> instanceAttributeDescriptions();

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkBuffer m_instanceBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_instanceBufferMemory{VK_NULL_HANDLE};
    VkDeviceSize m_instanceBufferSize{0};
    glm::uvec2 m_viewportSize{1U, 1U};
    glm::mat4 m_viewProjection{1.0f};
    std::size_t m_maxInstances{0};
    std::vector<Instance> m_instances;
};

} // namespace Engine