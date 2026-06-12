#include "Engine/Core/Window.hpp"

#include <stdexcept>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

Window::Window(const WindowProps& props)
{
    create(props);
}

Window::~Window()
{
    destroy();
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

// Requests that GLFW close the window on the next loop check.
void Window::requestClose()
{
    if (m_handle != nullptr) {
        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }
}

// Switches between normal windowed mode and fullscreen primary-monitor mode.
void Window::toggleFullscreen()
{
    if (m_handle == nullptr) {
        return;
    }

    if (!m_fullscreen) {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        glfwGetWindowPos(m_handle, &x, &y);
        glfwGetWindowSize(m_handle, &width, &height);

        m_windowedX = x;
        m_windowedY = y;
        m_windowedWidth = width > 0 ? static_cast<std::uint32_t>(width) : m_width;
        m_windowedHeight = height > 0 ? static_cast<std::uint32_t>(height) : m_height;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        if (monitor == nullptr || mode == nullptr) {
            return;
        }

        glfwSetWindowMonitor(m_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
        spdlog::info("Window switched to fullscreen ({}x{}).", mode->width, mode->height);
    } else {
        glfwSetWindowMonitor(
            m_handle,
            nullptr,
            m_windowedX,
            m_windowedY,
            static_cast<int>(m_windowedWidth),
            static_cast<int>(m_windowedHeight),
            0);
        m_fullscreen = false;
        spdlog::info("Window restored to windowed mode ({}x{}).", m_windowedWidth, m_windowedHeight);
    }

    m_framebufferResized = true;
}

// Returns and clears the resize flag raised by GLFW callbacks.
bool Window::consumeFramebufferResized()
{
    const bool resized = m_framebufferResized;
    m_framebufferResized = false;
    return resized;
}

void Window::clearTransientInput()
{
    m_typedCharacters.clear();
    m_pressedKeys.clear();
    m_scrollX = 0.0;
    m_scrollY = 0.0;
}

// Installs a callback used by GLFW when the OS asks the window to refresh.
void Window::setRefreshCallback(RefreshCallback callback)
{
    m_refreshCallback = std::move(callback);
}

// Reports whether the window should shut down.
bool Window::shouldClose() const
{
    return m_handle == nullptr || glfwWindowShouldClose(m_handle) == GLFW_TRUE;
}

// Reads the current keyboard state for a GLFW key code.
bool Window::isKeyPressed(const int key) const
{
    return m_handle != nullptr && glfwGetKey(m_handle, key) == GLFW_PRESS;
}

// Reads the current mouse state for a GLFW mouse button code.
bool Window::isMouseButtonPressed(const int button) const
{
    return m_handle != nullptr && glfwGetMouseButton(m_handle, button) == GLFW_PRESS;
}

const std::vector<char32_t>& Window::typedCharacters() const
{
    return m_typedCharacters;
}

const std::vector<int>& Window::pressedKeys() const
{
    return m_pressedKeys;
}

std::string Window::clipboardText() const
{
    const char* text = m_handle != nullptr ? glfwGetClipboardString(m_handle) : nullptr;
    return text != nullptr ? text : "";
}

void Window::setClipboardText(const std::string_view text) const
{
    if (m_handle != nullptr) {
        glfwSetClipboardString(m_handle, std::string{text}.c_str());
    }
}

double Window::scrollX() const
{
    return m_scrollX;
}

double Window::scrollY() const
{
    return m_scrollY;
}

// Returns the current cursor x coordinate scaled into framebuffer space.
double Window::mouseX() const
{
    double x = 0.0;
    double y = 0.0;
    if (m_handle != nullptr) {
        glfwGetCursorPos(m_handle, &x, &y);
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(m_handle, &windowWidth, &windowHeight);
        if (windowWidth > 0) {
            x *= static_cast<double>(m_width) / static_cast<double>(windowWidth);
        }
    }
    return x;
}

// Returns the current cursor y coordinate scaled into framebuffer space.
double Window::mouseY() const
{
    double x = 0.0;
    double y = 0.0;
    if (m_handle != nullptr) {
        glfwGetCursorPos(m_handle, &x, &y);
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(m_handle, &windowWidth, &windowHeight);
        if (windowHeight > 0) {
            y *= static_cast<double>(m_height) / static_cast<double>(windowHeight);
        }
    }
    return y;
}

void Window::setMousePosition(const double x, const double y) const
{
    if (m_handle == nullptr || m_width == 0U || m_height == 0U) {
        return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(m_handle, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    const double windowX = x * static_cast<double>(windowWidth) / static_cast<double>(m_width);
    const double windowY = y * static_cast<double>(windowHeight) / static_cast<double>(m_height);
    glfwSetCursorPos(m_handle, windowX, windowY);
}

void Window::setCursorShape(const CursorShape shape) const
{
    if (m_handle == nullptr || shape == m_currentCursorShape) {
        return;
    }

    glfwSetCursor(m_handle, cursorForShape(shape));
    m_currentCursorShape = shape;
}

void Window::setCursorVisible(const bool visible) const
{
    if (m_handle == nullptr || visible == m_cursorVisible) {
        return;
    }

    glfwSetInputMode(m_handle, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    m_cursorVisible = visible;
}

// Reports whether this window is currently fullscreen.
bool Window::isFullscreen() const
{
    return m_fullscreen;
}

std::uint32_t Window::width() const
{
    return m_width;
}

std::uint32_t Window::height() const
{
    return m_height;
}

GLFWwindow* Window::nativeHandle() const
{
    return m_handle;
}

void Window::create(const WindowProps& props)
{
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_width = props.width;
    m_height = props.height;
    m_handle = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        props.title.c_str(),
        nullptr,
        nullptr);

    if (m_handle == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, [](GLFWwindow* window, const int width, const int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        self->m_width = width > 0 ? static_cast<std::uint32_t>(width) : 0U;
        self->m_height = height > 0 ? static_cast<std::uint32_t>(height) : 0U;
        self->m_framebufferResized = true;
    });

    glfwSetCharCallback(m_handle, [](GLFWwindow* window, const unsigned int codepoint) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr) {
            self->m_typedCharacters.push_back(static_cast<char32_t>(codepoint));
        }
    });

    glfwSetKeyCallback(m_handle, [](GLFWwindow* window, const int key, const int, const int action, const int) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            self->m_pressedKeys.push_back(key);
        }
    });

    glfwSetScrollCallback(m_handle, [](GLFWwindow* window, const double xOffset, const double yOffset) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr) {
            self->m_scrollX += xOffset;
            self->m_scrollY += yOffset;
        }
    });

    glfwSetWindowRefreshCallback(m_handle, [](GLFWwindow* window) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        if (self->m_refreshCallback) {
            self->m_refreshCallback();
        }
    });

    m_arrowCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    m_textCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    m_resizeHorizontalCursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);

    spdlog::info("Window created: '{}' ({}x{})", props.title, m_width, m_height);
}

void Window::destroy()
{
    if (m_arrowCursor != nullptr) {
        glfwDestroyCursor(m_arrowCursor);
        m_arrowCursor = nullptr;
    }
    if (m_textCursor != nullptr) {
        glfwDestroyCursor(m_textCursor);
        m_textCursor = nullptr;
    }
    if (m_resizeHorizontalCursor != nullptr) {
        glfwDestroyCursor(m_resizeHorizontalCursor);
        m_resizeHorizontalCursor = nullptr;
    }

    if (m_handle != nullptr) {
        glfwDestroyWindow(m_handle);
        m_handle = nullptr;
        spdlog::info("Window destroyed.");
    }

    glfwTerminate();
}

GLFWcursor* Window::cursorForShape(const CursorShape shape) const
{
    switch (shape) {
    case CursorShape::Text:
        return m_textCursor;
    case CursorShape::ResizeHorizontal:
        return m_resizeHorizontalCursor;
    case CursorShape::Arrow:
        return m_arrowCursor;
    }

    return m_arrowCursor;
}

} // namespace Engine
