#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <vector>

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
    void clearTransientInput();
    

    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] bool isKeyPressed(int key) const;
    [[nodiscard]] bool isMouseButtonPressed(int button) const;
    [[nodiscard]] const std::vector<char32_t>& typedCharacters() const;
    [[nodiscard]] const std::vector<int>& pressedKeys() const;
    [[nodiscard]] std::string clipboardText() const;
    void setClipboardText(std::string_view text) const;
    [[nodiscard]] double scrollX() const;
    [[nodiscard]] double scrollY() const;
    [[nodiscard]] double mouseX() const;
    [[nodiscard]] double mouseY() const;
    void setMousePosition(double x, double y) const;
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
    std::vector<char32_t> m_typedCharacters;
    std::vector<int> m_pressedKeys;
    double m_scrollX{0.0};
    double m_scrollY{0.0};
};

} // namespace Engine
