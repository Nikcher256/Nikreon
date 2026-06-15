#include "Engine/Renderer/Vulkan/World2D/VulkanRenderer2DWorld.hpp"

#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/TextureResource.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Engine {

namespace {

constexpr std::uint32_t VerticesPerInstance = 6;
constexpr std::uint32_t WorldTextureSlotCount = 16;

#ifndef NIKREON_ENGINE_SHADER_DIR
#define NIKREON_ENGINE_SHADER_DIR "shaders"
#endif

void checkVk(const VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

std::uint64_t textureCacheKey(const std::uint64_t texture, const WorldSamplerMode samplerMode)
{
    return (texture << 1U) | (samplerMode == WorldSamplerMode::Nearest ? 1U : 0U);
}

} // namespace

std::size_t VulkanRenderer2DWorld::TextureBatchKeyHash::operator()(const TextureBatchKey& key) const noexcept
{
    std::size_t hash = 1469598103934665603ull;
    for (const std::uint64_t texture : key.textures) {
        hash ^= static_cast<std::size_t>(texture);
        hash *= 1099511628211ull;
    }
    hash ^= static_cast<std::size_t>(key.samplerMode);
    hash *= 1099511628211ull;
    return hash;
}

VulkanRenderer2DWorld::VulkanRenderer2DWorld(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkQueue graphicsQueue,
    VkCommandPool commandPool,
    VkRenderPass renderPass,
    std::size_t maxInstances)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_graphicsQueue(graphicsQueue)
    , m_commandPool(commandPool)
    , m_maxInstances(std::max(maxInstances, std::size_t{1}))
{
    m_instances.reserve(m_maxInstances);
    createInstanceBuffer();
    createDescriptorResources();
    createFallbackTexture();
    createPipelineLayout();
    createPipelines(renderPass);
}

VulkanRenderer2DWorld::~VulkanRenderer2DWorld()
{
    destroy();
}

void VulkanRenderer2DWorld::begin(const glm::uvec2& viewportSize)
{
    m_viewportSize = glm::max(viewportSize, glm::uvec2{1U, 1U});
    m_instances.clear();
    m_drawCommands.clear();
    m_viewProjection = glm::mat4{1.0f};
}

void VulkanRenderer2DWorld::submit(const Renderer2DWorld& worldRenderer, ResourceManager& resources)
{
    m_viewProjection = worldViewProjection(worldRenderer);

    const std::span<const WorldQuadVertex> vertices = worldRenderer.vertices();
    const std::size_t quadCount = vertices.size() / 4U;
    const std::size_t instanceCount = std::min(quadCount, m_maxInstances);

    m_instances.clear();
    m_instances.reserve(instanceCount);

    for (std::size_t index = 0; index < instanceCount; ++index) {
        const std::size_t first = index * 4U;
        m_instances.push_back({
           vertices[first + 0U].position, 
           vertices[first + 1U].position, 
           vertices[first + 2U].position, 
           vertices[first + 3U].position, 

           vertices[first + 0U].uv,
           vertices[first + 1U].uv,
           vertices[first + 2U].uv,
           vertices[first + 3U].uv,

           vertices[first + 0U].color, 
           vertices[first + 0U].textureIndex, 
        });
    }

    for (const auto& batch : worldRenderer.batches()) {
        if (batch.quadCount == 0 || batch.firstQuad >= m_instances.size()) {
            continue;
        }

        const auto availableInstances = m_instances.size() - batch.firstQuad;
        const auto batchInstanceCount = std::min(batch.quadCount, availableInstances);

        if (batchInstanceCount == 0) {
            continue;
        }

        m_drawCommands.push_back(DrawCommand{
            static_cast<std::uint32_t>(batch.firstQuad),
            static_cast<std::uint32_t>(batchInstanceCount),
            batch.key.blendMode,
            descriptorSetForTextures(batch.textures, batch.key.samplerMode, resources),
        });
    }
}

void VulkanRenderer2DWorld::end()
{
    uploadInstances();
}

void VulkanRenderer2DWorld::record(const VkCommandBuffer commandBuffer) const
{
    if (m_drawCommands.empty() || m_pipelineLayout == VK_NULL_HANDLE || m_instances.empty()) {
        return;
    }

    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_instanceBuffer, offsets);
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_viewportSize.x);
    viewport.height = static_cast<float>(m_viewportSize.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_viewportSize.x, m_viewportSize.y};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdPushConstants(
        commandBuffer,
        m_pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &m_viewProjection);

    VkPipeline boundPipeline = VK_NULL_HANDLE;
    for (const auto& command : m_drawCommands) {
        const VkPipeline pipeline = pipelineFor(command.blendMode);
        if (pipeline == VK_NULL_HANDLE) {
            continue;
        }

        if (pipeline != boundPipeline) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            boundPipeline = pipeline;
        }

        const VkDescriptorSet descriptorSet = command.descriptorSet != VK_NULL_HANDLE ? command.descriptorSet : m_fallbackTexture.descriptorSet;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            1,
            &descriptorSet,
            0,
            nullptr);

        vkCmdDraw(
            commandBuffer,
            VerticesPerInstance,
            command.instanceCount,
            0,
            command.firstInstance);
    }
}

std::size_t VulkanRenderer2DWorld::instanceCount() const
{
    return m_instances.size();
}

std::size_t VulkanRenderer2DWorld::drawCommandCount() const
{
    return m_drawCommands.size();
}

std::size_t VulkanRenderer2DWorld::maxInstances() const
{
    return m_maxInstances;
}

void VulkanRenderer2DWorld::createPipelineLayout()
{
    VkPushConstantRange pushConstants{};
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstants.offset = 0;
    pushConstants.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstants;

    checkVk(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create VulkanRenderer2DWorld pipeline layout.");
}

void VulkanRenderer2DWorld::createPipelines(const VkRenderPass renderPass)
{
    createPipeline(renderPass, WorldBlendMode::Opaque, m_opaquePipeline);
    createPipeline(renderPass, WorldBlendMode::Alpha, m_alphaPipeline);
    createPipeline(renderPass, WorldBlendMode::Additive, m_additivePipeline);
}

void VulkanRenderer2DWorld::createPipeline(
    const VkRenderPass renderPass,
    const WorldBlendMode blendMode,
    VkPipeline& pipeline)
{
    const auto vertexShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/world2d_instanced.vert.spv");
    const auto fragmentShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/world2d_instanced.frag.spv");

    const VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    const VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexStage{};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexShaderModule;
    vertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentShaderModule;
    fragmentStage.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertexStage, fragmentStage};

    const auto bindingDescription = instanceBindingDescription();
    const auto attributeDescriptions = instanceAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

const VkPipelineColorBlendAttachmentState colorBlendAttachment = blendAttachmentFor(blendMode);

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    checkVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "Failed to create VulkanRenderer2DWorld pipeline.");

    vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
}

VkPipeline VulkanRenderer2DWorld::pipelineFor(const WorldBlendMode blendMode) const
{
    switch (blendMode) {
    case WorldBlendMode::Opaque:
        return m_opaquePipeline;
    case WorldBlendMode::Alpha:
        return m_alphaPipeline;
    case WorldBlendMode::Additive:
        return m_additivePipeline;
    }

    return m_alphaPipeline;
}

VkPipelineColorBlendAttachmentState VulkanRenderer2DWorld::blendAttachmentFor(const WorldBlendMode blendMode)
{
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    switch (blendMode) {
    case WorldBlendMode::Opaque:
        attachment.blendEnable = VK_FALSE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        break;

    case WorldBlendMode::Alpha:
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        break;

    case WorldBlendMode::Additive:
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    }

    return attachment;
}

void VulkanRenderer2DWorld::destroyPipelines()
{
    if (m_opaquePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_opaquePipeline, nullptr);
        m_opaquePipeline = VK_NULL_HANDLE;
    }

    if (m_alphaPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_alphaPipeline, nullptr);
        m_alphaPipeline = VK_NULL_HANDLE;
    }

    if (m_additivePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_additivePipeline, nullptr);
        m_additivePipeline = VK_NULL_HANDLE;
    }
}

void VulkanRenderer2DWorld::createInstanceBuffer()
{
    m_instanceBufferSize = static_cast<VkDeviceSize>(m_maxInstances * sizeof(Instance));
    createBuffer(
        m_instanceBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_instanceBuffer,
        m_instanceBufferMemory);
    checkVk(vkMapMemory(m_device, m_instanceBufferMemory, 0, m_instanceBufferSize, 0, &m_instanceBufferMapped), "Failed to map VulkanRenderer2DWorld instance buffer.");
}

void VulkanRenderer2DWorld::uploadInstances()
{
    if (m_instanceBufferMapped == nullptr || m_instances.empty()) {
        return;
    }

    std::memcpy(m_instanceBufferMapped, m_instances.data(), m_instances.size() * sizeof(Instance));
}

void VulkanRenderer2DWorld::createDescriptorResources()
{
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = WorldTextureSlotCount;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &textureBinding;
    checkVk(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout), "Failed to create VulkanRenderer2DWorld descriptor set layout.");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 512U * WorldTextureSlotCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 512U;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    checkVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "Failed to create VulkanRenderer2DWorld descriptor pool.");
}

void VulkanRenderer2DWorld::createFallbackTexture()
{
    const std::array<std::uint8_t, 4> whitePixel = {255U, 255U, 255U, 255U};
    m_fallbackTexture = createTextureResource(whitePixel.data(), 1U, 1U, WorldSamplerMode::Linear);
}

VkDescriptorSet VulkanRenderer2DWorld::descriptorSetForTextures(
    const std::vector<std::uint64_t>& textures,
    const WorldSamplerMode samplerMode,
    ResourceManager& resources)
{
    TextureBatchKey key{};
    key.samplerMode = samplerMode;
    const std::size_t textureCount = std::min(textures.size(), key.textures.size());
    for (std::size_t index = 0; index < textureCount; ++index) {
        key.textures[index] = textures[index];
    }

    if (const auto found = m_textureBatchDescriptors.find(key); found != m_textureBatchDescriptors.end()) {
        return found->second;
    }

    std::array<VkDescriptorImageInfo, WorldTextureSlotCount> imageInfos{};
    for (std::size_t index = 0; index < imageInfos.size(); ++index) {
        GpuTextureResource& texture = textureResourceForTexture(key.textures[index], samplerMode, resources);
        imageInfos[index] = {
            texture.sampler,
            texture.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_descriptorSetLayout;
    checkVk(vkAllocateDescriptorSets(m_device, &allocateInfo, &descriptorSet), "Failed to allocate VulkanRenderer2DWorld texture batch descriptor set.");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = static_cast<std::uint32_t>(imageInfos.size());
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = imageInfos.data();
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    m_textureBatchDescriptors.emplace(key, descriptorSet);
    return descriptorSet;
}

VulkanRenderer2DWorld::GpuTextureResource& VulkanRenderer2DWorld::textureResourceForTexture(
    const std::uint64_t texture,
    const WorldSamplerMode samplerMode,
    ResourceManager& resources)
{
    if (texture == 0U) {
        return m_fallbackTexture;
    }

    const std::uint64_t key = textureCacheKey(texture, samplerMode);
    if (const auto found = m_uploadedTextures.find(key); found != m_uploadedTextures.end()) {
        return found->second;
    }

    const TextureResource* resource = resources.tryTexture(TextureHandle::fromValue(texture));
    if (resource == nullptr || resource->pixels.rgba8.empty()) {
        return m_fallbackTexture;
    }

    auto [inserted, _] = m_uploadedTextures.emplace(
        key,
        createTextureResource(resource->pixels.rgba8.data(), resource->pixels.width, resource->pixels.height, samplerMode));
    return inserted->second.imageView != VK_NULL_HANDLE && inserted->second.sampler != VK_NULL_HANDLE
        ? inserted->second
        : m_fallbackTexture;
}

VulkanRenderer2DWorld::GpuTextureResource VulkanRenderer2DWorld::createTextureResource(
    const std::uint8_t* rgba8,
    const std::uint32_t width,
    const std::uint32_t height,
    const WorldSamplerMode samplerMode)
{
    if (rgba8 == nullptr || width == 0U || height == 0U) {
        return {};
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    void* mapped = nullptr;
    checkVk(vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &mapped), "Failed to map VulkanRenderer2DWorld texture staging memory.");
    std::memcpy(mapped, rgba8, static_cast<std::size_t>(imageSize));
    vkUnmapMemory(m_device, stagingMemory);

    GpuTextureResource texture;
    createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, texture.image, texture.memory);
    transitionImageLayout(texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture.image, width, height);
    transitionImageLayout(texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(m_device, &viewInfo, nullptr, &texture.imageView), "Failed to create VulkanRenderer2DWorld texture image view.");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    const VkFilter filter = samplerMode == WorldSamplerMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    checkVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &texture.sampler), "Failed to create VulkanRenderer2DWorld texture sampler.");

    return texture;
}

void VulkanRenderer2DWorld::createImage(
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    VkImage& image,
    VkDeviceMemory& memory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    checkVk(vkCreateImage(m_device, &imageInfo, nullptr, &image), "Failed to create VulkanRenderer2DWorld image.");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(m_device, image, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(m_device, &allocateInfo, nullptr, &memory), "Failed to allocate VulkanRenderer2DWorld image memory.");
    checkVk(vkBindImageMemory(m_device, image, memory, 0), "Failed to bind VulkanRenderer2DWorld image memory.");
}

void VulkanRenderer2DWorld::transitionImageLayout(
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout)
{
    const VkCommandBuffer commandBuffer = beginSingleUseCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported VulkanRenderer2DWorld image layout transition.");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleUseCommands(commandBuffer);
}

void VulkanRenderer2DWorld::copyBufferToImage(
    const VkBuffer buffer,
    const VkImage image,
    const std::uint32_t width,
    const std::uint32_t height)
{
    const VkCommandBuffer commandBuffer = beginSingleUseCommands();

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleUseCommands(commandBuffer);
}

VkCommandBuffer VulkanRenderer2DWorld::beginSingleUseCommands() const
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    checkVk(vkAllocateCommandBuffers(m_device, &allocateInfo, &commandBuffer), "Failed to allocate VulkanRenderer2DWorld upload command buffer.");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin VulkanRenderer2DWorld upload command buffer.");

    return commandBuffer;
}

void VulkanRenderer2DWorld::endSingleUseCommands(const VkCommandBuffer commandBuffer) const
{
    checkVk(vkEndCommandBuffer(commandBuffer), "Failed to end VulkanRenderer2DWorld upload command buffer.");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    checkVk(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit VulkanRenderer2DWorld upload command buffer.");
    checkVk(vkQueueWaitIdle(m_graphicsQueue), "Failed to wait for VulkanRenderer2DWorld upload command buffer.");

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void VulkanRenderer2DWorld::destroy()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    destroyUploadedTextures();
    destroyFallbackTexture();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_instanceBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
        m_instanceBuffer = VK_NULL_HANDLE;
    }

    if (m_instanceBufferMemory != VK_NULL_HANDLE) {
        if (m_instanceBufferMapped != nullptr) {
            vkUnmapMemory(m_device, m_instanceBufferMemory);
            m_instanceBufferMapped = nullptr;
        }
        vkFreeMemory(m_device, m_instanceBufferMemory, nullptr);
        m_instanceBufferMemory = VK_NULL_HANDLE;
    }

    destroyPipelines();

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanRenderer2DWorld::destroyFallbackTexture()
{
    destroyTextureResource(m_fallbackTexture);
}

void VulkanRenderer2DWorld::destroyUploadedTextures()
{
    m_textureBatchDescriptors.clear();
    for (auto& [_, texture] : m_uploadedTextures) {
        destroyTextureResource(texture);
    }
    m_uploadedTextures.clear();
}

void VulkanRenderer2DWorld::destroyTextureResource(GpuTextureResource& texture)
{
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }

    if (texture.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, texture.imageView, nullptr);
        texture.imageView = VK_NULL_HANDLE;
    }

    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }

    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }

    texture.descriptorSet = VK_NULL_HANDLE;
}

void VulkanRenderer2DWorld::createBuffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    checkVk(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "Failed to create VulkanRenderer2DWorld buffer.");

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    checkVk(vkAllocateMemory(m_device, &allocateInfo, nullptr, &memory), "Failed to allocate VulkanRenderer2DWorld buffer memory.");
    checkVk(vkBindBufferMemory(m_device, buffer, memory, 0), "Failed to bind VulkanRenderer2DWorld buffer memory.");
}

VkShaderModule VulkanRenderer2DWorld::createShaderModule(const std::vector<char>& bytecode) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytecode.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule), "Failed to create VulkanRenderer2DWorld shader module.");
    return shaderModule;
}

std::uint32_t VulkanRenderer2DWorld::findMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0U &&
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }

    throw std::runtime_error("Failed to find VulkanRenderer2DWorld memory type.");
}

glm::mat4 VulkanRenderer2DWorld::worldViewProjection(const Renderer2DWorld& worldRenderer) const
{
    const Renderer2DWorldCamera& camera = worldRenderer.camera();
    if (camera.mode == Renderer2DWorldCameraMode::Perspective3D) {
        return camera.camera3D.viewProjection();
    }

    return camera.camera2D.viewProjection();
}

std::vector<char> VulkanRenderer2DWorld::readFile(const char* path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open VulkanRenderer2DWorld shader file: ") + path);
    }

    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

VkVertexInputBindingDescription VulkanRenderer2DWorld::instanceBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Instance);
    binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 10> VulkanRenderer2DWorld::instanceAttributeDescriptions()
{
    return {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Instance, corner0)},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Instance, corner1)},
        VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Instance, corner2)},
        VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Instance, corner3)},

        VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, uv0)},
        VkVertexInputAttributeDescription{5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, uv1)},
        VkVertexInputAttributeDescription{6, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, uv2)},
        VkVertexInputAttributeDescription{7, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, uv3)},

        VkVertexInputAttributeDescription{8, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, color)},
        VkVertexInputAttributeDescription{9, 0, VK_FORMAT_R32_SFLOAT, offsetof(Instance, textureIndex)},
    };
}

}// namespace Engine
