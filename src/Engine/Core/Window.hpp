#pragma once

#include <cstdint>
#include <string>

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

    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] bool isKeyPressed(int key) const;
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;
    [[nodiscard]] GLFWwindow* nativeHandle() const;

private:
    void create(const WindowProps& props);
    void destroy();

    GLFWwindow* m_handle{nullptr};
    std::uint32_t m_width{0};
    std::uint32_t m_height{0};
};

} // namespace Engine
