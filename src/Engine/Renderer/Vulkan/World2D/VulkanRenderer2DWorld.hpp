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
enum class WorldBlendMode;

class VulkanRenderer2DWorld final {
public:
    struct Instance {
        glm::vec3 corner0{};
        glm::vec3 corner1{};
        glm::vec3 corner2{};
        glm::vec3 corner3{};

        glm::vec2 uv0{};
        glm::vec2 uv1{};
        glm::vec2 uv2{};
        glm::vec2 uv3{};

        glm::vec4 color{};
        float textureIndex{0.0f};
    };

    VulkanRenderer2DWorld(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkQueue graphicsQueue,
        VkCommandPool commandPool,
        VkRenderPass renderPass,
        std::size_t maxInstances = 4096);
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
    [[nodiscard]] std::size_t drawCommandCount() const;

private:
    struct TextureResource {
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView imageView{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
    };

    struct DrawCommand {
        std::uint32_t firstInstance{0};
        std::uint32_t instanceCount{0};
        WorldBlendMode blendMode{};
    };

    std::vector<DrawCommand> m_drawCommands;
    void createDescriptorResources();
    void createFallbackTexture();
    void destroyFallbackTexture();

    void createImage(std::uint32_t width, std::uint32_t height, VkFormat format, VkImage& image, VkDeviceMemory& memory);
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height);
    [[nodiscard]] VkCommandBuffer beginSingleUseCommands() const;
    void endSingleUseCommands(VkCommandBuffer commandBuffer) const;

    void createPipelineLayout();
    void createPipelines(VkRenderPass renderPass);
    void createPipeline(VkRenderPass renderPass, WorldBlendMode blendMode, VkPipeline& pipeline);
    void destroyPipelines();
    void createInstanceBuffer();
    void uploadInstances();
    void destroy();

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& memory);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& bytecode) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] glm::mat4 worldViewProjection(const Renderer2DWorld& worldRenderer) const;
    [[nodiscard]] VkPipeline pipelineFor(WorldBlendMode blendMode) const;

    static VkPipelineColorBlendAttachmentState blendAttachmentFor(WorldBlendMode blendMode);
    static std::vector<char> readFile(const char* path);
    static VkVertexInputBindingDescription instanceBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 10> instanceAttributeDescriptions();

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_opaquePipeline{VK_NULL_HANDLE};
    VkPipeline m_alphaPipeline{VK_NULL_HANDLE};
    VkPipeline m_additivePipeline{VK_NULL_HANDLE};
    VkBuffer m_instanceBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_instanceBufferMemory{VK_NULL_HANDLE};
    VkDeviceSize m_instanceBufferSize{0};
    glm::uvec2 m_viewportSize{1U, 1U};
    glm::mat4 m_viewProjection{1.0f};
    std::size_t m_maxInstances{0};
    std::vector<Instance> m_instances;

    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};

    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_fallbackDescriptorSet{VK_NULL_HANDLE};
    TextureResource m_fallbackTexture{};
};

} // namespace Engine