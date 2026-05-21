#include "Engine/Editor/UIContext.hpp"

#include "Engine/Core/Input.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>

namespace Engine {

// Captures mouse input once so every widget sees a consistent frame state.
void UIContext::beginFrame(const Input& input)
{
    m_hotId.clear();
    m_mousePosition = input.mousePosition();
    m_previousMouseDown = m_mouseDown;
    m_mouseDown = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    m_mousePressed = m_mouseDown && !m_previousMouseDown;
    m_mouseReleased = !m_mouseDown && m_previousMouseDown;
}

// Clears active capture after release so the next frame can pick a new widget.
void UIContext::endFrame()
{
    if (m_mouseReleased) {
        m_activeId.clear();
    }
}

// Computes hover, hold, and click state for a rectangular widget.
UIInteraction UIContext::interact(const std::string_view id, const glm::vec2& position, const glm::vec2& size)
{
    const std::string widgetId{id};
    const bool hovered = contains(position, size);
    if (hovered) {
        m_hotId = widgetId;
    }

    if (hovered && m_mousePressed) {
        m_activeId = widgetId;
    }

    const bool held = m_mouseDown && m_activeId == widgetId;
    const bool pressed = m_mouseReleased && hovered && m_activeId == widgetId;
    return {hovered, held, pressed};
}

// Returns true on the frame a button is clicked.
bool UIContext::button(const std::string_view id, const glm::vec2& position, const glm::vec2& size)
{
    return interact(id, position, size).pressed;
}

// Toggles a boolean value when the checkbox is clicked.
bool UIContext::checkbox(const std::string_view id, const glm::vec2& position, const glm::vec2& size, bool& value)
{
    if (!button(id, position, size)) {
        return false;
    }

    value = !value;
    return true;
}

// Updates a normalized value while dragging inside the slider track.
bool UIContext::slider(
    const std::string_view id,
    const glm::vec2& position,
    const glm::vec2& size,
    float& value,
    const float minValue,
    const float maxValue)
{
    const UIInteraction state = interact(id, position, size);
    if (!state.held || maxValue <= minValue) {
        return false;
    }

    const float t = std::clamp((m_mousePosition.x - position.x) / std::max(size.x, 1.0f), 0.0f, 1.0f);
    value = minValue + (maxValue - minValue) * t;
    return true;
}

// Returns the mouse position captured for the current UI frame.
glm::vec2 UIContext::mousePosition() const
{
    return m_mousePosition;
}

// Reports whether a widget is currently under the mouse.
bool UIContext::isHot(const std::string_view id) const
{
    return m_hotId == id;
}

// Reports whether a widget currently owns the mouse drag/click.
bool UIContext::isActive(const std::string_view id) const
{
    return m_activeId == id;
}

// Tests whether the captured mouse position lies inside a rectangle.
bool UIContext::contains(const glm::vec2& position, const glm::vec2& size) const
{
    return m_mousePosition.x >= position.x &&
        m_mousePosition.y >= position.y &&
        m_mousePosition.x <= position.x + size.x &&
        m_mousePosition.y <= position.y + size.y;
}

} // namespace Engine
