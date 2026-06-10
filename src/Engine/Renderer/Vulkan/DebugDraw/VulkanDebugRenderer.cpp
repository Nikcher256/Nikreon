#include "Engine/Renderer/Vulkan/DebugDraw/VulkanDebugRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include <glm/common.hpp>
#include <glm/matrix.hpp>

namespace Engine {

namespace {

#ifndef NIKREON_ENGINE_SHADER_DIR
#define NIKREON_ENGINE_SHADER_DIR "shaders"
#endif

void checkVk(const VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

} // namespace

VulkanDebugRenderer::VulkanDebugRenderer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkRenderPass renderPass,
    std::size_t maxVertices)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_maxVertices(std::max(maxVertices, std::size_t{2}))
{
    m_vertices.reserve(m_maxVertices);
    createVertexBuffer();
    createPipelineLayout();
    createPipeline(renderPass);
}

VulkanDebugRenderer::~VulkanDebugRenderer()
{
    destroy();
}

void VulkanDebugRenderer::begin(const glm::uvec2& viewportSize)
{
    m_viewportSize = glm::max(viewportSize, glm::uvec2{1U, 1U});
    m_viewProjection = glm::mat4{1.0f};
    m_gridPushConstants = {};
    m_gridRequest = {};
    m_vertices.clear();
}

void VulkanDebugRenderer::submit(const DebugRenderer& debugRenderer, const Renderer2DWorldCamera& camera)
{
    m_viewProjection = viewProjectionFor(camera);
    m_gridRequest = debugRenderer.gridRequest();
    m_gridPushConstants = gridPushConstantsFor(camera, m_gridRequest);

    const std::span<const DebugLineVertex> lineVertices = debugRenderer.lineVertices();
    const std::size_t vertexCount = std::min(lineVertices.size(), m_maxVertices);
    m_vertices.assign(lineVertices.begin(), lineVertices.begin() + static_cast<std::ptrdiff_t>(vertexCount));

    if ((m_vertices.size() % 2U) != 0U) {
        m_vertices.pop_back();
    }
}

void VulkanDebugRenderer::end()
{
    uploadVertices();
}

void VulkanDebugRenderer::record(const VkCommandBuffer commandBuffer) const
{
    if ((!m_gridRequest.enabled && m_vertices.empty()) ||
        m_pipeline == VK_NULL_HANDLE ||
        m_pipelineLayout == VK_NULL_HANDLE) {
        return;
    }

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

    recordGrid(commandBuffer);
    recordLines(commandBuffer);
}

void VulkanDebugRenderer::recordGrid(const VkCommandBuffer commandBuffer) const
{
    if (!m_gridRequest.enabled || m_gridPipeline == VK_NULL_HANDLE || m_gridPipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);
    vkCmdPushConstants(
        commandBuffer,
        m_gridPipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(GridPushConstants),
        &m_gridPushConstants);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void VulkanDebugRenderer::recordLines(const VkCommandBuffer commandBuffer) const
{
    if (m_vertices.empty() || m_pipeline == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);

    vkCmdPushConstants(
        commandBuffer,
        m_pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &m_viewProjection);

    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(m_vertices.size()), 1, 0, 0);
}

std::size_t VulkanDebugRenderer::vertexCount() const
{
    return m_vertices.size();
}

void VulkanDebugRenderer::createPipelineLayout()
{
    VkPushConstantRange linePushConstants{};
    linePushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    linePushConstants.offset = 0;
    linePushConstants.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &linePushConstants;

    checkVk(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create VulkanDebugRenderer pipeline layout.");

    VkPushConstantRange gridPushConstants{};
    gridPushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    gridPushConstants.offset = 0;
    gridPushConstants.size = sizeof(GridPushConstants);

    VkPipelineLayoutCreateInfo gridPipelineLayoutInfo{};
    gridPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    gridPipelineLayoutInfo.pushConstantRangeCount = 1;
    gridPipelineLayoutInfo.pPushConstantRanges = &gridPushConstants;

    checkVk(vkCreatePipelineLayout(m_device, &gridPipelineLayoutInfo, nullptr, &m_gridPipelineLayout), "Failed to create VulkanDebugRenderer grid pipeline layout.");
}

void VulkanDebugRenderer::createPipeline(const VkRenderPass renderPass)
{
    const auto vertexShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/debug_line.vert.spv");
    const auto fragmentShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/debug_line.frag.spv");

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
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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

    checkVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create VulkanDebugRenderer pipeline.");

    vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);

    const auto gridVertexShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/debug_grid.vert.spv");
    const auto gridFragmentShaderCode = readFile(NIKREON_ENGINE_SHADER_DIR "/debug_grid.frag.spv");

    const VkShaderModule gridVertexShaderModule = createShaderModule(gridVertexShaderCode);
    const VkShaderModule gridFragmentShaderModule = createShaderModule(gridFragmentShaderCode);

    VkPipelineShaderStageCreateInfo gridVertexStage{};
    gridVertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    gridVertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    gridVertexStage.module = gridVertexShaderModule;
    gridVertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo gridFragmentStage{};
    gridFragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    gridFragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    gridFragmentStage.module = gridFragmentShaderModule;
    gridFragmentStage.pName = "main";

    const VkPipelineShaderStageCreateInfo gridShaderStages[] = {gridVertexStage, gridFragmentStage};

    VkPipelineVertexInputStateCreateInfo gridVertexInput{};
    gridVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo gridInputAssembly{};
    gridInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    gridInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkGraphicsPipelineCreateInfo gridPipelineInfo{};
    gridPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gridPipelineInfo.stageCount = 2;
    gridPipelineInfo.pStages = gridShaderStages;
    gridPipelineInfo.pVertexInputState = &gridVertexInput;
    gridPipelineInfo.pInputAssemblyState = &gridInputAssembly;
    gridPipelineInfo.pViewportState = &viewportState;
    gridPipelineInfo.pRasterizationState = &rasterizer;
    gridPipelineInfo.pMultisampleState = &multisampling;
    gridPipelineInfo.pColorBlendState = &colorBlending;
    gridPipelineInfo.pDynamicState = &dynamicState;
    gridPipelineInfo.layout = m_gridPipelineLayout;
    gridPipelineInfo.renderPass = renderPass;
    gridPipelineInfo.subpass = 0;

    checkVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gridPipelineInfo, nullptr, &m_gridPipeline), "Failed to create VulkanDebugRenderer grid pipeline.");

    vkDestroyShaderModule(m_device, gridFragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_device, gridVertexShaderModule, nullptr);
}

void VulkanDebugRenderer::createVertexBuffer()
{
    m_vertexBufferSize = static_cast<VkDeviceSize>(m_maxVertices * sizeof(DebugLineVertex));
    createBuffer(
        m_vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_vertexBuffer,
        m_vertexBufferMemory);
}

void VulkanDebugRenderer::uploadVertices()
{
    if (m_vertexBufferMemory == VK_NULL_HANDLE || m_vertices.empty()) {
        return;
    }

    void* mapped = nullptr;
    checkVk(vkMapMemory(m_device, m_vertexBufferMemory, 0, m_vertexBufferSize, 0, &mapped), "Failed to map VulkanDebugRenderer vertex buffer.");
    std::memcpy(mapped, m_vertices.data(), m_vertices.size() * sizeof(DebugLineVertex));
    vkUnmapMemory(m_device, m_vertexBufferMemory);
}

void VulkanDebugRenderer::destroy()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

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

    if (m_gridPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_gridPipeline, nullptr);
        m_gridPipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_gridPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_gridPipelineLayout, nullptr);
        m_gridPipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanDebugRenderer::createBuffer(
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

    checkVk(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "Failed to create VulkanDebugRenderer buffer.");

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    checkVk(vkAllocateMemory(m_device, &allocateInfo, nullptr, &memory), "Failed to allocate VulkanDebugRenderer buffer memory.");
    checkVk(vkBindBufferMemory(m_device, buffer, memory, 0), "Failed to bind VulkanDebugRenderer buffer memory.");
}

VkShaderModule VulkanDebugRenderer::createShaderModule(const std::vector<char>& bytecode) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytecode.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule), "Failed to create VulkanDebugRenderer shader module.");
    return shaderModule;
}

std::uint32_t VulkanDebugRenderer::findMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0U &&
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }

    throw std::runtime_error("Failed to find VulkanDebugRenderer memory type.");
}

glm::mat4 VulkanDebugRenderer::viewProjectionFor(const Renderer2DWorldCamera& camera) const
{
    if (camera.mode == Renderer2DWorldCameraMode::Perspective3D) {
        return camera.camera3D.viewProjection();
    }

    return camera.camera2D.viewProjection();
}

VulkanDebugRenderer::GridPushConstants VulkanDebugRenderer::gridPushConstantsFor(
    const Renderer2DWorldCamera& camera,
    const DebugGridRequest& request) const
{
    GridPushConstants pushConstants;
    const glm::mat4 viewProjection = viewProjectionFor(camera);
    pushConstants.inverseViewProjection = glm::inverse(viewProjection);
    pushConstants.gridSettings = {
        std::max(request.step, 0.001f),
        std::max(request.majorEvery, 1.0f),
        static_cast<float>(std::max(m_viewportSize.x, 1U)),
        static_cast<float>(std::max(m_viewportSize.y, 1U)),
    };

    if (camera.mode == Renderer2DWorldCameraMode::Perspective3D) {
        pushConstants.cameraPosition = {
            camera.camera3D.position.x,
            camera.camera3D.position.y,
            camera.camera3D.position.z,
            1.0f,
        };
        pushConstants.fadeSettings = {
            request.fadeStart,
            request.fadeEnd,
            0.0f,
            0.0f,
        };
        return pushConstants;
    }

    pushConstants.cameraPosition = {
        camera.camera2D.position.x,
        camera.camera2D.position.y,
        0.0f,
        1.0f,
    };
    pushConstants.fadeSettings = {
        100000.0f,
        100001.0f,
        0.0f,
        0.0f,
    };
    return pushConstants;
}

std::vector<char> VulkanDebugRenderer::readFile(const char* path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open VulkanDebugRenderer shader file: ") + path);
    }

    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

VkVertexInputBindingDescription VulkanDebugRenderer::vertexBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(DebugLineVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 2> VulkanDebugRenderer::vertexAttributeDescriptions()
{
    return {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DebugLineVertex, position)},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(DebugLineVertex, color)},
    };
}

} // namespace Engine
