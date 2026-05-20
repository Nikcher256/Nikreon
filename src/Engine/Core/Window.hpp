#pragma once

#include <cstdint>
#include <string>
#include <functional>

struct GLFWwindow;

namespace Engine {

struct WindowProps {
    std::string title{"Nikreon Engine"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
};

class Window {
public:
    explicit Window(const WindowProps& props = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    void pollEvents() const;
    void requestClose();
    void toggleFullscreen();
    using RefreshCallback = std::function<void()>;
    void setRefreshCallback(RefreshCallback callback);
    [[nodiscard]] bool consumeFramebufferResized();
    

    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] bool isKeyPressed(int key) const;
    [[nodiscard]] bool isFullscreen() const;
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;
    [[nodiscard]] GLFWwindow* nativeHandle() const;

private:
    void create(const WindowProps& props);
    void destroy();

    GLFWwindow* m_handle{nullptr};
    std::uint32_t m_width{0};
    std::uint32_t m_height{0};
    int m_windowedX{100};
    int m_windowedY{100};
    std::uint32_t m_windowedWidth{1280};
    std::uint32_t m_windowedHeight{720};
    bool m_fullscreen{false};
    bool m_framebufferResized{false};
    RefreshCallback m_refreshCallback;
};

} // namespace Engine
