#pragma once

namespace Engine {

class Window;

class Input {
public:
    explicit Input(const Window& window);

    [[nodiscard]] bool isKeyPressed(int key) const;

private:
    const Window& m_window;
};

} // namespace Engine
