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

} // namespace Engine
