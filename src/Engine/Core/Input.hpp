#pragma once

#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace Engine {

class Window;

class Input {
public:
    explicit Input(const Window& window);

    [[nodiscard]] bool isKeyPressed(int key) const;
    [[nodiscard]] bool isMouseButtonPressed(int button) const;
    [[nodiscard]] glm::vec2 mousePosition() const;
    [[nodiscard]] glm::vec2 scrollDelta() const;
    [[nodiscard]] const std::vector<char32_t>& typedCharacters() const;
    [[nodiscard]] const std::vector<int>& pressedKeys() const;
    [[nodiscard]] std::string clipboardText() const;
    void setClipboardText(std::string_view text) const;

private:
    const Window& m_window;
};

} // namespace Engine
