#include "Engine/Renderer/Vulkan/VulkanViewportRenderTarget.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace Engine {

namespace {

void checkVk(const VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

void transitionImage(
    const VkCommandBuffer commandBuffer,
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const VkAccessFlags sourceAccess,
    const VkAccessFlags destinationAccess,
    const VkPipelineStageFlags sourceStage,
    const VkPipelineStageFlags destinationStage,
    const VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkImageSubresourceRange colorRange()
{
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    return range;
}

VkImageSubresourceRange depthRange()
{
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    return range;
}

} // namespace

VulkanViewportRenderTarget::VulkanViewportRenderTarget(
    const VkDevice device,
    const VkPhysicalDevice physicalDevice,
    const VkFormat format)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_format(format)
{
    m_depthFormat = findDepthFormat();
    createRenderPass();
    create();
}

VulkanViewportRenderTarget::~VulkanViewportRenderTarget()
{
    destroy();
    destroyRenderPass();
}

void VulkanViewportRenderTarget::resize(const glm::uvec2& size)
{
    const glm::uvec2 clampedSize{std::max(size.x, 1U), std::max(size.y, 1U)};
    if (clampedSize == m_size) {
        return;
    }

    destroy();
    m_size = clampedSize;
    create();
}

void VulkanViewportRenderTarget::recordClear(const VkCommandBuffer commandBuffer, const glm::vec4& clearColor) const
{
    transitionImage(
        commandBuffer,
        m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkClearColorValue color{};
    color.float32[0] = clearColor.r;
    color.float32[1] = clearColor.g;
    color.float32[2] = clearColor.b;
    color.float32[3] = clearColor.a;
    const VkImageSubresourceRange range = colorRange();
    vkCmdClearColorImage(commandBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    transitionImage(
        commandBuffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    transitionImage(
        commandBuffer,
        m_depthImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    const VkClearDepthStencilValue depthClear{1.0f, 0U};
    const VkImageSubresourceRange depthSubresource = depthRange();
    vkCmdClearDepthStencilImage(commandBuffer, m_depthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthClear, 1, &depthSubresource);

    transitionImage(
        commandBuffer,
        m_depthImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanViewportRenderTarget::recordBeginRenderPass(const VkCommandBuffer commandBuffer, const VkSubpassContents contents) const
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_size.x, m_size.y};
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, contents);
}

void VulkanViewportRenderTarget::recordEndRenderPass(const VkCommandBuffer commandBuffer) const
{
    vkCmdEndRenderPass(commandBuffer);
}

void VulkanViewportRenderTarget::recordCopyTo(
    const VkCommandBuffer commandBuffer,
    const VkImage destinationImage,
    const glm::ivec2& destinationPosition,
    const glm::uvec2& destinationSize) const
{
    transitionImage(
        commandBuffer,
        m_image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
    transitionImage(
        commandBuffer,
        destinationImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkClearColorValue windowBackground{};
    windowBackground.float32[0] = 0.08f;
    windowBackground.float32[1] = 0.09f;
    windowBackground.float32[2] = 0.12f;
    windowBackground.float32[3] = 1.0f;
    const VkImageSubresourceRange range = colorRange();
    vkCmdClearColorImage(commandBuffer, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &windowBackground, 1, &range);

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount = 1;
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.layerCount = 1;
    copy.dstOffset = {destinationPosition.x, destinationPosition.y, 0};
    copy.extent = {destinationSize.x, destinationSize.y, 1};
    vkCmdCopyImage(commandBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transitionImage(
        commandBuffer,
        destinationImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
}

glm::uvec2 VulkanViewportRenderTarget::size() const
{
    return m_size;
}

VkRenderPass VulkanViewportRenderTarget::renderPass() const
{
    return m_renderPass;
}

VkFramebuffer VulkanViewportRenderTarget::framebuffer() const
{
    return m_framebuffer;
}

void VulkanViewportRenderTarget::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments = {
        colorAttachment,
        depthAttachment,
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    checkVk(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "Failed to create viewport render target render pass.");
}

void VulkanViewportRenderTarget::create()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {m_size.x, m_size.y, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateImage(m_device, &imageInfo, nullptr, &m_image), "Failed to create viewport render target image.");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(m_device, m_image, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory), "Failed to allocate viewport render target memory.");
    checkVk(vkBindImageMemory(m_device, m_image, m_memory, 0), "Failed to bind viewport render target memory.");

    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = m_image;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = m_format;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_imageView), "Failed to create viewport render target image view.");

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.extent = {m_size.x, m_size.y, 1};
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.format = m_depthFormat;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateImage(m_device, &depthImageInfo, nullptr, &m_depthImage), "Failed to create viewport render target depth image.");

    VkMemoryRequirements depthRequirements{};
    vkGetImageMemoryRequirements(m_device, m_depthImage, &depthRequirements);

    VkMemoryAllocateInfo depthAllocateInfo{};
    depthAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthAllocateInfo.allocationSize = depthRequirements.size;
    depthAllocateInfo.memoryTypeIndex = findMemoryType(depthRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(m_device, &depthAllocateInfo, nullptr, &m_depthMemory), "Failed to allocate viewport render target depth memory.");
    checkVk(vkBindImageMemory(m_device, m_depthImage, m_depthMemory, 0), "Failed to bind viewport render target depth memory.");

    VkImageViewCreateInfo depthImageViewInfo{};
    depthImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthImageViewInfo.image = m_depthImage;
    depthImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewInfo.format = m_depthFormat;
    depthImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewInfo.subresourceRange.levelCount = 1;
    depthImageViewInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(m_device, &depthImageViewInfo, nullptr, &m_depthImageView), "Failed to create viewport render target depth image view.");

    const VkImageView attachments[] = {m_imageView, m_depthImageView};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_size.x;
    framebufferInfo.height = m_size.y;
    framebufferInfo.layers = 1;
    checkVk(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer), "Failed to create viewport render target framebuffer.");
}

void VulkanViewportRenderTarget::destroy()
{
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }
}

void VulkanViewportRenderTarget::destroyRenderPass()
{
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

VkFormat VulkanViewportRenderTarget::findDepthFormat() const
{
    constexpr std::array<VkFormat, 2> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };

    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find viewport render target depth format.");
}

std::uint32_t VulkanViewportRenderTarget::findMemoryType(
    const std::uint32_t typeFilter,
    const VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0 &&
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }

    throw std::runtime_error("Failed to find viewport render target memory type.");
}

} // namespace Engine
