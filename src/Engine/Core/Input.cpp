#include "Engine/Core/Input.hpp"

#include "Engine/Core/Window.hpp"

namespace Engine {

Input::Input(const Window& window)
    : m_window(window)
{
}

bool Input::isKeyPressed(const int key) const
{
    return m_window.isKeyPressed(key);
}

} // namespace Engine
