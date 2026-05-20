#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Engine {

namespace {

constexpr std::size_t VerticesPerQuad = 6;

#ifndef NIKREON_SHADER_DIR
#define NIKREON_SHADER_DIR "shaders"
#endif

void checkVk(const VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

} // namespace

VulkanRenderer2D::VulkanRenderer2D(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkRenderPass renderPass,
    const std::size_t maxQuads)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_maxQuads(maxQuads)
{
    m_vertices.reserve(m_maxQuads * VerticesPerQuad);
    createVertexBuffer();
    createPipeline(renderPass);
}

VulkanRenderer2D::~VulkanRenderer2D()
{
    destroy();
}

void VulkanRenderer2D::begin(const glm::uvec2& viewportSize)
{
    m_viewportSize = viewportSize;
    m_vertices.clear();
}

void VulkanRenderer2D::drawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    if (m_vertices.size() + VerticesPerQuad > m_maxQuads * VerticesPerQuad) {
        return;
    }

    const glm::vec2 p0 = toNdc(position);
    const glm::vec2 p1 = toNdc({position.x + size.x, position.y});
    const glm::vec2 p2 = toNdc({position.x + size.x, position.y + size.y});
    const glm::vec2 p3 = toNdc({position.x, position.y + size.y});

    m_vertices.push_back({p0, color});
    m_vertices.push_back({p1, color});
    m_vertices.push_back({p2, color});
    m_vertices.push_back({p2, color});
    m_vertices.push_back({p3, color});
    m_vertices.push_back({p0, color});
}

void VulkanRenderer2D::drawRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, const float thickness)
{
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const float line = std::max(thickness, 1.0f);
    drawQuad(position, {size.x, line}, color);
    drawQuad({position.x, position.y + size.y - line}, {size.x, line}, color);
    drawQuad(position, {line, size.y}, color);
    drawQuad({position.x + size.x - line, position.y}, {line, size.y}, color);
}

void VulkanRenderer2D::end()
{
    uploadVertices();
}

void VulkanRenderer2D::record(const VkCommandBuffer commandBuffer)
{
    if (m_vertices.empty()) {
        return;
    }

    const VkDeviceSize offsets[] = {0};
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_viewportSize.x);
    viewport.height = static_cast<float>(m_viewportSize.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_viewportSize.x, m_viewportSize.y};

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(m_vertices.size()), 1, 0, 0);
}

std::size_t VulkanRenderer2D::quadCount() const
{
    return m_vertices.size() / VerticesPerQuad;
}

std::size_t VulkanRenderer2D::maxQuads() const
{
    return m_maxQuads;
}

void VulkanRenderer2D::createPipeline(const VkRenderPass renderPass)
{
    const auto vertexShaderCode = readFile(NIKREON_SHADER_DIR "/renderer2d_quad.vert.spv");
    const auto fragmentShaderCode = readFile(NIKREON_SHADER_DIR "/renderer2d_quad.frag.spv");

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
    const auto bindingDescription = vertexBindingDescription();
    const auto attributeDescriptions = vertexAttributeDescriptions();

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

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    checkVk(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create Renderer2D pipeline layout.");

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

    checkVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create Renderer2D pipeline.");

    vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
}

void VulkanRenderer2D::createVertexBuffer()
{
    m_vertexBufferSize = static_cast<VkDeviceSize>(m_maxQuads * VerticesPerQuad * sizeof(Vertex));

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_vertexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    checkVk(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_vertexBuffer), "Failed to create Renderer2D vertex buffer.");

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    checkVk(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_vertexBufferMemory), "Failed to allocate Renderer2D vertex buffer memory.");
    checkVk(vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexBufferMemory, 0), "Failed to bind Renderer2D vertex buffer memory.");
}

void VulkanRenderer2D::destroy()
{
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }

    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanRenderer2D::uploadVertices()
{
    if (m_vertices.empty()) {
        return;
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(m_vertices.size() * sizeof(Vertex));
    if (uploadSize > m_vertexBufferSize) {
        throw std::runtime_error("Renderer2D vertex upload exceeded vertex buffer capacity.");
    }

    void* mappedMemory = nullptr;
    checkVk(vkMapMemory(m_device, m_vertexBufferMemory, 0, uploadSize, 0, &mappedMemory), "Failed to map Renderer2D vertex buffer memory.");
    std::memcpy(mappedMemory, m_vertices.data(), static_cast<std::size_t>(uploadSize));
    vkUnmapMemory(m_device, m_vertexBufferMemory);
}

glm::vec2 VulkanRenderer2D::toNdc(const glm::vec2& pixelPosition) const
{
    const float width = static_cast<float>(std::max(m_viewportSize.x, 1U));
    const float height = static_cast<float>(std::max(m_viewportSize.y, 1U));

    return {
        (pixelPosition.x / width) * 2.0f - 1.0f,
        1.0f - (pixelPosition.y / height) * 2.0f,
    };
}

VkShaderModule VulkanRenderer2D::createShaderModule(const std::vector<char>& bytecode) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytecode.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule), "Failed to create shader module.");
    return shaderModule;
}

std::uint32_t VulkanRenderer2D::findMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0 &&
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type.");
}

std::vector<char> VulkanRenderer2D::readFile(const char* path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open shader file: ") + path);
    }

    const auto fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkVertexInputBindingDescription VulkanRenderer2D::vertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> VulkanRenderer2D::vertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
}

} // namespace Engine
