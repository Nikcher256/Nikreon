#include <cstdint>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

int main()
{
    if (glfwInit() != GLFW_TRUE) {
        spdlog::error("Failed to initialize GLFW.");
        return 1;
    }

    if (glfwVulkanSupported() != GLFW_TRUE) {
        spdlog::error("Vulkan is not supported by the current system or driver.");
        glfwTerminate();
        return 1;
    }

    const std::uint32_t version = VK_HEADER_VERSION;
    const glm::vec3 forward{0.0f, 0.0f, -1.0f};

    spdlog::info("Nikreon Engine bootstrap OK. Vulkan header version: {}", version);
    spdlog::info("Default forward vector: ({}, {}, {})", forward.x, forward.y, forward.z);

    glfwTerminate();
    return 0;
}
