#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <volk.h>

namespace Engine {

class SpriteRenderer;
class ResourceManager;
enum class WorldBlendMode;
enum class WorldSamplerMode;

// Vulkan backend for engine sprite/tilemap/particle world quads, not the UI
// primitive renderer.
class VulkanSpriteRenderer final {
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

    VulkanSpriteRenderer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkQueue graphicsQueue,
        VkCommandPool commandPool,
        VkRenderPass renderPass,
        std::size_t maxInstances = 4096);
    ~VulkanSpriteRenderer();

    VulkanSpriteRenderer(const VulkanSpriteRenderer&) = delete;
    VulkanSpriteRenderer& operator=(const VulkanSpriteRenderer&) = delete;
    VulkanSpriteRenderer(VulkanSpriteRenderer&&) = delete;
    VulkanSpriteRenderer& operator=(VulkanSpriteRenderer&&) = delete;

    void begin(const glm::uvec2& viewportSize);
    void submit(const SpriteRenderer& worldRenderer, ResourceManager& resources);
    void end();
    void record(VkCommandBuffer commandBuffer) const;

    [[nodiscard]] std::size_t instanceCount() const;
    [[nodiscard]] std::size_t maxInstances() const;
    [[nodiscard]] std::size_t drawCommandCount() const;

private:
    struct GpuTextureResource {
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView imageView{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    };

    struct DrawCommand {
        std::uint32_t firstInstance{0};
        std::uint32_t instanceCount{0};
        WorldBlendMode blendMode{};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    };

    struct TextureBatchKey {
        std::array<std::uint64_t, 16> textures{};
        WorldSamplerMode samplerMode{};

        [[nodiscard]] friend bool operator==(const TextureBatchKey& left, const TextureBatchKey& right)
        {
            return left.textures == right.textures && left.samplerMode == right.samplerMode;
        }
    };

    struct TextureBatchKeyHash {
        [[nodiscard]] std::size_t operator()(const TextureBatchKey& key) const noexcept;
    };

    std::vector<DrawCommand> m_drawCommands;
    void createDescriptorResources();
    void createFallbackTexture();
    void destroyFallbackTexture();
    void destroyTextureResource(GpuTextureResource& texture);
    void destroyUploadedTextures();
    [[nodiscard]] VkDescriptorSet descriptorSetForTextures(
        const std::vector<std::uint64_t>& textures,
        WorldSamplerMode samplerMode,
        ResourceManager& resources);
    [[nodiscard]] GpuTextureResource& textureResourceForTexture(std::uint64_t texture, WorldSamplerMode samplerMode, ResourceManager& resources);
    GpuTextureResource createTextureResource(
        const std::uint8_t* rgba8,
        std::uint32_t width,
        std::uint32_t height,
        WorldSamplerMode samplerMode);

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
    [[nodiscard]] glm::mat4 worldViewProjection(const SpriteRenderer& worldRenderer) const;
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
    void* m_instanceBufferMapped{nullptr};
    VkDeviceSize m_instanceBufferSize{0};
    glm::uvec2 m_viewportSize{1U, 1U};
    glm::mat4 m_viewProjection{1.0f};
    std::size_t m_maxInstances{0};
    std::vector<Instance> m_instances;

    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};

    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    GpuTextureResource m_fallbackTexture{};
    std::unordered_map<std::uint64_t, GpuTextureResource> m_uploadedTextures;
    std::unordered_map<TextureBatchKey, VkDescriptorSet, TextureBatchKeyHash> m_textureBatchDescriptors;
};

using VulkanRenderer2DWorld = VulkanSpriteRenderer;

} // namespace Engine
