#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <volk.h>

namespace Engine {

enum class EditorViewportMode;
struct EditorViewportPresentation;
class VulkanRenderer2D;
class VulkanTextRenderer;
class VulkanViewportRenderTarget;
class RenderPipeline;
struct RenderFrameContext;
class Window;

class VulkanContext {
public:
    enum class FrameResult {
        Rendered,
        RecreateSwapchain,
    };

    explicit VulkanContext(Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    FrameResult drawFrame(
        VulkanRenderer2D& renderer2D,
        VulkanRenderer2D& viewportRenderer2D,
        VulkanTextRenderer& textRenderer,
        RenderPipeline& renderPipeline,
        const RenderFrameContext& frameContext);
    void waitIdle() const;
    void recreateSwapchain();
    void setEditorViewport(const EditorViewportPresentation& presentation, EditorViewportMode mode);

    [[nodiscard]] bool isDrawable() const;
    [[nodiscard]] bool shouldRecreateSwapchain();
    [[nodiscard]] VkDevice device() const;
    [[nodiscard]] VkPhysicalDevice physicalDevice() const;
    [[nodiscard]] VkRenderPass renderPass() const;
    [[nodiscard]] VkRenderPass viewportRenderPass() const;
    [[nodiscard]] VkQueue graphicsQueue() const;
    [[nodiscard]] VkCommandPool commandPool() const;
    [[nodiscard]] glm::uvec2 swapchainSize() const;
    [[nodiscard]] glm::uvec2 viewportRenderTargetSize() const;

private:
    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily{UINT32_MAX};
        std::uint32_t presentFamily{UINT32_MAX};

        [[nodiscard]] bool complete() const;
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void initialize();
    void shutdown();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createRenderFinishedSemaphores();
    void destroyRenderFinishedSemaphores();
    void createViewportRenderTarget();
    void prepareViewportRenderTarget();

    void cleanupSwapchain();

    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        std::uint32_t imageIndex,
        VulkanRenderer2D& renderer2D,
        VulkanRenderer2D& viewportRenderer2D,
        VulkanTextRenderer& textRenderer,
        RenderPipeline& renderPipeline,
        const RenderFrameContext& frameContext);

    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    [[nodiscard]] bool validationLayersAvailable() const;
    [[nodiscard]] std::vector<const char*> requiredInstanceExtensions() const;

public:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData);

private:
    Window& m_window;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchainImageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_swapchainFramebuffers;
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::unique_ptr<VulkanViewportRenderTarget> m_viewportRenderTarget;
    glm::vec2 m_editorViewportPosition{0.0f, 0.0f};
    glm::vec2 m_editorViewportSize{1.0f, 1.0f};
    glm::vec4 m_editorViewportClearColor{0.055f, 0.085f, 0.14f, 1.0f};
    EditorViewportMode m_editorViewportMode;
    std::uint32_t m_currentFrame{0};
};

} // namespace Engine
