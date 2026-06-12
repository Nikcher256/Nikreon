#include "Engine/Core/Input.hpp"

#include "Engine/Core/Window.hpp"

namespace Engine {

Input::Input(const Window& window)
    : m_window(window)
{
}

// Reads whether a keyboard key is currently pressed.
bool Input::isKeyPressed(const int key) const
{
    return m_window.isKeyPressed(key);
}

// Reads whether a mouse button is currently pressed.
bool Input::isMouseButtonPressed(const int button) const
{
    return m_window.isMouseButtonPressed(button);
}

// Returns the current mouse position in window coordinates.
glm::vec2 Input::mousePosition() const
{
    return {
        static_cast<float>(m_window.mouseX()),
        static_cast<float>(m_window.mouseY()),
    };
}

void Input::setMousePosition(const glm::vec2& position) const
{
    m_window.setMousePosition(position.x, position.y);
}

glm::vec2 Input::scrollDelta() const
{
    return {
        static_cast<float>(m_window.scrollX()),
        static_cast<float>(m_window.scrollY()),
    };
}

const std::vector<char32_t>& Input::typedCharacters() const
{
    return m_window.typedCharacters();
}

const std::vector<int>& Input::pressedKeys() const
{
    return m_window.pressedKeys();
}

std::string Input::clipboardText() const
{
    return m_window.clipboardText();
}

void Input::setClipboardText(const std::string_view text) const
{
    m_window.setClipboardText(text);
}

void Input::setCursorShape(const CursorShape shape) const
{
    m_window.setCursorShape(shape);
}

void Input::setCursorVisible(const bool visible) const
{
    m_window.setCursorVisible(visible);
}

} // namespace Engine
