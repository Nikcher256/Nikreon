#include "Engine/Renderer/Vulkan/VulkanContext.hpp"

#include "Engine/Core/Window.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Renderer/RenderPipeline.hpp"
#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"
#include "Engine/Renderer/Vulkan/VulkanTextRenderer.hpp"
#include "Engine/Renderer/Vulkan/VulkanViewportRenderTarget.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

namespace {

constexpr std::uint32_t FramesInFlight = 2;

const std::array<const char*, 1> ValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::array<const char*, 1> DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#if defined(NDEBUG)
constexpr bool EnableValidationLayers = false;
#else
constexpr bool EnableValidationLayers = true;
#endif

void checkVk(const VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

class VulkanRenderCommandRecorder final : public RenderCommandRecorder {
public:
    explicit VulkanRenderCommandRecorder(const VkCommandBuffer commandBuffer)
        : m_commandBuffer(commandBuffer)
    {
    }

    void beginStage(RenderStage stage) override
    {
        (void)stage;
        (void)m_commandBuffer;
    }

    void endStage(RenderStage stage) override
    {
        (void)stage;
        (void)m_commandBuffer;
    }

private:
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
};

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = VulkanContext::debugCallback;
}

} // namespace

bool VulkanContext::QueueFamilyIndices::complete() const
{
    return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
}

VulkanContext::VulkanContext(Window& window)
    : m_window(window)
    , m_editorViewportMode(EditorViewportMode::Edit)
{
    initialize();
}

VulkanContext::~VulkanContext()
{
    shutdown();
}

VulkanContext::FrameResult VulkanContext::drawFrame(
    VulkanRenderer2D& renderer2D,
    VulkanRenderer2D& viewportRenderer2D,
    VulkanTextRenderer& textRenderer,
    RenderPipeline& renderPipeline,
    const RenderFrameContext& frameContext)
{
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return FrameResult::RecreateSwapchain;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    prepareViewportRenderTarget();
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex, renderer2D, viewportRenderer2D, textRenderer, renderPipeline, frameContext);

    const VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[imageIndex]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    checkVk(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit draw command buffer.");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return FrameResult::RecreateSwapchain;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    m_currentFrame = (m_currentFrame + 1) % FramesInFlight;
    return FrameResult::Rendered;
}

void VulkanContext::waitIdle() const
{
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

void VulkanContext::setEditorViewport(const EditorViewportPresentation& presentation, const EditorViewportMode mode)
{
    m_editorViewportPosition = presentation.position;
    m_editorViewportSize = presentation.size;
    m_editorViewportClearColor = presentation.clearColor;
    m_editorViewportMode = mode;
}

bool VulkanContext::isDrawable() const
{
    return m_window.width() > 0 && m_window.height() > 0;
}

bool VulkanContext::shouldRecreateSwapchain()
{
    const bool framebufferResized = m_window.consumeFramebufferResized();
    const bool extentChanged =
        m_window.width() != m_swapchainExtent.width ||
        m_window.height() != m_swapchainExtent.height;

    return isDrawable() && (framebufferResized || extentChanged);
}

VkDevice VulkanContext::device() const
{
    return m_device;
}

VkPhysicalDevice VulkanContext::physicalDevice() const
{
    return m_physicalDevice;
}

VkRenderPass VulkanContext::renderPass() const
{
    return m_renderPass;
}

VkRenderPass VulkanContext::viewportRenderPass() const
{
    return m_viewportRenderTarget->renderPass();
}

VkQueue VulkanContext::graphicsQueue() const
{
    return m_graphicsQueue;
}

VkCommandPool VulkanContext::commandPool() const
{
    return m_commandPool;
}

glm::uvec2 VulkanContext::swapchainSize() const
{
    return {m_swapchainExtent.width, m_swapchainExtent.height};
}

glm::uvec2 VulkanContext::viewportRenderTargetSize() const
{
    return m_viewportRenderTarget ? m_viewportRenderTarget->size() : glm::uvec2{1U, 1U};
}

void VulkanContext::initialize()
{
    checkVk(volkInitialize(), "Failed to initialize volk.");

    if (glfwVulkanSupported() != GLFW_TRUE) {
        throw std::runtime_error("Vulkan is not supported by the current system or driver.");
    }

    createInstance();
    volkLoadInstance(m_instance);
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    volkLoadDevice(m_device);
    createSwapchain();
    createViewportRenderTarget();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();

    spdlog::info("Vulkan context initialized.");
}

void VulkanContext::shutdown()
{
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    cleanupSwapchain();
    m_viewportRenderTarget.reset();

    for (std::size_t index = 0; index < m_imageAvailableSemaphores.size(); ++index) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[index], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[index], nullptr);
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }

    if (m_debugMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }

    spdlog::info("Vulkan context shutdown.");
}

void VulkanContext::createInstance()
{
    const bool useValidation = EnableValidationLayers && validationLayersAvailable();
    if (EnableValidationLayers && !useValidation) {
        spdlog::warn("Validation layers requested but unavailable; continuing without them.");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Nikreon Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Nikreon";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    const auto extensions = requiredInstanceExtensions();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (useValidation) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(ValidationLayers.size());
        createInfo.ppEnabledLayerNames = ValidationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    checkVk(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create Vulkan instance.");
}

void VulkanContext::setupDebugMessenger()
{
    if (!EnableValidationLayers || !validationLayersAvailable()) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    checkVk(vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger), "Failed to create Vulkan debug messenger.");
}

void VulkanContext::createSurface()
{
    checkVk(glfwCreateWindowSurface(m_instance, m_window.nativeHandle(), nullptr, &m_surface), "Failed to create window surface.");
}

void VulkanContext::pickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable Vulkan physical device found.");
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    spdlog::info("Selected Vulkan device: {}", properties.deviceName);
}

void VulkanContext::createLogicalDevice()
{
    const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    const std::set<std::uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily,
        indices.presentFamily,
    };

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (const std::uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

    checkVk(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "Failed to create logical device.");

    vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily, 0, &m_presentQueue);
}

void VulkanContext::createSwapchain()
{
    const SwapchainSupportDetails support = querySwapchainSupport(m_physicalDevice);
    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    const VkExtent2D extent = chooseSwapExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((support.capabilities.supportedUsageFlags & imageUsage) != imageUsage) {
        throw std::runtime_error("Swapchain does not support the image usages required by the minimal Vulkan frame.");
    }

    createInfo.imageUsage = imageUsage;

    const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    const std::uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    checkVk(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "Failed to create swapchain.");

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    spdlog::info("Swapchain created: {} images ({}x{})", imageCount, extent.width, extent.height);
}

void VulkanContext::createImageViews()
{
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (std::size_t index = 0; index < m_swapchainImages.size(); ++index) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapchainImages[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapchainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        checkVk(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[index]), "Failed to create swapchain image view.");
    }
}

void VulkanContext::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    checkVk(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "Failed to create render pass.");
}

void VulkanContext::createFramebuffers()
{
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());

    for (std::size_t index = 0; index < m_swapchainImageViews.size(); ++index) {
        const VkImageView attachments[] = {m_swapchainImageViews[index]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapchainExtent.width;
        framebufferInfo.height = m_swapchainExtent.height;
        framebufferInfo.layers = 1;

        checkVk(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapchainFramebuffers[index]), "Failed to create framebuffer.");
    }
}

void VulkanContext::createCommandPool()
{
    const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily;

    checkVk(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "Failed to create command pool.");
}

void VulkanContext::createCommandBuffers()
{
    m_commandBuffers.resize(FramesInFlight);

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(m_commandBuffers.size());

    checkVk(vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers.data()), "Failed to allocate command buffers.");
}

void VulkanContext::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(FramesInFlight);
    m_inFlightFences.resize(FramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t index = 0; index < FramesInFlight; ++index) {
        checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[index]), "Failed to create image-available semaphore.");
        checkVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[index]), "Failed to create in-flight fence.");
    }

    createRenderFinishedSemaphores();
}

void VulkanContext::createRenderFinishedSemaphores()
{
    m_renderFinishedSemaphores.resize(m_swapchainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (std::size_t index = 0; index < m_renderFinishedSemaphores.size(); ++index) {
        checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[index]), "Failed to create render-finished semaphore.");
    }
}

void VulkanContext::destroyRenderFinishedSemaphores()
{
    for (const auto semaphore : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }
    m_renderFinishedSemaphores.clear();
}

void VulkanContext::createViewportRenderTarget()
{
    m_viewportRenderTarget = std::make_unique<VulkanViewportRenderTarget>(m_device, m_physicalDevice, m_swapchainImageFormat);
}

void VulkanContext::prepareViewportRenderTarget()
{
    const std::uint32_t availableWidth = m_swapchainExtent.width;
    const std::uint32_t availableHeight = m_swapchainExtent.height;
    const std::uint32_t x = std::min(static_cast<std::uint32_t>(std::max(m_editorViewportPosition.x, 0.0f)), availableWidth);
    const std::uint32_t y = std::min(static_cast<std::uint32_t>(std::max(m_editorViewportPosition.y, 0.0f)), availableHeight);
    const glm::uvec2 size{
        std::max(std::min(static_cast<std::uint32_t>(std::max(m_editorViewportSize.x, 1.0f)), availableWidth - x), 1U),
        std::max(std::min(static_cast<std::uint32_t>(std::max(m_editorViewportSize.y, 1.0f)), availableHeight - y), 1U),
    };

    if (m_viewportRenderTarget->size() != size) {
        vkDeviceWaitIdle(m_device);
        m_viewportRenderTarget->resize(size);
    }
}

void VulkanContext::cleanupSwapchain()
{
    for (const auto framebuffer : m_swapchainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_swapchainFramebuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    destroyRenderFinishedSemaphores();

    for (const auto imageView : m_swapchainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void VulkanContext::recreateSwapchain()
{
    while (m_window.width() == 0 || m_window.height() == 0) {
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);
    m_viewportRenderTarget.reset();
    cleanupSwapchain();
    createSwapchain();
    createViewportRenderTarget();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createRenderFinishedSemaphores();
}

void VulkanContext::recordCommandBuffer(
    VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    VulkanRenderer2D& renderer2D,
    VulkanRenderer2D& viewportRenderer2D,
    VulkanTextRenderer& textRenderer,
    RenderPipeline& renderPipeline,
    const RenderFrameContext& frameContext)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer.");

    VulkanRenderCommandRecorder renderCommandRecorder(commandBuffer);
    renderPipeline.recordCommands(renderCommandRecorder, frameContext);

    m_viewportRenderTarget->recordClear(commandBuffer, m_editorViewportClearColor);
    m_viewportRenderTarget->recordBeginRenderPass(commandBuffer);
    viewportRenderer2D.record(commandBuffer);
    m_viewportRenderTarget->recordEndRenderPass(commandBuffer);
    m_viewportRenderTarget->recordCopyTo(
        commandBuffer,
        m_swapchainImages[imageIndex],
        {
            static_cast<int>(std::max(m_editorViewportPosition.x, 0.0f)),
            static_cast<int>(std::max(m_editorViewportPosition.y, 0.0f)),
        },
        m_viewportRenderTarget->size());

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    renderer2D.record(commandBuffer);
    textRenderer.record(commandBuffer);
    vkCmdEndRenderPass(commandBuffer);

    checkVk(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index) {
        if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphicsFamily = index;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, m_surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = index;
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const
{
    const QueueFamilyIndices indices = findQueueFamilies(device);
    const bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapchainAdequate = false;

    if (extensionsSupported) {
        const SwapchainSupportDetails support = querySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.complete() && extensionsSupported && swapchainAdequate;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const
{
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VulkanContext::SwapchainSupportDetails VulkanContext::querySwapchainSupport(VkPhysicalDevice device) const
{
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return formats.front();
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const
{
    for (const auto availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent{m_window.width(), m_window.height()};
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

bool VulkanContext::validationLayersAvailable() const
{
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : ValidationLayers) {
        const auto found = std::find_if(availableLayers.begin(), availableLayers.end(), [layerName](const VkLayerProperties& properties) {
            return std::strcmp(layerName, properties.layerName) == 0;
        });

        if (found == availableLayers.end()) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanContext::requiredInstanceExtensions() const
{
    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr || glfwExtensionCount == 0) {
        throw std::runtime_error("Failed to get required GLFW Vulkan instance extensions.");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (EnableValidationLayers && validationLayersAvailable()) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData)
{
    (void)type;
    (void)userData;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("Vulkan validation: {}", callbackData->pMessage);
    } else {
        spdlog::warn("Vulkan validation: {}", callbackData->pMessage);
    }

    return VK_FALSE;
}

} // namespace Engine
