#pragma once

#include <string>
#include <string_view>

#include <glm/vec2.hpp>

namespace Engine {

class Input;

struct UIInteraction {
    bool hovered{false};
    bool held{false};
    bool pressed{false};
};

class UIContext {
public:
    void beginFrame(const Input& input);
    void endFrame();

    [[nodiscard]] UIInteraction interact(std::string_view id, const glm::vec2& position, const glm::vec2& size);
    [[nodiscard]] bool button(std::string_view id, const glm::vec2& position, const glm::vec2& size);
    bool checkbox(std::string_view id, const glm::vec2& position, const glm::vec2& size, bool& value);
    bool slider(std::string_view id, const glm::vec2& position, const glm::vec2& size, float& value, float minValue, float maxValue);

    [[nodiscard]] glm::vec2 mousePosition() const;
    [[nodiscard]] bool isHot(std::string_view id) const;
    [[nodiscard]] bool isActive(std::string_view id) const;

private:
    [[nodiscard]] bool contains(const glm::vec2& position, const glm::vec2& size) const;

    glm::vec2 m_mousePosition{0.0f, 0.0f};
    std::string m_hotId;
    std::string m_activeId;
    bool m_mouseDown{false};
    bool m_previousMouseDown{false};
    bool m_mousePressed{false};
    bool m_mouseReleased{false};
};

} // namespace Engine
