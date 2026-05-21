#pragma once

#include <glm/vec2.hpp>

namespace Engine {

class Window;

class Input {
public:
    explicit Input(const Window& window);

    [[nodiscard]] bool isKeyPressed(int key) const;
    [[nodiscard]] bool isMouseButtonPressed(int button) const;
    [[nodiscard]] glm::vec2 mousePosition() const;

private:
    const Window& m_window;
};

} // namespace Engine
