#pragma once

#include "Engine/Core/Input.hpp"
#include "Engine/Core/LayerStack.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Renderer/Renderer.hpp"

namespace Engine {

struct RunOptions {
    unsigned int maxFrames{0};
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run(const RunOptions& options = {});

private:
    void update(float deltaTime);
    void render();

    Window m_window;
    Input m_input;
    Time m_time;
    Renderer m_renderer;
    LayerStack m_layers;
    unsigned int m_frameCount{0};
    bool m_fullscreenToggleWasPressed{false};
    bool m_renderingFrame = false;
};

} // namespace Engine
